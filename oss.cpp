// oss.cpp
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "shared_data.h"

const int NUM_RESOURCES = 10;
const int NUM_INSTANCES = 20;
const int MAX_USER_PROCESSES = 18;
const int MAX_CHILDREN = 40;
const int MAX_RUNTIME = 5;

std::mutex resource_mutex;
std::condition_variable resource_cv;

struct ResourceDescriptor {
    int total_instances;
    int available_instances;
    int allocated_instances[MAX_USER_PROCESSES];
};

void release_resources(int process_id, ResourceDescriptor* resource_descriptors) {
    for (int i = 0; i < NUM_RESOURCES; ++i) {
        resource_descriptors[i].available_instances += resource_descriptors[i].allocated_instances[process_id];
        resource_descriptors[i].allocated_instances[process_id] = 0;
    }
}

void user_process(int process_id, ResourceDescriptor* resource_descriptors) {
    int max_iterations = 100;
    int current_iteration = 0;

    while (true) {
        // Implement user process logic here, including requesting, allocating, and releasing resources

        // Check for deadlock periodically
        if (check_deadlock()) {
            resolve_deadlock();
        }

        // Sleep for a random duration between 1 and 500 milliseconds
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<> dist(1, 500);
        std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng)));

        // Termination condition
        if (++current_iteration >= max_iterations) {
            std::unique_lock<std::mutex> lock(resource_mutex);
            release_resources(process_id, resource_descriptors);
            lock.unlock();
            resource_cv.notify_all(); // Notify other processes waiting for resources
            break;
        }
    }
}

int main() {
    key_t shm_key = ftok("shmfile", SHM_KEY);
    int shmid = shmget(shm_key, sizeof(ResourceDescriptor) * NUM_RESOURCES, 0666 | IPC_CREAT);
    ResourceDescriptor* resource_descriptors = (ResourceDescriptor*)shmat(shmid, nullptr, 0);

    // Initialize resource descriptors
    for (int i = 0; i < NUM_RESOURCES; ++i) {
        resource_descriptors[i].total_instances = NUM_INSTANCES;
        resource_descriptors[i].available_instances = NUM_INSTANCES;
        for (int j = 0; j < MAX_USER_PROCESSES; ++j) {
            resource_descriptors[i].allocated_instances[j] = 0;
        }
    }

    std::vector<std::thread> user_processes;

    int children_created = 0;
    auto start_time = std::chrono::system_clock::now();

    while (children_created < MAX_CHILDREN &&
           std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start_time).count() <= MAX_RUNTIME) {
        if (user_processes.size() < MAX_USER_PROCESSES) {
            user_processes.emplace_back(user_process, user_processes.size(), resource_descriptors);
            children_created++;
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 500 + 1));
        }
    }

    for (auto& process : user_processes) {
        if (process.joinable()) {
            process.join();
        }
    }

    // Clean up shared memory
    shmdt(resource_descriptors
    shmctl(shmid, IPC_RMID, nullptr);

    return 0;
}