#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <pthread.h>

using namespace std;

const int MAX_QUEUE_SIZE = 20;

struct Queue {
    vector<string> q; 

    void push(string item) {
        q.push_back(item); //Unprotect overflow rn
    }

    string pop() {
        string item = q.front();
        q.erase(q.begin());
        return item;
    }

    bool isEmpty() {
        return q.empty();
    }

    bool isFull() {
        return (q.size() >= MAX_QUEUE_SIZE);
    }
};

Queue buffer;
bool firstWrite = true;


ifstream inputFile;
ofstream outputFile;

int threadMax;
int activeProducers;
bool doneReading = false;

pthread_t *readersThreadArray; //Init size once args are read
pthread_t *writersThreadArray;

pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;

void *producer(void *arg) {
    while (true) {
        pthread_mutex_lock(&queueMutex);

        if (buffer.isFull()) {
            pthread_mutex_unlock(&queueMutex);
            continue; //Busy wait problem here
        }

        string read;
        if (!getline(inputFile, read)) {
            pthread_mutex_unlock(&queueMutex);
            break;                          // no more data to read
        }

        buffer.push(read);
        pthread_mutex_unlock(&queueMutex);

    }

    // this producer is finished REVIEW<<<<<<<<<<<<<<<<<<
    pthread_mutex_lock(&queueMutex);
    activeProducers--;
    if (activeProducers == 0) {
        doneReading = true;
    }

    pthread_mutex_unlock(&queueMutex);

    return nullptr;
}

void *consumer(void *arg) {
    while (true) {
        pthread_mutex_lock(&queueMutex);

        if (buffer.isEmpty() && !doneReading) {
            pthread_mutex_unlock(&queueMutex);
            continue; //Busy wait problem here
        }

        if (buffer.isEmpty() && doneReading) {
            pthread_mutex_unlock(&queueMutex);
            break;
        }

        string consumed = buffer.pop();
        
        //Remove 1 too many newlines in output
        if (!firstWrite) {
            outputFile << "\n";
        }
        outputFile << consumed;
        firstWrite = false;

        pthread_mutex_unlock(&queueMutex);
    }

    return nullptr;
}

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        cerr << "Usage: " << argv[0] << " n inputFile outputFile" << endl;
        return 1;
    }

    // n = number of threads for readers and writers, n=10 means 10 reader 10 writers
    threadMax = atoi(argv[1]);
    string inputPathStr = string(argv[2]) + ".txt";
    string outputPathStr = string(argv[3]) + ".txt";
    const char *inputPath = inputPathStr.c_str();
    const char *outputPath = outputPathStr.c_str();

    // load / create files
    inputFile.open(inputPath);
    outputFile.open(outputPath);

    activeProducers = threadMax;

    readersThreadArray = new pthread_t[threadMax];
    writersThreadArray = new pthread_t[threadMax];

    // generate threads
    for (int i = 0; i < threadMax; i++) {
        pthread_create(&readersThreadArray[i], nullptr, producer, nullptr);
        pthread_create(&writersThreadArray[i], nullptr, consumer, nullptr);
    }

    // use threads to create read one file then write
    for (int i = 0; i < threadMax; i++) {
        pthread_join(readersThreadArray[i], nullptr);
        pthread_join(writersThreadArray[i], nullptr);
    }

    delete[] readersThreadArray;
    delete[] writersThreadArray;

    inputFile.close();
    outputFile.close();

    return 0;
}