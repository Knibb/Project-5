// worker.cpp
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include "shared_data.h"

const int BOUND_B = 10;

bool running = true;

void sig_handler(int signum) {
    running = false;
}

void clean_resources(int shmid, SharedData* shared_data) {
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, nullptr);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    int process_id = atoi(argv[1]);
    key_t shm_key = ftok("shmfile", SHM_KEY);
    int shmid = shmget(shm_key, sizeof(SharedData), 0666 | IPC_CREAT);
    SharedData* shared_data = (SharedData*)shmat(shmid, nullptr, 0);

    srand(time(0) ^ process_id);

    while (running) {
        // Generate a random number in the range [0, BOUND_B]
        int random_number = rand() % (BOUND_B + 1);

        // Request or release resources based on the random number

        // Check if the process should terminate
        if (shared_data->terminate && shared_data->process_id == process_id) {
            clean_resources(shmid, shared_data);
            break;
        }

        // Sleep for a random duration between 0 and 250 milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 251));
    }

    return 0;
}
