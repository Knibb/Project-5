#include <iostream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <condition_variable>
#include <time.h>
#include <ctime>
#include <queue>
#include <random>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>

using namespace std;

#define PERMS 0666

const int MAX_REC = 20;
const int MAX_USER_PROCESSES = 18;
const int MAX_TERMINATED = 40;
const int MAX_RUNTIME = 5;
const key_t SHM_KEY = 1616; // Shared memory key
const int UPDATE_CLOCK = 2; // New message action for clock updates

typedef struct msgbuffer {
    long mtype;
    int resource;
    int action;
    int pid;
    int amount;
}msgbuffer;

typedef struct resource_descriptor {
    int r0;
    int r1;
    int r2;
    int r3;
    int r4;
    int r5;
    int r6;
    int r7;
    int r8;
    int r9;
}resource_descriptor;

typedef struct PCB {
    int occupied; // either true or false
    pid_t pid; // process id of this child
    int startSeconds; // time when it was forked
    int startNano; // time when it was forked
    int blocked; // -1 if not blocked otherwise 0-9 on what it is blocked on
    int resources[10];
}PCB;

struct SimulatedClock {
    unsigned int seconds;
    unsigned int nanoseconds;
}SimulatedClock;

const int SHM_SIZE = sizeof(SimulatedClock) + MAX_USER_PROCESSES * sizeof(PCB);

void output_state(PCB *pcbTable, resource_descriptor &my_resource_descriptor) {
    ofstream log_file("oss.log", ios::app);
    
    log_file << "Current System State:\n";
    log_file << "Resources: ";
    for (int i = 0; i < 10; ++i) {
        log_file << "R" << i << ": " << my_resource_descriptor.resources[i] << " ";
    }
    log_file << "\n";

    log_file << "Processes:\n";
    for (int i = 0; i < MAX_USER_PROCESSES; ++i) {
        if (pcbTable[i].occupied) {
            log_file << "PID: " << pcbTable[i].pid << ", Blocked: ";
            if (pcbTable[i].blocked != -1) {
                log_file << "True, Resource: " << pcbTable[i].blocked << "\n";
            } else {
                log_file << "False\n";
            }
            log_file << "Allocated Resources: ";
            for (int j = 0; j < 10; ++j) {
                log_file << "R" << j << ": " << pcbTable[i].resources[j] << " ";
            }
            log_file << "\n";
        }
    }

    log_file.close();
}

int main() {
    
    time_t startTime = time(NULL);
    system("touch oss_mq.txt");

    if ((key_t msg_key = ftok("oss_mq.txt", 1)) == -1) {
        perror("msg_q ftok error");
        exit(EXIT_FAILURE);
    }

    // Create a message queue
    int msgid = msgget(msg_key, PERMS | IPC_CREAT);
    if ( msgid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    printf("Message queue setup\n");

    // Allocated shared memory
    int shmid = shmget(SHM_KEY, SHM_SIZE, PERMS | IPCREAT);
    if (shmid < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    printf("Shared memory setup\n");
    
    // Attach to shared memory
    void *shmaddr = shmat(shmid, NULL, 0);
    if (shmaddr == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Initialize clock in shared memory
    SimulatedClock *simClock = static_cast<SimulatedClock *>(shmaddr);
    simClock->seconds = 0;
    simClock->nanoseconds = 0;

    // Detach from shared memory
    if (shmdt(shmaddr) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    // Initialize resource descriptors
    struct resource_descriptor my_resource_descriptor;

    //Set all resources to MAX values
    for (int i = 0; i < 10; ++i){
        my_resource_descriptor.resources[i] = MAX_REC;
    }
    

    int children_created = 0;
    int activeChildren = 0;
    int terminatedChildren = 0;
    
    // Create a vector of blocked queues for each resource descriptor
    vector<queue<pid_t>> blocked_queues(10);

    // Prime random number generation for time increment and forking
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> timeDis(5000, 1000000000);
    uniform_int_distribution<> forkDis(1, 500);

    unsigned int nextForkTime = 0;

    output_state(pcbTable, my_resource_descriptor);

    while (activeChildren < MAX_USER_PROCESSES && children_created < MAX_TERMINATED) {
        // Check if 5 real-world seconds have passed
        time_t currentTime = time(NULL);
        if (difftime(currentTime, startTime) >= MAX_RUNTIME) {
            // More than 5 seconds have passed
            break;
        }

        // Attach to shared memory
        SimulatedClock *simClock = static_cast<SimulatedClock *>(shmaddr);

        // Increment simulated clock
        unsigned int increment = timeDis(gen);
        simClock->nanoseconds += increment;
        if (simClock->nanoseconds >= 1000000000) {
            simClock->seconds += simClock->nanoseconds / 1000000000;
            simClock->nanoseconds %= 1000000000;
        }

        // Check for UPDATE_CLOCK messages (non-blocking)
        msgbuffer clockUpdate;
        while (msgrcv(msqid, &clockUpdate, sizeof(clockUpdate) - sizeof(long), 1, IPC_NOWAIT) != -1) {
            if (clockUpdate.action == UPDATE_CLOCK) {
                simClock->nanoseconds += clockUpdate.amount;
                if (simClock->nanoseconds >= 1000000000) {
                    simClock->seconds += 1;
                    simClock->nanoseconds -= 1000000000;
                }
            }
        }

    // Receive a message from the worker processes
    msgbuffer msg;
    if (msgrcv(msqid, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1) {
        perror("msgrcv");
        exit(EXIT_FAILURE);
    }

        // Check if it's time to fork a new child process
        if (simClock->nanoseconds >= nextForkTime) {
           pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                //child
                execl("./worker", "worker", (char *) NULL);

                // should reach this but if we do throw error
                perror("execl");
                exit(EXIT_FAILURE);
            } else {
                // Parent
                activeChildren++;
                children_created++;

                // Update the PCB table for the new child process
                int childIndex = findEmptyPCBIndex(pcbTable);
                if (childIndex >= 0) {
                    pcbTable[childIndex].occupied = true;
                    pcbTable[childIndex].pid = pid;
                    pcbTable[childIndex].blocked = -1;
                } 
            }

            //update the next for time
            unsigned int forkIncrement = forkDis(gen);
            nextForkTime = simClock->nanoseconds + forkIncrement;

            //Increment children_created
            children_created++;
        }

        ssize_t result = msgrcv(msgid, &msg, sizeof(msg) - sizeof(msg.mtype), 0, IPC_NOWAIT);
        if (result < 0) {
            // Check if the error was caused by no message being available
            if (errno == ENOMSG) {
                // No message available, continue execution
            } else {
                perror("msgrcv");
                exit(EXIT_FAILURE);
            }
        } else {
            // Message received, process the message
            int childIndex = -1;
            for (int i = 0; i < MAX_USER_PROCESSES; ++i) {
                if (pcbTable[i].pid == msg.pid) {
                    childIndex = i;
                    break;
                }
            }

            if (childIndex == -1) {
                // Error: could not find the child process in PCB table
                cerr << "Error: child process not found in PCB table. PID: " << msg.pid << endl;
                continue;
            }

            if (msg.action == REQUEST_RESOURCES) {
                // Request resources
                if (my_resource_descriptor.resources[msg.resource] >= msg.amount) {
                    // Grant the request, update the resource descriptor and process allocation
                    my_resource_descriptor.resources[msg.resource] -= msg.amount;
                    pcbTable[childIndex].resources[msg.resource] += msg.amount;
                } else {
                    // Not enough resources available, block the process
                    pcbTable[childIndex].blocked = msg.resource;
                    blocked_queues[msg.resource].push(msg.pid);
                }
            } else if (msg.action == RELEASE_RESOURCES) {
                // Release resources
                if (pcbTable[childIndex].resources[msg.resource] >= msg.amount) {
                    // Release the resources, update the resource descriptor and process allocation
                    pcbTable[childIndex].resources[msg.resource] -= msg.amount;
                    my_resource_descriptor.resources[msg.resource] += msg.amount;
                } else {
                    // Error: process trying to release more resources than it owns
                    cerr << "Error: process trying to release more resources than it owns. PID: " << msg.pid << endl;
                }
            } else if (msg.action == TERMINATE) {
                // Terminate process
                waitpid(msg.pid, NULL, 0); // wait for resources to get released and for process to die
                terminatedChildren++;
                activeChildren--;

                // Release all resources held by the process
                for (int i = 0; i < 10; ++i) {
                    my_resource_descriptor.resources[i] += pcbTable[childIndex].resources[i];
                    pcbTable[childIndex].resources[i] = 0;
                }

                // Mark the PCB entry as unoccupied
                pcbTable[childIndex].occupied = false;
                pcbTable[childIndex].pid = 0;
                pcbTable[childIndex].blocked = -1;
            } else {
                cout << "Invalid message received. Action: " << msg.action << endl;
            }

            // Check blocked queues
            for (int i = 0; i < 10; ++i) {
                if (!blocked_queues[i].empty() && activeChildren < MAX_USER_PROCESSES) {
                    pid_t blocked_pid = blocked_queues[i].front();
                    int blocked_index = findIndexByPID(pcbTable, blocked_pid);

                    if (blocked_index >= 0) {
                        // Check if the requested resources are now available
                        bool resources_available = true;
                        for (int j = 0; j < 10; ++j) {
                            if (pcbTable[blocked_index].request[j] > my_resource_descriptor.resources[j]) {
                                resources_available = false;
                                break;
                            }
                        }

                        if (resources_available) {
                            // Grant the requested resources and unblock the process
                            for (int j = 0; j < 10; ++j) {
                                my_resource_descriptor.resources[j] -= pcbTable[blocked_index].request[j];
                                pcbTable[blocked_index].resources[j] += pcbTable[blocked_index].request[j];
                                pcbTable[blocked_index].request[j] = 0;
                            }
                            pcbTable[blocked_index].blocked = -1;
                            blocked_queues[i].pop();
                        }
                    }
                }
            }
            output_state(pcbTable, my_resource_descriptor);
        }
    }
    
    while (activeChildren > 0) {
        int status;
        pid_t pid = wait(&status);

        if (pid < 0) {
            perror("wait");
            exit(EXIT_FAILURE);
        }

        activeChildren--;
    }

    // Detach from shared memory
    if (shmdt(shmaddr) == -1) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }

    // Clean up shared memory
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(EXIT_FAILURE);
    }

    // Clean up message queue
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }

    output_state(pcbTable, my_resource_descriptor);

    return 0;
}