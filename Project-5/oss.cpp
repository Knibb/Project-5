#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <time.h>
#include <signal.h>
#include "oss_shared.h"

static int shm_id;
static int msg_id;
static SharedMemory *shm_ptr;

void cleanup(int signum) {
    if (shm_ptr) {
        shmdt(shm_ptr);
    }
    if (shm_id) {
        shmctl(shm_id, IPC_RMID, NULL);
    }
    if (msg_id) {
        msgctl(msg_id, IPC_RMID, NULL);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    key_t shm_key = ftok("oss_shared.h", 1);
    shm_id = shmget(shm_key, sizeof(SharedMemory), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    shm_ptr = (SharedMemory *)shmat(shm_id, NULL, 0);
    if ((int)shm_ptr == -1) {
        perror("shmat");
        exit(1);
    }

    key_t msg_key = ftok("oss_shared.h", 2);
    msg_id = msgget(msg_key, IPC_CREAT | 0666);
    if (msg_id < 0) {
        perror("msgget");
        exit(1);
    }

    // Initialize shared memory
    init_shared_memory(shm_ptr);

    // Main loop for generating processes and managing resources
    while (1) {
        // Generate processes
        generate_processes(shm_ptr);

        // Check for messages from user processes
        process_messages(shm_ptr, msg_id);

        // Periodically run deadlock detection and recovery
        deadlock_detection_and_recovery(shm_ptr);

        // Check termination conditions
        if (termination_conditions_met(shm_ptr)) {
            break;
        }
    }

    cleanup(0);
    return 0;
}
