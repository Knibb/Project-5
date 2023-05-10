#include <iostream>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <unistd.h>
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

const int REQUEST_RESOURCES = 1;
const int RELEASE_RESOURCES = 0;
const int TERMINATE = -1;

// Helper function to increment the appropriate resource field
void increment_resource(struct resource_descriptor *locRec, int resource) {
    switch (resource) {
        case 0: locRec->r0++; break;
        case 1: locRec->r1++; break;
        case 2: locRec->r2++; break;
        case 3: locRec->r3++; break;
        case 4: locRec->r4++; break;
        case 5: locRec->r5++; break;
        case 6: locRec->r6++; break;
        case 7: locRec->r7++; break;
        case 8: locRec->r8++; break;
        case 9: locRec->r9++; break;
        default: printf("Invalid resource number: %d\n", resource);
    }
}

// Helper function to decrement the appropriate resource field
void decrement_resource(struct resource_descriptor *locRec, int resource) {
    switch (resource) {
        case 0: locRec->r0--; break;
        case 1: locRec->r1--; break;
        case 2: locRec->r2--; break;
        case 3: locRec->r3--; break;
        case 4: locRec->r4--; break;
        case 5: locRec->r5--; break;
        case 6: locRec->r6--; break;
        case 7: locRec->r7--; break;
        case 8: locRec->r8--; break;
        case 9: locRec->r9--; break;
        default: printf("Invalid resource number: %d\n", resource);
    }
}

int requestsCounter = 0;

int main(int argc, char* argv[]) {
    srand(time(NULL));
    int pid = getpid();

    struct resource_descriptor locRec;

    //Initialize local resources
    locRec.r0 = 0;
    locRec.r1 = 0;
    locRec.r2 = 0;
    locRec.r3 = 0;
    locRec.r4 = 0;
    locRec.r5 = 0;
    locRec.r6 = 0;
    locRec.r7 = 0;
    locRec.r8 = 0;
    locRec.r9 = 0;

    // Connect to the message queue
    key_t msg_key = ftok("oss_mq.txt", 1);
    int msqid = msgget(msg_key, PERMS);
    if (msqid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    SimulatedClock startTime = *simClock;
    bool terminated = false;

    // Check if it's time to terminate
    unsigned int elapsedTime = (simClock->seconds - startTime.seconds) * 1000000000 + (simClock->nanoseconds - startTime.nanoseconds);
    if (elapsedTime >= 1000000000) { // 1 second in nanoseconds
        int terminateChance = rand() % 100;
        if (terminateChance < 10) { // 10% chance to terminate after 1 second
            terminated = true;
        }
    }

    printf("In worker: made it past time termination check\n"); //comment out after testing

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
            msg.amount = 1; // Request one resource
        } else if (rand_decision < 95) { // 15% chance of releasing resources
            msg.action = RELEASE_RESOURCES;
            msg.resource = rand() % 10; // Randomly choose a resource from r0 to r9
            msg.amount = 1; // Release one resource
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
        reply.mtype = getppid();

        if (msgrcv(msqid, &reply, sizeof(msgbuffer) - sizeof(long), pid, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        // Process the reply
        if (reply.action == REQUEST_RESOURCES) {
            increment_resource(&locRec, reply.resource);

        }else if (reply.action == RELEASE_RESOURCES) {
            decrement_resource(&locRec, reply.resource);
            
        } else if (reply.action == TERMINATE) {
            // Detach from shared memory before exiting normally
            if (shmdt(simClock) == -1) {
                perror("shmdt");
                exit(EXIT_SUCCESS);
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