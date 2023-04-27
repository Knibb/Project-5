#include <iostream>
#include <ctime>
#include <cstdlib>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include "resource_descriptor.h"

using namespace std;

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
const key_t SHM_KEY = 1616; // Shared memory key

int shmid;
SimulatedClock* simClock;

int allocatedResources[10] = {0};
int maxResourceInstances[10] = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20};

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

    // Terminate the process
    exit(signum);
}

int main() {
    srand(time(NULL));
    int pid = getpid();

    //Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

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
            // Update the simulated clock at random times (between 0 and 250ms)
            unsigned int timeToAdd = rand() % 250001; // Generate a random number in the range [0, 250000] nanoseconds
            simClock->nanoseconds += timeToAdd;
            if (simClock->nanoseconds >= 1000000000) {
                simClock->seconds += 1;
                simClock->nanoseconds -= 1000000000;
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
                int maxRequest = maxInstances[msg.resource] - allocatedResources[msg.resource];
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
            } else if (reply.action == RELEASE_RESOURCES) {
                allocatedResources[reply.resource] -= reply.amount;
            } else if (reply.action == TERMINATE) {
                terminated = true;
            }
        }
    }

    // Detach from shared memory before exiting normally
    if (shmdt(simClock) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    return 0;
}
