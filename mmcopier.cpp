/*
 * COSC1114 Project 1, Task 1 - mmcopier
 * Group 46
 *
 * ./mmcopier n source_dir destination_dir
 *
 * Starts n threads. Thread i copies source<i>.txt out of source_dir into
 * destination_dir. The threads never touch the same data, so there is no
 * mutex anywhere in this file. Each one opens its own two files and works
 * on its own struct. That is the whole point of task 1.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <pthread.h>
#include <sys/stat.h>

using namespace std;

// The spec says 2 <= n <= 10, and n is also how many files we copy.
const int MIN_THREADS = 2;
const int MAX_THREADS = 10;

// What one thread gets handed. The thread writes its result into copied so
// main can check it afterwards, which means the thread never has to malloc
// anything to report back.
struct CopyJob {
    string sourceName;
    string destName;
    bool copied;
};

/*
 * Copies one file a line at a time, the same way we read files in the
 * week 5 lab. Returns false and prints why if anything goes wrong.
 */
bool copyOneFile(string sourceName, string destName) {
    ifstream source(sourceName.c_str());
    if (!source.is_open()) {
        cerr << "mmcopier: cannot open " << sourceName
             << " (" << strerror(errno) << ")" << endl;
        return false;
    }

    ofstream dest(destName.c_str());
    if (!dest.is_open()) {
        cerr << "mmcopier: cannot open " << destName
             << " (" << strerror(errno) << ")" << endl;
        source.close();
        return false;
    }

    string line;
    while (getline(source, line)) {
        dest << line;

        // getline throws away the '\n' it stopped on, so we put it back.
        // The exception is the last line of a file that had no newline on
        // the end, and eof() is how we can tell that apart.
        if (!source.eof()) {
            dest << "\n";
        }

        if (!dest.good()) {
            cerr << "mmcopier: writing to " << destName << " failed" << endl;
            source.close();
            dest.close();
            return false;
        }
    }

    source.close();
    dest.close();
    return true;
}

/*
 * The thread function. pthread_create can only hand over one void pointer,
 * so we point it at a CopyJob and cast it back here.
 */
void *runner(void *param) {
    CopyJob *job = (CopyJob *) param;
    job->copied = copyOneFile(job->sourceName, job->destName);
    pthread_exit(0);
}

/*
 * Makes destination_dir if it does not exist yet. Returns false if we end
 * up without a usable directory.
 */
bool makeDestDir(string path) {
    struct stat info;

    if (stat(path.c_str(), &info) == 0) {
        if (!S_ISDIR(info.st_mode)) {
            cerr << "mmcopier: " << path << " is not a directory" << endl;
            return false;
        }
        return true;
    }

    if (mkdir(path.c_str(), 0755) != 0) {
        cerr << "mmcopier: cannot create " << path
             << " (" << strerror(errno) << ")" << endl;
        return false;
    }
    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " n source_dir destination_dir" << endl;
        return 1;
    }

    // atoi has no way of telling us the argument was rubbish, so check the
    // characters first. "abc" would otherwise come back as 0.
    string countArg = argv[1];
    if (countArg.empty() ||
        countArg.find_first_not_of("0123456789") != string::npos) {
        cerr << "mmcopier: n must be a number" << endl;
        return 1;
    }

    int n = atoi(argv[1]);
    if (n < MIN_THREADS || n > MAX_THREADS) {
        cerr << "mmcopier: n must be between " << MIN_THREADS
             << " and " << MAX_THREADS << endl;
        return 1;
    }

    string sourceDir = argv[2];
    string destDir = argv[3];

    struct stat sourceInfo;
    if (stat(sourceDir.c_str(), &sourceInfo) != 0 ||
        !S_ISDIR(sourceInfo.st_mode)) {
        cerr << "mmcopier: " << sourceDir
             << " is not a directory we can read" << endl;
        return 1;
    }

    if (!makeDestDir(destDir)) {
        return 1;
    }

    // n never goes above 10, so plain arrays are enough and there is nothing
    // to delete later.
    pthread_t threads[MAX_THREADS];
    CopyJob jobs[MAX_THREADS];

    for (int i = 0; i < n; i++) {
        ostringstream fileName;
        fileName << "source" << (i + 1) << ".txt";

        jobs[i].sourceName = sourceDir + "/" + fileName.str();
        jobs[i].destName = destDir + "/" + fileName.str();
        jobs[i].copied = false;
    }

    // started counts the threads that really got going. If pthread_create
    // fails partway we must only join the ones that exist.
    int started = 0;
    for (int i = 0; i < n; i++) {
        int rc = pthread_create(&threads[i], NULL, runner, &jobs[i]);
        if (rc) {
            cerr << "mmcopier: pthread_create failed with code " << rc << endl;
            break;
        }
        started++;
    }

    bool allDone = (started == n);
    for (int i = 0; i < started; i++) {
        int rc = pthread_join(threads[i], NULL);
        if (rc) {
            cerr << "mmcopier: pthread_join failed with code " << rc << endl;
            allDone = false;
        }
        if (!jobs[i].copied) {
            allDone = false;
        }
    }

    if (!allDone) {
        cerr << "mmcopier: some files did not copy" << endl;
        return 1;
    }

    cout << "Copied " << n << " file(s) from " << sourceDir
         << " to " << destDir << endl;
    return 0;
}
