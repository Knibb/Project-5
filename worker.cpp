#include <iostream>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include "resource_descriptor.h"

using namespace std;

#define PERMS 0666

typedef struct msgbuffer {
    long mtype;
    int action;
    int pid;
    int resource;
    int amount;
} msgbuffer;

struct SimulatedClock {
    unsigned int seconds;
    unsigned int nanoseconds;
};

const int REQUEST_RESOURCES = 1;
const int RELEASE_RESOURCES = 0;
const int TERMINATE = -1;
const int UPDATE_CLOCK = 2; // New message action for clock updates
const key_t SHM_KEY = 1616; // Shared memory key

int shmid;
SimulatedClock* simClock;

int allocatedResources[10] = {0};
int maxResourceInstances[10] = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20};

int grantedRequests = 0;
int terminatedProcesses = 0;
int deadlockDetections = 0;
ofstream logFile;

// Signal handler function for cleanup and termination
void signalHandler(int signum) {
    // Detach from shared memory
    if (shmdt(simClock) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    // Remove shared memory (only if it's the last process using it)
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
    }

    // Close log file
    logFile.close();

    // Terminate the process
    exit(signum);
}

int requestsCounter = 0;

int main(int argc, char* argv[]) {
    srand(time(NULL));
    int pid = getpid();

    //Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Process command-line arguments for log verbosity
    int logVerbosity = 1; // Default log verbosity level
    if (argc > 1) {
        logVerbosity = atoi(argv[1]);
    }

    // Open log file for writing
    logFile.open("worker_log.txt", ios::out | ios::app);
    if (!logFile.is_open()) {
        perror("Failed to open worker_log.txt");
        exit(EXIT_FAILURE);
    }

    // Connect to the message queue
    key_t msg_key = ftok("oss_mq.txt", 1);
    int msqid = msgget(msg_key, PERMS);
    if (msqid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // connect to our shared memory segment and see the simClock
    int shmid = shmget(SHM_KEY, sizeof(SimulatedClock), PERMS);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    SimulatedClock* simClock = (SimulatedClock*)shmat(shmid, nullptr, 0);

    SimulatedClock startTime = *simClock;
    bool terminated = false;

    while (!terminated) {
        // Check if it's time to terminate
        unsigned int elapsedTime = (simClock->seconds - startTime.seconds) * 1000000000 + (simClock->nanoseconds - startTime.nanoseconds);
        if (elapsedTime >= 1000000000) { // 1 second in nanoseconds
            int terminateChance = rand() % 100;
            if (terminateChance < 10) { // 10% chance to terminate after 1 second
                terminated = true;
            }
        } else {
            // Update the simulated clock at randome times (between 0 and 250ms)
            unsigned int timeToAdd = rand() % 250001;

            // Send a message to oss to request a clock update
            msgbuffer clockUpdate;
            clockUpdate.mtype = 1;
            clockUpdate.pid = pid;
            clockUpdate.action = UPDATE_CLOCK;
            clockUpdate.amount = timeToAdd;

            if (msgsnd(msqid, &clockUpdate, sizeof(clockUpdate) - sizeof(long), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }
        }
    }

        // If terminated, release all resources and send termination message
        if (terminated) {
            msgbuffer msg;
            msg.mtype = 1; 
            msg.pid = pid;
            msg.action = TERMINATE;

            if (msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }
        } else {

            // Prepare the message
            msgbuffer msg;
            msg.mtype = 1; 
            msg.pid = pid;

            // Randomly decide between requesting resources, releasing resources, or terminating
            int rand_decision = rand() % 100;
            

            if (rand_decision < 80) { // 80% chance of requesting resources
                msg.action = REQUEST_RESOURCES;
                msg.resource = rand() % 10; // Randomly choose a resource from r0 to r9
                int maxRequest = maxResourceInstances[msg.resource] - allocatedResources[msg.resource];
                msg.amount = rand() % maxRequest + 1; // Request a random amount of resources within the allowed limit
            } else if (rand_decision < 95) { // 15% chance of releasing resources
                msg.action = RELEASE_RESOURCES;
                msg.resource = rand() % 10; // Randomly choose a resource from r0 to r9
                msg.amount = rand() % (allocatedResources[msg.resource] + 1); // Release a random amount of allocated resources
            } else { // 5% chance of terminating
                msg.action = TERMINATE;
            }

            // Send the message to the parent process (oss)
            if (msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }

            // Wait for a message back indicating the request was granted before continuing
            msgbuffer reply;
            if (msgrcv(msqid, &reply, sizeof(reply) - sizeof(long), pid, 0) == -1) {
                perror("msgrcv");
                exit(EXIT_FAILURE);
            }

            // Process the reply
            if (reply.action == REQUEST_RESOURCES) {
                allocatedResources[reply.resource] += reply.amount;
                grantedRequests++;
                requestsCounter++;
                if (logVerbosity > 1) {
                    logFile << "Worker " << pid << ": Granted request for resource " << reply.resource << ", amount: " << reply.amount << endl;
                }

                // Output the allocated resources table every 20 granted requests
                if (requestsCounter % 20 == 0) {
                    logFile << "Allocated Resources Table for Worker " << pid << ":\n";
                    logFile << "Resource\tAmount\n";
                    for (int i = 0; i < 10; i++) {
                        logFile << "r" << i << "\t\t" << allocatedResources[i] << "\n";
                    }
                    logFile << endl;
                }

            }else if (reply.action == RELEASE_RESOURCES) {
                allocatedResources[reply.resource] -= reply.amount;
                if (logVerbosity > 1) {
                    logFile << "Worker " << pid << ": Released resource " << reply.resource << ", amount: " << reply.amount << endl;
                }
            } else if (reply.action == TERMINATE) {
                terminated = true;
                terminatedProcesses++;
                if (logVerbosity > 0) {
                    logFile << "Worker " << pid << ": Terminated" << endl;
                }
            }
        }

        // Print the table of allocated resources if the log verbosity is set to the highest level (3)
        if (logVerbosity > 2) {
            logFile << "Allocated Resources Table for Worker " << pid << ":\n";
            logFile << "Resource\tAmount\n";
            for (int i = 0; i < 10; i++) {
                logFile << "r" << i << "\t\t" << allocatedResources[i] << "\n";
            }
            logFile << endl;
        }

    } // End of while loop

    // Output statistics
    logFile << "Worker " << pid << " Statistics:\n";
    logFile << "Granted Requests: " << grantedRequests << "\n";
    logFile << "Terminated Processes: " << terminatedProcesses << "\n";
    logFile << "Deadlock Detections: " << deadlockDetections << "\n";
    logFile << endl;

    // Detach from shared memory before exiting normally
    if (shmdt(simClock) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    // Close log file
    logFile.close();

    return 0;
}
