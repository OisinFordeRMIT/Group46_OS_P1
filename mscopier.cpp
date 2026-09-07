/*
 * COSC1114 Project 1, Task 2 - mscopier
 * Group 46
 *
 * ./mscopier n source_file destination_file
 *
 * One file, copied through a shared queue. n reader threads pull lines out
 * of the source and drop them in the queue, n writer threads take them back
 * out and put them in the destination. Both teams run at the same time.
 *
 * Subtask 1 is the queue, which holds 20 lines at most.
 * Subtask 2 is the mutex that guards it.
 * Subtask 3 is the two condition variables, so a thread with nothing to do
 * goes to sleep instead of spinning on the CPU.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <pthread.h>

using namespace std;

// The spec caps the queue at 20 items and n at 2 to 10.
const int QUEUE_SIZE = 20;
const int MIN_THREADS = 2;
const int MAX_THREADS = 10;

/*
 * The shared queue. A plain array used as a ring: head is where the next
 * line comes out, tail is where the next one goes in, and count says how
 * many are sitting in there. Wrapping with % is what makes it a ring, so
 * nothing ever has to shuffle along.
 *
 * Every variable below this comment is shared, and none of it gets touched
 * without holding queueLock.
 */
static string queueData[QUEUE_SIZE];
static int queueHead = 0;
static int queueTail = 0;
static int queueCount = 0;

static ifstream sourceFile;
static ofstream destFile;

static int readersLeft = 0;      // readers that have not hit the end yet
static bool sourceDrained = false;   // true once readersLeft reaches 0
static bool giveUp = false;      // something failed, everyone stop

static pthread_mutex_t queueLock;
static pthread_cond_t hasRoom;   // a writer freed a slot
static pthread_cond_t hasWork;   // a reader added a line

/*
 * Reader thread. Takes a turn at the source file, puts what it read on the
 * queue, then nudges a writer.
 *
 * The getline happens with the lock held. All the readers share one stream,
 * so taking turns is the only way it works, and it also keeps "read a line"
 * and "add that line" as one step. Split them and two readers could swap
 * order on the way to the queue, which would scramble the copy.
 */
void *reader(void *param) {
    (void) param;

    while (true) {
        pthread_mutex_lock(&queueLock);

        // Queue is full, so wait for a writer to take something out. This
        // is the part that used to be a busy wait. The while loop is not
        // an if: a condition variable can wake a thread for no reason, so
        // the test has to run again after every wake.
        while (queueCount == QUEUE_SIZE && !giveUp) {
            pthread_cond_wait(&hasRoom, &queueLock);
        }

        if (giveUp) {
            pthread_mutex_unlock(&queueLock);
            break;
        }

        string line;
        if (!getline(sourceFile, line)) {
            pthread_mutex_unlock(&queueLock);
            break;
        }

        // getline eats the newline it stopped on. Put it back, unless this
        // was the last line of a file that never had one on the end.
        if (!sourceFile.eof()) {
            line += "\n";
        }

        queueData[queueTail] = line;
        queueTail = (queueTail + 1) % QUEUE_SIZE;
        queueCount++;

        pthread_cond_signal(&hasWork);
        pthread_mutex_unlock(&queueLock);
    }

    // Say this reader is done. When the last one goes, the writers need
    // waking, or the ones sitting on an empty queue would wait forever.
    pthread_mutex_lock(&queueLock);
    readersLeft--;
    if (readersLeft == 0) {
        sourceDrained = true;
        pthread_cond_broadcast(&hasWork);
    }
    pthread_mutex_unlock(&queueLock);

    pthread_exit(0);
}

/*
 * Writer thread. Takes the oldest line off the queue, writes it, and tells
 * a reader there is room again.
 *
 * The write stays inside the lock on purpose. Lines have to reach the file
 * in the order they left the queue, and letting go of the lock first would
 * let two writers overtake each other.
 */
void *writer(void *param) {
    (void) param;

    while (true) {
        pthread_mutex_lock(&queueLock);

        // Nothing to write yet, so sleep until a reader adds something or
        // the readers finish. The other half of subtask 3.
        while (queueCount == 0 && !sourceDrained && !giveUp) {
            pthread_cond_wait(&hasWork, &queueLock);
        }

        // Quit when there is a problem, or when the queue is empty and no
        // more lines are coming.
        if (giveUp || (queueCount == 0 && sourceDrained)) {
            pthread_mutex_unlock(&queueLock);
            break;
        }

        string line = queueData[queueHead];
        queueHead = (queueHead + 1) % QUEUE_SIZE;
        queueCount--;

        destFile << line;
        if (!destFile.good()) {
            cerr << "mscopier: writing to the destination failed" << endl;
            giveUp = true;
            // Wake everyone so the program can stop rather than hang.
            pthread_cond_broadcast(&hasWork);
            pthread_cond_broadcast(&hasRoom);
            pthread_mutex_unlock(&queueLock);
            break;
        }

        pthread_cond_signal(&hasRoom);
        pthread_mutex_unlock(&queueLock);
    }

    pthread_exit(0);
}

/*
 * Sets up the lock and the two condition variables, cleaning up after
 * itself if one of them will not start.
 */
bool startSync() {
    if (pthread_mutex_init(&queueLock, NULL) != 0) {
        cerr << "mscopier: could not set up the mutex" << endl;
        return false;
    }
    if (pthread_cond_init(&hasRoom, NULL) != 0) {
        cerr << "mscopier: could not set up hasRoom" << endl;
        pthread_mutex_destroy(&queueLock);
        return false;
    }
    if (pthread_cond_init(&hasWork, NULL) != 0) {
        cerr << "mscopier: could not set up hasWork" << endl;
        pthread_cond_destroy(&hasRoom);
        pthread_mutex_destroy(&queueLock);
        return false;
    }
    return true;
}

void stopSync() {
    pthread_cond_destroy(&hasWork);
    pthread_cond_destroy(&hasRoom);
    pthread_mutex_destroy(&queueLock);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " n source_file destination_file" << endl;
        return 1;
    }

    // atoi cannot tell us the argument was rubbish, so check it first.
    string countArg = argv[1];
    if (countArg.empty() ||
        countArg.find_first_not_of("0123456789") != string::npos) {
        cerr << "mscopier: n must be a number" << endl;
        return 1;
    }

    // n is the size of each team, so n readers and n writers.
    int n = atoi(argv[1]);
    if (n < MIN_THREADS || n > MAX_THREADS) {
        cerr << "mscopier: n must be between " << MIN_THREADS
             << " and " << MAX_THREADS << endl;
        return 1;
    }

    // The paths go through exactly as typed. The spec says they can be any
    // path, so nothing gets added to the end of them.
    sourceFile.open(argv[2]);
    if (!sourceFile.is_open()) {
        cerr << "mscopier: cannot open " << argv[2]
             << " (" << strerror(errno) << ")" << endl;
        return 1;
    }

    destFile.open(argv[3]);
    if (!destFile.is_open()) {
        cerr << "mscopier: cannot open " << argv[3]
             << " (" << strerror(errno) << ")" << endl;
        sourceFile.close();
        return 1;
    }

    if (!startSync()) {
        sourceFile.close();
        destFile.close();
        return 1;
    }

    readersLeft = n;

    // n tops out at 10, so fixed arrays do the job and there is no heap to
    // clean up afterwards.
    pthread_t readers[MAX_THREADS];
    pthread_t writers[MAX_THREADS];
    int readersStarted = 0;
    int writersStarted = 0;

    for (int i = 0; i < n; i++) {
        int rc = pthread_create(&readers[i], NULL, reader, NULL);
        if (rc) {
            cerr << "mscopier: pthread_create failed with code " << rc << endl;
            break;
        }
        readersStarted++;

        rc = pthread_create(&writers[i], NULL, writer, NULL);
        if (rc) {
            cerr << "mscopier: pthread_create failed with code " << rc << endl;
            break;
        }
        writersStarted++;
    }

    // If some readers never started they will never count themselves out,
    // so do it here. Otherwise the writers would wait on a queue that
    // nobody is ever going to fill.
    if (readersStarted < n) {
        pthread_mutex_lock(&queueLock);
        readersLeft -= (n - readersStarted);
        giveUp = true;
        if (readersLeft <= 0) {
            sourceDrained = true;
        }
        pthread_cond_broadcast(&hasWork);
        pthread_cond_broadcast(&hasRoom);
        pthread_mutex_unlock(&queueLock);
    }

    // Readers first, so the writers get to see sourceDrained and empty the
    // queue before they are joined.
    bool joinFailed = false;
    for (int i = 0; i < readersStarted; i++) {
        if (pthread_join(readers[i], NULL) != 0) {
            cerr << "mscopier: pthread_join failed on reader " << i << endl;
            joinFailed = true;
        }
    }
    for (int i = 0; i < writersStarted; i++) {
        if (pthread_join(writers[i], NULL) != 0) {
            cerr << "mscopier: pthread_join failed on writer " << i << endl;
            joinFailed = true;
        }
    }

    stopSync();

    destFile.flush();
    bool wroteCleanly = destFile.good();
    sourceFile.close();
    destFile.close();

    if (joinFailed || giveUp || !wroteCleanly ||
        readersStarted < n || writersStarted < n) {
        cerr << "mscopier: the copy did not finish properly" << endl;
        return 1;
    }

    cout << "Copied " << argv[2] << " to " << argv[3]
         << " using " << n << " readers and " << n << " writers" << endl;
    return 0;
}
