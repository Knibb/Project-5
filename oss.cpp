#include <iostream>
#include <stdarg.h>
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
#include "resource_descriptor.h"

using namespace std;

#define PERMS 0666

const int MAX_REC = 20;
const int MAX_USER_PROCESSES = 18;
const int MAX_RUNTIME = 5;

typedef struct msgbuffer {
    long mtype;
    int resource;
    int action;
    int pid;
    int amount;
}msgbuffer;

typedef struct PCB {
    int occupied; // either true or false
    pid_t pid; // process id of this child
    int startSeconds; // time when it was forked
    int startNano; // time when it was forked
    int blocked; // -1 if not blocked otherwise 0-9 on what it is blocked on
    struct resource_descriptor recs;
}PCB;

typedef struct SimulatedClock{
    unsigned int seconds;
    unsigned int nanoseconds;
} SimulatedClock;

queue<int> removePid(queue<int> myQueue, int pid){
    queue<int> funcQueue;
    while (myQueue.size() > 0){
        if (myQueue.front() != pid)
            funcQueue.push(myQueue.front());
        myQueue.pop();
    }
    return funcQueue;
}

vector<int> deadlockDetection(PCB processTable[], resource_descriptor resources) {
    PCB tempTable[18];
    resource_descriptor tempResources = resources;
    vector<int> deadlockedProcesses;

    // Copy the process table to a temporary table
    for (int i = 0; i < 18; ++i) {
        tempTable[i] = processTable[i];
    }

    // Remove non-blocked processes and release their resources
    for (int i = 0; i < 18; ++i) {
        if (tempTable[i].blocked == -1) {
            tempResources.r0 += tempTable[i].recs.r0;
            tempResources.r1 += tempTable[i].recs.r1;
            tempResources.r2 += tempTable[i].recs.r2;
            tempResources.r3 += tempTable[i].recs.r3;
            tempResources.r4 += tempTable[i].recs.r4;
            tempResources.r5 += tempTable[i].recs.r5;
            tempResources.r6 += tempTable[i].recs.r6;
            tempResources.r7 += tempTable[i].recs.r7;
            tempResources.r8 += tempTable[i].recs.r8;
            tempResources.r9 += tempTable[i].recs.r9;
            tempTable[i].recs = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            tempTable[i].occupied = false;
        }
    }

    // Count blocked processes
    int blockedProcessCount = 0;
    for (int i = 0; i < 18; ++i) {
        if (tempTable[i].blocked != -1) {
            blockedProcessCount++;
        }
    }

    // Check if blocked processes can be unblocked by available resources
    for (int i = 0; i < blockedProcessCount; ++i) {
        for (int j = 0; j < 18; ++j) {
            if (tempTable[j].occupied && tempTable[j].blocked != -1) {
                int required_resource = tempTable[j].blocked;
                int available_resource = 0;
                switch (required_resource) {
                    case 0: available_resource = tempResources.r0; break;
                    case 1: available_resource = tempResources.r1; break;
                    case 2: available_resource = tempResources.r2; break;
                    case 3: available_resource = tempResources.r3; break;
                    case 4: available_resource = tempResources.r4; break;
                    case 5: available_resource = tempResources.r5; break;
                    case 6: available_resource = tempResources.r6; break;
                    case 7: available_resource = tempResources.r7; break;
                    case 8: available_resource = tempResources.r8; break;
                    case 9: available_resource = tempResources.r9; break;
                }
                if (available_resource > 0) {
                    tempResources.r0 += tempTable[j].recs.r0;
                    tempResources.r1 += tempTable[j].recs.r1;
                    tempResources.r2 += tempTable[j].recs.r2;
                    tempResources.r3 += tempTable[j].recs.r3;
                    tempResources.r4 += tempTable[j].recs.r4;
                    tempResources.r5 += tempTable[j].recs.r5;
                    tempResources.r6 += tempTable[j].recs.r6;
                    tempResources.r7 += tempTable[j].recs.r7;
                    tempResources.r8 += tempTable[j].recs.r8;
                    tempResources.r9 += tempTable[j].recs.r9;
                    tempTable[j].blocked = -1;
                    blockedProcessCount--;
                    break;
                }
            }
        }
    }

    // Add remaining blocked processes to the deadlockedProcesses vector
    for (int i = 0; i < 18; ++i) {
        if (tempTable[i].blocked != -1) {
            deadlockedProcesses.push_back(tempTable[i].pid);
        }
    }

    return deadlockedProcesses;
}

int get_resource_value(resource_descriptor& rd, int index) {
    switch (index) {
        case 0: return rd.r0;
        case 1: return rd.r1;
        case 2: return rd.r2;
        case 3: return rd.r3;
        case 4: return rd.r4;
        case 5: return rd.r5;
        case 6: return rd.r6;
        case 7: return rd.r7;
        case 8: return rd.r8;
        case 9: return rd.r9;
        default: return -1; // Error case
    }
}

void set_resource_value(resource_descriptor& rd, int index, int value) {
    switch (index) {
        case 0: rd.r0 = value; break;
        case 1: rd.r1 = value; break;
        case 2: rd.r2 = value; break;
        case 3: rd.r3 = value; break;
        case 4: rd.r4 = value; break;
        case 5: rd.r5 = value; break;
        case 6: rd.r6 = value; break;
        case 7: rd.r7 = value; break;
        case 8: rd.r8 = value; break;
        case 9: rd.r9 = value; break;
        default: break; // Error case, do nothing
    }
}

int findEmptyPCBIndex(PCB pcbTable[], int tableSize) {
    for (int i = 0; i < tableSize; ++i) {
        if (pcbTable[i].occupied == false) {
            return i;
        }
    }
    return -1; // All entries are occupied
}

unsigned int log_lines = 0;
const unsigned int MAX_LOG_LINES = 100000;

void log_message(FILE *logfile, bool verbose_mode, bool is_verbose, const char *format, ...) {
    if (log_lines >= MAX_LOG_LINES) {
        return;
    }
    if (verbose_mode || !is_verbose) {
        va_list args;
        va_start(args, format);
        vfprintf(logfile, format, args);
        va_end(args);
        log_lines++;
    }
}

int main(int argc, char *argv[]) {
    
    time_t startTime = time(NULL);
    system("touch oss_mq.txt");
    system("touch shm.txt");
    int msgid;
    key_t msg_key;
    msgbuffer msg;
    int MAX_TERMINATED = 40;
    int opt;
    bool verbose = false;
    
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                verbose = true;
                break;
            default:
                fprintf(stderr, "Usage: %s [-v]\n", argv[0]);
                exit(EXIT_FAILURE);
                break;
        }
    }

    FILE *logfile = fopen("logfile.txt", "w");
    if (logfile == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }


    PCB pcbTable[18];
    for (int i = 0; i < 18; i++)
    {
        pcbTable[i].blocked = -1;
        pcbTable[i].occupied = 0;
        pcbTable[i].pid = 0;
        pcbTable[i].startNano = 0;
        pcbTable[i].startSeconds = 0;
        pcbTable[i].recs.r0 = 0;
        pcbTable[i].recs.r1 = 0;
        pcbTable[i].recs.r2 = 0;
        pcbTable[i].recs.r3 = 0;
        pcbTable[i].recs.r4 = 0;
        pcbTable[i].recs.r5 = 0;
        pcbTable[i].recs.r6 = 0;
        pcbTable[i].recs.r7 = 0;
        pcbTable[i].recs.r8 = 0;
        pcbTable[i].recs.r9 = 0;
    }
    
    int findEmptyPCBIndex(PCB pcbTable[], int tableSize);

    if ((msg_key = ftok("oss_mq.txt", 1)) == -1) {
        perror("msg_q ftok error");
        exit(EXIT_FAILURE);
    }

    // Create a message queue
    msgid = msgget(msg_key, PERMS | IPC_CREAT);
    if ( msgid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    printf("Message queue setup\n");

    // Allocated shared memory
    key_t SHM_KEY = ftok("shm.txt", 2);
    if (SHM_KEY == -1) {
        perror("shm ftok error");
        exit(EXIT_FAILURE);
    }

    int shmid = shmget(SHM_KEY, sizeof(SimulatedClock), PERMS | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    printf("Shared memory setup\n");
    
    // Attach to shared memory
    SimulatedClock *simClock = (SimulatedClock *) shmat(shmid, NULL, 0);
    if ((void *) simClock == (void *) -1) {
        perror("shamt");
        exit(EXIT_FAILURE);
    }

    // Initialize clock in shared memory
    simClock->seconds = 0;
    simClock->nanoseconds = 0;

    resource_descriptor my_recs;

    my_recs.r0 = MAX_REC;
    my_recs.r1 = MAX_REC;
    my_recs.r2 = MAX_REC;
    my_recs.r3 = MAX_REC;
    my_recs.r4 = MAX_REC;
    my_recs.r5 = MAX_REC;
    my_recs.r6 = MAX_REC;
    my_recs.r7 = MAX_REC;
    my_recs.r8 = MAX_REC;
    my_recs.r9 = MAX_REC;
    

    int children_created = 0;
    int activeChildren = 0;
    int terminatedChildren = 0;
    
    // Create a blocked queues
    queue<int>blockedQ0;
    queue<int>blockedQ1;
    queue<int>blockedQ2;
    queue<int>blockedQ3;
    queue<int>blockedQ4;
    queue<int>blockedQ5;
    queue<int>blockedQ6;
    queue<int>blockedQ7;
    queue<int>blockedQ8;
    queue<int>blockedQ9;

    int nextInterval = rand()% 500000000;
    int nextL_Nano = simClock->nanoseconds + nextInterval;
    int nextL_Sec;
    if (nextL_Nano >= 1000000000)
    {
        nextL_Sec = simClock->seconds + 1;
        nextL_Nano -= 1000000000;
    } else {
        nextL_Sec = simClock->seconds;
    }

    int REQUEST_RESOURCES = 1;
    int RELEASE_RESOURCES = 0;
    int TERMINATE = -1;

    while (terminatedChildren < MAX_TERMINATED) {
        // Check if 5 real-world seconds have passed
        time_t currentTime = time(NULL);
        if (difftime(currentTime, startTime) >= MAX_RUNTIME) {
            // More than 5 seconds have passed
            MAX_TERMINATED = children_created;
        }

        // Increment simulated clock
        printf("secs before %d\n", simClock->seconds);
        simClock->nanoseconds += 1000000;
        if (simClock->nanoseconds >= 1000000000) {
            (simClock->seconds)++;
            simClock->nanoseconds -= 1000000000;
        }
        printf("secs after %d\n", simClock->seconds);

        printf("made it past time check\n"); //comment out after testing
    
        // Check if it's time to fork a new child process
        if ((simClock->seconds > nextL_Sec || simClock->seconds == nextL_Sec && simClock->nanoseconds >= nextL_Nano) && findEmptyPCBIndex(pcbTable, 18) != -1) {
            printf("###################################################################################we're forking\n");

            pid_t pid = fork();
            
            if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            else if (pid == 0) {
                //child
                execl("./worker", "worker", (char *) NULL);

                // should reach this but if we do throw error
                perror("execl");
                exit(EXIT_FAILURE);
            }

            else {
                // Parent
                activeChildren++;
                children_created++;

                nextInterval = rand()% 500000000;
                nextL_Nano = simClock->nanoseconds + nextInterval;
                nextL_Sec;
                if (nextL_Nano >= 1000000000)
                {
                    nextL_Sec = simClock->seconds + 1;
                    nextL_Nano -= 1000000000;
                } else {
                    nextL_Sec = simClock->seconds;
                }


                printf("Made it to start of parent\n"); //comment out after testing

                // Update the PCB table for the new child process
                int childIndex = findEmptyPCBIndex(pcbTable, 18);
                if (childIndex >= 0) {
                    pcbTable[childIndex].occupied = true;
                    pcbTable[childIndex].pid = pid;
                    pcbTable[childIndex].blocked = -1;
                    pcbTable[childIndex].startSeconds = simClock->seconds;
                    pcbTable[childIndex].startNano = simClock->nanoseconds;
                } 
            }

            printf("post PCB update\n"); //comment out after testing
        }

        ssize_t result = msgrcv(msgid, &msg, sizeof(msgbuffer), getpid(), IPC_NOWAIT);

        if (result == -1) {
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
                bool sendMsg = false;
                switch (msg.resource) {
                    case 0:
                        if (my_recs.r0 > 0){
                            sendMsg = true;
                            pcbTable[childIndex].recs.r0 += 1;
                            my_recs.r0--;
                            if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                                perror("msgsnd");
                                exit(EXIT_FAILURE);
                            }
                            log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 0, simClock->seconds, simClock->nanoseconds);
                            break;              
                        } else {
                            // Not enough resources available, block the process
                            pcbTable[childIndex].blocked = msg.resource;
                            blockedQ0.push(msg.pid);
                            break;
                        }
                    case 1:
                    if (my_recs.r1 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r1 += 1;
                        my_recs.r1--;
                        if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 1, simClock->seconds, simClock->nanoseconds);
                        break;                       
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ1.push(msg.pid);
                        break;
                    }
                    case 2:
                    if (my_recs.r2 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r2 += 1;
                        my_recs.r2--;
                        if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 2, simClock->seconds, simClock->nanoseconds);
                        break;                        
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ2.push(msg.pid);
                        break;
                    }
                    case 3:
                    if (my_recs.r3 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r3 += 1;
                        my_recs.r3--;
                        if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 3, simClock->seconds, simClock->nanoseconds);
                        break;                         
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ3.push(msg.pid);
                        break;
                    }
                    case 4:
                    if (my_recs.r4 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r4 += 1;
                        my_recs.r4--;
                        if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 4, simClock->seconds, simClock->nanoseconds);
                        break;                         
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ4.push(msg.pid);
                        break;
                    }
                    case 5:
                    if (my_recs.r5 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r5 += 1;
                        my_recs.r5--;
                        if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 5, simClock->seconds, simClock->nanoseconds);
                        break;                        
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ5.push(msg.pid);
                        break;
                    }
                    case 6:
                    if (my_recs.r6 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r6 += 1;
                        my_recs.r6--;
                        if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 6, simClock->seconds, simClock->nanoseconds);
                        break;                       
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ6.push(msg.pid);
                        break;
                    }
                    case 7:
                    if (my_recs.r7 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r7 += 1;
                        my_recs.r7--;
                        if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 7, simClock->seconds, simClock->nanoseconds);
                        break;                         
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ7.push(msg.pid);
                        break;
                    }
                    case 8:
                    if (my_recs.r8 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r8 += 1;
                        my_recs.r8--;
                        if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 8, simClock->seconds, simClock->nanoseconds);
                        break;                        
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ8.push(msg.pid);
                        break;
                    }
                    case 9:
                    if (my_recs.r9 > 0){
                        sendMsg = true;
                        pcbTable[childIndex].recs.r9 += 1;
                        my_recs.r9--;
                        if (msgsnd(msgid, &msg,sizeof(msg) - sizeof(long), 0) == -1) {
                            perror("msgsnd");
                            exit(EXIT_FAILURE);
                        }
                        log_message(logfile, verbose, true, "Master granting P%d request R%d at time %d:%d\n", pcbTable[childIndex].pid, 9, simClock->seconds, simClock->nanoseconds);
                        break;                        
                    } else {
                        // Not enough resources available, block the process
                        pcbTable[childIndex].blocked = msg.resource;
                        blockedQ9.push(msg.pid);
                        break;
                    }
                }
                
            }else if (msg.action == RELEASE_RESOURCES) {
                // Release resources
                switch (msg.resource) {
                    case 0:
                        pcbTable[childIndex].recs.r0 -= 1;
                        my_recs.r0++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 0, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 1:
                        pcbTable[childIndex].recs.r1 -= 1;
                        my_recs.r1++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 1, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 2:
                        pcbTable[childIndex].recs.r2 -= 1;
                        my_recs.r2++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 2, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 3:
                        pcbTable[childIndex].recs.r3 -= 1;
                        my_recs.r3++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 3, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 4:
                        pcbTable[childIndex].recs.r4 -= 1;
                        my_recs.r4++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 4, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 5:
                        pcbTable[childIndex].recs.r5 -= 1;
                        my_recs.r5++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 5, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 6:
                        pcbTable[childIndex].recs.r6 -= 1;
                        my_recs.r6++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 6, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 7:
                        pcbTable[childIndex].recs.r7 -= 1;
                        my_recs.r7++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 7, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 8:
                        pcbTable[childIndex].recs.r8 -= 1;
                        my_recs.r8++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 8, simClock->seconds, simClock->nanoseconds);
                        break;
                    case 9:
                        pcbTable[childIndex].recs.r9 -= 1;
                        my_recs.r9++;
                        log_message(logfile, verbose, true, "Master has acknowledged P%d releasing R%d at time %d:%d\n", pcbTable[childIndex].pid, 9, simClock->seconds, simClock->nanoseconds);
                        break;
                }
            } else if (msg.action == TERMINATE) {
                // Terminate process
                wait(NULL); // wait for resources to get released and for process to die
                terminatedChildren++;
                activeChildren--;

                // Release all resources held by the process
                for (int i = 0; i < 10; ++i) {
                    set_resource_value(my_recs, i, get_resource_value(my_recs, i) + get_resource_value(pcbTable[childIndex].recs, i));
                    set_resource_value(pcbTable[childIndex].recs, i, 0);
                }

                // Mark the PCB entry as unoccupied
                pcbTable[childIndex].occupied = false;
                pcbTable[childIndex].pid = 0;
                pcbTable[childIndex].blocked = -1;
            } else {
                perror("Error: Incorrect message perameters");
                exit(EXIT_FAILURE);//update later to be a better exit
            }

            int unblockedIndex = -1; // Declare unblockedIndex here
            int unblockedPid;

            switch (msg.resource)
            {
            case 0:
                if (blockedQ0.size() > 0){
                    unblockedPid = blockedQ0.front();
                    blockedQ0.pop();
                }
                break;
            case 1:
                if (blockedQ1.size() > 0){
                    unblockedPid = blockedQ1.front();
                    blockedQ1.pop();
                }
                break;
            case 2:
                if (blockedQ2.size() > 0){
                    unblockedPid = blockedQ2.front();
                    blockedQ2.pop();
                }
                break;
            case 3:
                if (blockedQ3.size() > 0){
                    unblockedPid = blockedQ3.front();
                    blockedQ3.pop();
                }
                break;
            case 4:
                if (blockedQ4.size() > 0){
                    unblockedPid = blockedQ4.front();
                    blockedQ4.pop();
                }
                break;
            case 5:
                if (blockedQ5.size() > 0){
                    unblockedPid = blockedQ5.front();
                    blockedQ5.pop();
                }
                break;
            case 6:
                if (blockedQ6.size() > 0){
                    unblockedPid = blockedQ6.front();
                    blockedQ6.pop();
                }
                break;
            case 7:
                if (blockedQ7.size() > 0){
                    unblockedPid = blockedQ7.front();
                    blockedQ7.pop();
                }
                break;
            case 8:
                if (blockedQ8.size() > 0){
                    unblockedPid = blockedQ8.front();
                    blockedQ8.pop();
                }
                break;
            case 9:
                if (blockedQ9.size() > 0){
                    unblockedPid = blockedQ9.front();
                    blockedQ9.pop();
                }
                break;
            }





            // // Check if any blocked processes can be unblocked
            // if (!blocked_queues[msg.resource].empty()) {
            //     unblockedPid = blocked_queues[msg.resource].front();
            //     blocked_queues[msg.resource].pop();

            //     // Find the unblocked process in the PCB table
            //     for (int i = 0; i < 18; i++) {
            //         if (pcbTable[i].pid == unblockedPid) {
            //             unblockedIndex = i;
            //             break;
            //         }
            //     }
            // }

            // Unblock the process and grant it the resource
            if (unblockedIndex != -1) {
                pcbTable[unblockedIndex].blocked = -1;
                set_resource_value(pcbTable[unblockedIndex].recs, msg.resource, get_resource_value(pcbTable[unblockedIndex].recs, msg.resource) + 1);
                set_resource_value(my_recs, msg.resource, get_resource_value(my_recs, msg.resource) - 1);

                // Send a message to the unblocked process
                msg.mtype = unblockedPid;
                msg.resource = msg.resource;
                msg.action = REQUEST_RESOURCES;
                if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                    perror("msgsnd");
                    exit(EXIT_FAILURE);
                }
            }
        }
        vector<int> deadlockedPids = deadlockDetection(pcbTable, my_recs);

        // Log a message when deadlock detection is run
        log_message(logfile, verbose, false, "Master running deadlock detection at time %d:%d\n", simClock->seconds, simClock->nanoseconds);

        for (int i = 0; i < deadlockedPids.size(); i++) {
            int deadlockedPid = deadlockedPids[i];

            msg.mtype = deadlockedPid;
            msg.action = -1;

            if (msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }

            int index = 0;
            for (int j = 0 ; j < 18 ; j++) {
                if (deadlockedPid == pcbTable[j].pid) {
                    index = j;
                    break;
                }
            }

            my_recs.r0 += pcbTable[index].recs.r0;
            pcbTable[index].recs.r0 = 0;

            my_recs.r1 += pcbTable[index].recs.r1;
            pcbTable[index].recs.r1 = 0;

            my_recs.r2 += pcbTable[index].recs.r2;
            pcbTable[index].recs.r2 = 0;

            my_recs.r3 += pcbTable[index].recs.r3;
            pcbTable[index].recs.r3 = 0;

            my_recs.r4 += pcbTable[index].recs.r4;
            pcbTable[index].recs.r4 = 0;

            my_recs.r5 += pcbTable[index].recs.r5;
            pcbTable[index].recs.r5 = 0;

            my_recs.r6 += pcbTable[index].recs.r6;
            pcbTable[index].recs.r6 = 0;
            
            my_recs.r7 += pcbTable[index].recs.r7;
            pcbTable[index].recs.r7 = 0;

            my_recs.r8 += pcbTable[index].recs.r8;
            pcbTable[index].recs.r8 = 0;

            my_recs.r9 += pcbTable[index].recs.r9;
            pcbTable[index].recs.r9 = 0;

            switch (pcbTable[index].blocked) {
                case 0:
                    blockedQ0 = removePid(blockedQ0, deadlockedPid);
                    break;
                case 1:
                    blockedQ1 = removePid(blockedQ1, deadlockedPid);
                    break;
                case 2:
                    blockedQ2 = removePid(blockedQ2, deadlockedPid);
                    break;
                case 3:
                    blockedQ3 = removePid(blockedQ3, deadlockedPid);
                    break;
                case 4:
                    blockedQ4 = removePid(blockedQ4, deadlockedPid);
                    break;
                case 5:
                    blockedQ5 = removePid(blockedQ5, deadlockedPid);
                    break;
                case 6:
                    blockedQ6 = removePid(blockedQ6, deadlockedPid);
                    break;
                case 7:
                    blockedQ7 = removePid(blockedQ7, deadlockedPid);
                    break;
                case 8:
                    blockedQ8 = removePid(blockedQ8, deadlockedPid);
                    break;
                case 9:
                    blockedQ9 = removePid(blockedQ9, deadlockedPid);
                    break;       
            }

            // Remove the process with deadlockedPid from the process table
            pcbTable[index].occupied = false;
            pcbTable[index].pid = -1;
            pcbTable[index].blocked = -1;

            // Wait for the child process to terminate
            wait(NULL);
            terminatedChildren++;

            // Log a message when a process is terminated to resolve a deadlock
            log_message(logfile, verbose, false, "Master terminating P%d to remove deadlock\n", pcbTable[deadlockedPid].pid);
        }
    }

    // Detach from shared memory
    if (shmdt(simClock) == -1) {
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

    //close logfile
    fclose(logfile);

    return 0;
}