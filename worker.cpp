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

const int REQUEST_RESOURCES = 1;
const int RELEASE_RESOURCES = 0;
const int TERMINATE = -1;

int main() {
    srand(time(NULL));
    int pid = getpid();

    // Connect to the message queue
    key_t msg_key = ftok("oss_mq.txt", 1);
    int msqid = msgget(msg_key, PERMS);
    if (msqid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // Prepare the message
    msgbuffer msg;
    msg.mtype = 1; // Assuming 1 is the message type to communicate with oss
    msg.pid = pid;

    // Randomly decide between requesting resources, releasing resources, or terminating
    int rand_decision = rand() % 100;

    if (rand_decision < 80) { // 80% chance of requesting resources
        msg.action = REQUEST_RESOURCES;
        msg.resource = rand() % 10; // Randomly choose a resource from r0 to r9
        msg.amount = rand() % 20 + 1; // Request a random amount of resources (1 to 20)

    } else if (rand_decision < 95) { // 15% chance of releasing resources
        msg.action = RELEASE_RESOURCES;
        msg.resource = rand() % 10; // Randomly choose a resource from r0 to r9
        msg.amount = rand() % 20 + 1; // Request a random amount of resources (1 to 20)

    } else { // 5% chance of terminating
        msg.action = TERMINATE;
    }

    // Send the message to the parent process (oss)
    if (msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        exit(EXIT_FAILURE);
    }

    // ... (Rest of the worker code)

    return 0;
}
