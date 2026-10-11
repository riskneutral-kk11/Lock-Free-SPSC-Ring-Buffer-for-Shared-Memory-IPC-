#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <thread>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include "types.hpp"
#include "SPSCqueue.hpp"

constexpr const char* SHM_NAME = "/spsc_queue";
constexpr size_t RING_CAPACITY = 65536;

void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "[Warning] Failed to set CPU affinity to core " << core_id << "\n";
    }
}


std::vector<ring::Tick> parse_csv_trace(const std::string& filepath){
    std::vector<ring::Tick> ticks;
    std::ifstream file(filepath);
    std::string line;
    if (!std::getline(file, line)) return ticks;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        ring::Tick t{};
        std::getline(ss, item, ',');
        t.timestamp_ns = std::stoull(item);
        std::getline(ss, item, ',');
        std::memcpy(t.symbol, item.data(), 4);
        std::getline(ss, item, ',');
        t.bid_px = std::stoi(item);
        std::getline(ss, item, ',');
        t.ask_px = std::stoi(item);
        std::getline(ss, item, ',');
        t.bid_qty = std::stoul(item);
        std::getline(ss, item, ',');
        t.ask_qty = std::stoul(item);
        ticks.push_back(t);
    }
    return ticks;
}



int main(int argc, char* argv[]){
    std::string trace_file;
    if (argc > 1) {
        trace_file = argv[1];
    }else{
        std::cout << "Error : Trace File Not Specified . Exiting \n";
        return 1;
    }

    shm_unlink(SHM_NAME); // Unlinks previous memory(if exists)
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    size_t shm_size = sizeof(SPSC_Queue<RING_CAPACITY>);
    auto result = ftruncate(shm_fd, shm_size); // Resizing the Shared Memory so as to fit the Queue Object;
    if(result == -1){
        std::cout << "Error : Ftruncate Failed - Exiting \n";
        return 1;
    }
    void* shm_ptr = mmap(nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); // Mapping it
    auto* queue = new (shm_ptr) SPSC_Queue<RING_CAPACITY>(); // Placing the SPSC Queue in /dev/shm

    // Pre-loading trace into RAM before starting timers so as to avoid wasting time on hot path;
    auto trace = parse_csv_trace(trace_file);
    if (trace.empty()) {
        std::cerr << "Error: Trace is empty - (Check if Filename is Correct) Exiting\n. ";
        munmap(shm_ptr, shm_size);
        shm_unlink(SHM_NAME);
        return 1;
    }

    auto start_time = std::chrono::high_resolution_clock::now(); // Starting Benchmark time;

    // Spawning Consumer Process;
    pid_t consumer_pid = fork();
    if (consumer_pid == 0) {
        pin_to_core(2); 
        
        ring::Tick tick{};
        size_t currentPops = 0;
        size_t totalPops = trace.size();

        while (currentPops < totalPops) {
            if (queue->pop(tick)) {
                currentPops++;
            }
        }
        _exit(0);
    }

    // Spawning Producer Process
    pid_t producer_pid = fork();
    if (producer_pid == 0) {
        pin_to_core(1); // Pin Producer to Physical Core 1
        for (const auto& tick : trace) {
            while (!queue->push(tick)){}
        }
        _exit(0);
    }

    int status = 0;
    waitpid(producer_pid, &status, 0); // Waiting for both the processes to finish;
    waitpid(consumer_pid, &status, 0);

    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    double million_ops = static_cast<double>(trace.size()) / 1'000'000.0;
    double Mops_sec = million_ops / (elapsed_ms / 1000.0);
    std::cout << "Total Ticks Processed : " << trace.size() << "\n";
    std::cout << "Total Execution Time  : " << elapsed_ms << " ms\n";
    std::cout << "Throughput            : " << Mops_sec << " Million Ticks/sec\n";
    std::cout << "Avg Latency / Tick    : " << (elapsed_ms * 1e6) / trace.size() << " ns\n";

    // Clean Up .
    munmap(shm_ptr, shm_size);
    shm_unlink(SHM_NAME);
    close(shm_fd);

    return 0;
    return 0;
}
