#include <iostream>
#include <random>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>

using namespace std;
using namespace std::chrono;

void generateArray(vector<int>& arr, int minVal = 0, int maxVal = 10000) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<int> dist(minVal, maxVal);
    for (auto &x : arr) {
        x = dist(gen);
    }
}

int main() {
    const int n = 10000000;  // array size
    const int num_threads = 32;
    
    vector<int> arr(n);
    generateArray(arr);
    
    // -------------------- Sequential version --------------------
    auto start_seq = high_resolution_clock::now();
    long long sum_even = 0;
    int max_even = -1;  // if no even numbers exist, remains -1
    for (int x : arr) {
        if (x % 2 == 0) {
            sum_even += x;
            if (x > max_even) {
                max_even = x;
            }
        }
    }
    // Calculate the "difference" using the formula: 2 * max_even - (sum of all even numbers)
    long long result_seq = 2LL * max_even - sum_even;
    auto end_seq = high_resolution_clock::now();
    duration<double> duration_seq = end_seq - start_seq;
    
    cout << "Sequential version:" << endl;
    cout << "  Result = " << result_seq << endl;
    cout << "  Maximum even number = " << max_even << endl;
    cout << "  Execution time = " << duration_seq.count() << " sec\n\n";
    
    // -------------------- Version with blocking primitives (mutex) --------------------
    auto start_mutex = high_resolution_clock::now();
    sum_even = 0;
    max_even = -1;
    mutex mtx;
    vector<thread> threads;
    
    // Divide the array into chunks for threads
    int chunk = n / num_threads;
    for (int t = 0; t < num_threads; t++) {
        int start_idx = t * chunk;
        // The last thread processes until the end of the array
        int end_idx = (t == num_threads - 1) ? n : start_idx + chunk;
        threads.push_back(thread([&arr, start_idx, end_idx, &mtx, &sum_even, &max_even]() {
            for (int i = start_idx; i < end_idx; i++) {
                int x = arr[i];
                if (x % 2 == 0) {
                    // Protect the critical section
                    lock_guard<mutex> lock(mtx);
                    sum_even += x;
                    if (x > max_even) {
                        max_even = x;
                    }
                }
            }
        }));
    }
    for (auto &th : threads) {
        th.join();
    }
    long long result_mutex = 2LL * max_even - sum_even;
    auto end_mutex = high_resolution_clock::now();
    duration<double> duration_mutex = end_mutex - start_mutex;
    
    cout << "Version with blocking primitives (mutex):" << endl;
    cout << "  Result = " << result_mutex << endl;
    cout << "  Maximum even number = " << max_even << endl;
    cout << "  Execution time = " << duration_mutex.count() << " sec\n\n";
    
    // -------------------- Version with atomic variables and CAS --------------------
    auto start_atomic = high_resolution_clock::now();
    atomic<long long> atomic_sum(0);
    atomic<int> atomic_max(-1);
    threads.clear();
    for (int t = 0; t < num_threads; t++) {
        int start_idx = t * chunk;
        int end_idx = (t == num_threads - 1) ? n : start_idx + chunk;
        threads.push_back(thread([&arr, start_idx, end_idx, &atomic_sum, &atomic_max]() {
            for (int i = start_idx; i < end_idx; i++) {
                int x = arr[i];
                if (x % 2 == 0) {
                    // Atomic addition
                    atomic_sum.fetch_add(x, memory_order_relaxed);
                    // Update maximum even number using CAS (compare_exchange_weak)
                    int current_max = atomic_max.load(memory_order_relaxed);
                    while (x > current_max &&
                           !atomic_max.compare_exchange_weak(current_max, x)) {
                        // current_max is updated to the current value of atomic_max
                    }
                }
            }
        }));
    }
    for (auto &th : threads) {
        th.join();
    }
    long long result_atomic = 2LL * atomic_max.load() - atomic_sum.load();
    auto end_atomic = high_resolution_clock::now();
    duration<double> duration_atomic = end_atomic - start_atomic;

    cout << "Version with atomic variables (CAS):" << endl;
    cout << "  Result = " << result_atomic << endl;
    cout << "  Maximum even number = " << atomic_max.load() << endl;
    cout << "  Execution time = " << duration_atomic.count() << " sec\n";

    return 0;
}
