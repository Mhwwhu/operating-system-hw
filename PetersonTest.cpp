#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <unistd.h>
#include <numa.h>
#include "mcslocks.h"

#define CPU_FREQ 2.6

const size_t total_thread = 10;

class atomic_bool {
	alignas(64) std::atomic<bool> bv{ false };

public:
	void store(bool v) { bv.store(v); }

	bool load() { return bv.load(); }
};

class atomic_int {
	alignas(64) std::atomic<int> bi{ 0 };

public:
	void store(int i) { bi.store(i); }

	int load() { return bi.load(); }
};

#define  align_type 0

#if align_type
atomic_bool flag[total_thread], terminated;
atomic_int turn;
#else
std::atomic<bool> flag[total_thread], terminated{ false };
std::atomic<int> turn{ 0 };
#endif

void ping(int id) {
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(id, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

std::atomic<uint64_t> atomicwork{ 0 };

uint64_t workround = 0;

std::mutex outlock;

uint64_t get_sys_clock() {
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	uint64_t ret = ((uint64_t)lo) | (((uint64_t)hi) << 32);
	return (uint64_t)((double)ret / CPU_FREQ);
}

void simpleAlg1() {
	std::vector<std::thread> workers;
	uint64_t start = get_sys_clock();
	for (int i = 0; i < total_thread; i++) {
		workers.emplace_back(std::thread([](int tid) {
			ping(tid);
			size_t count = 0;
			while (true) {
				while (turn.load() != tid);
				workround++;
				turn.store((tid + 1) % total_thread);
				// usleep(10000);
				// usleep(100000);
				// std::cout << tid << " " << workround << std::endl;
				if (++count % 100 == 0 && terminated.load()) break;
			}
			outlock.lock();
			std::cout << tid << " " << count << std::endl;
			outlock.unlock();
			}, i));
	}
	while (get_sys_clock() - start < 3000000000) usleep(10000);
	terminated.store(true);
	for (int i = 0; i < total_thread; i++) workers[i].join();
	std::cout << "* " << workround << std::endl;
}

void simpleAlg2() {
	for (size_t i = 0; i < total_thread; i++) flag[i].store(false);
	std::vector<std::thread> workers;
	uint64_t start = get_sys_clock();
	int totalCount = 0;
	for (int i = 0; i < total_thread; i++) {
		workers.emplace_back(std::thread([&](int tid) {
			ping(tid);
			size_t count = 0;
			while (true) {
				size_t other = (tid + 1) % total_thread;
				while (flag[other].load());
				flag[tid].store(true);
				workround++;
				flag[tid].store(false);
				// usleep(100000);
				// std::cout << tid << " " << workround << std::endl;
				if (++count % 10000 == 0 && terminated.load()) break;
			}
			outlock.lock();
			std::cout << tid << " " << count << std::endl;
			totalCount += count;
			outlock.unlock();
			}, i));
	}
	while (get_sys_clock() - start < 3000000000) usleep(10000);
	terminated.store(true);
	for (int i = 0; i < total_thread; i++) workers[i].join();
	std::cout << "* " << workround << std::endl;
	std::cout << "* " << totalCount << std::endl;
}

void simpleAlg3() {
	for (size_t i = 0; i < total_thread; i++) flag[i].store(false);
	std::vector<std::thread> workers;
	uint64_t start = get_sys_clock();
	for (int i = 0; i < total_thread; i++) {
		workers.emplace_back(std::thread([](int tid) {
			ping(tid);
			size_t count = 0;
			while (true) {
				size_t other = (tid + 1) % total_thread;
				flag[tid].store(true);
				while (flag[other].load());
				workround++;
				flag[tid].store(false);
				// usleep(100000);
				// std::cout << tid << " " << workround << std::endl;
				if (++count % 100 == 0 && terminated.load()) break;
			}
			outlock.lock();
			std::cout << tid << " " << count << std::endl;
			outlock.unlock();
			}, i));
	}
	while (get_sys_clock() - start < 3000000000) usleep(10000);
	terminated.store(true);
	for (int i = 0; i < total_thread; i++) workers[i].join();
	std::cout << "* " << workround << std::endl;
}

void simpleAlg4() {
	for (size_t i = 0; i < total_thread; i++) flag[i].store(false);
	std::vector<std::thread> workers;
	uint64_t start = get_sys_clock();
	int totalCount = 0;
	for (int i = 0; i < total_thread; i++) {
		workers.emplace_back(std::thread([&](int tid) {
			ping(tid);
			size_t count = 0;
			while (true) {
				int other = (tid + 1) % total_thread;
				flag[tid].store(true);
				turn.store(other);
				while (flag[other].load() && turn.load() == other);
				workround++;
				flag[tid].store(false);
				// usleep(100000);
				// std::cout << tid << " " << workround << std::endl;
				if (++count % 100 == 0 && terminated.load()) break;
			}
			outlock.lock();
			std::cout << tid << " " << count << std::endl;
			totalCount += count;
			outlock.unlock();
			}, i));
	}
	while (get_sys_clock() - start < 3000000000) usleep(10000);
	terminated.store(true);
	for (int i = 0; i < total_thread; i++) workers[i].join();
	std::cout << "* " << workround << std::endl;
	std::cout << "* " << totalCount << std::endl;
}

static const uint64_t timetotal = 30000000000LLU;

const int philo_count = 40;

thread_local int thd_id;

thread_local size_t prcd = 0, done = 0;

std::atomic<uint64_t> indicator{ philo_count / 2 };

class spinlock {
	std::atomic<bool> release{ false };

public:
	spinlock() : release(false) {}

	bool lock() {
		//    bool released;
		//    do {
		//      released = release.exchange(true);
		//    } while(released);
		//    return true;
		int turn = indicator.load() % philo_count;
		// if(done > 0 && turn != thd_id) usleep(1);
		bool released; //= release.load();
		done = 0;
		//if(thd_id == 2) std::cout << "\t\t" << prcd << std::endl;
		do {
			int expect = done;
			if (turn != thd_id) while (expect--) usleep(1);
			while (released = release.load()) if (turn != thd_id) usleep(1);
			//          std::this_thread::yield();
			//        ;
			//        pthread_yield();
			assert(!released);
			done++;
		} while (!release.compare_exchange_strong(released, true));
		if (prcd++ % 100 == 0 && turn == thd_id) {
			indicator.fetch_add(1);
			//std::cout << "\t" << thd_id << " " << prcd << " " << indicator.load() << std::endl;
		}
		//if(thd_id == 2) std::cout << "\t\t" << prcd << std::endl;
		return true;
	}

	void unlock() {
		release.store(false);
	}
};

//std::mutex sticks[philo_count]; // 7.5 mops 40 threads
//pthread_mutex_t sticks[philo_count];
spinlock sticks[philo_count]; // 14.5 mops 40 threads
//mcs_lock_t sticks[philo_count];
//mcslock_t sticks[philo_count]; // 3.58 mops 40 threads

void philoTest() {
	std::vector<std::thread> workers;
	uint64_t start = get_sys_clock();
	//  for(int i = 0; i < philo_count; i++) mcslock_init(&sticks[i]);
	for (int i = 0; i < philo_count; i++) {
		workers.emplace_back(std::thread([](int tid) {
			thd_id = tid;
			ping(tid);
			size_t count = 0;
			while (true) {
				int mine = (tid) % philo_count;
				int other = (tid + 1) % philo_count;
				if (tid == 0) {
					int temp = mine;
					mine = other;
					other = temp;
				}
				/*pthread_mutex_lock(&sticks[mine]);
				pthread_mutex_lock(&sticks[other]);*/
				sticks[mine].lock();
				sticks[other].lock();
				/*mcslock_lock(&sticks[mine]);
				mcslock_lock(&sticks[other]);*/
				workround++;
				/*pthread_mutex_unlock(&sticks[mine]);
				pthread_mutex_unlock(&sticks[other]);*/
				sticks[mine].unlock();
				sticks[other].unlock();
				/*mcslock_unlock(&sticks[mine]);
				mcslock_unlock(&sticks[other]);*/
				if (++count % 10000 == 0 && terminated.load()) break;
			}
			outlock.lock();
			std::cout << tid << " " << count << std::endl;
			outlock.unlock();
			}, i));
	}
	while (get_sys_clock() - start < timetotal) usleep(10000);
	terminated.store(true);
	for (int i = 0; i < philo_count; i++) workers[i].join();
	std::cout << "* " << (double)workround * 1000 / timetotal << " " << indicator.load() << std::endl;
}

class Semaphore {
public:
	Semaphore(int count_ = 0)
		: count(count_) {
	}

	inline void notify(int tid) {
		std::unique_lock<std::mutex> lock(mtx);
		count++;
		// cout << "thread " << tid <<  " notify" << endl;
		// notify the waiting thread
		cv.notify_one();
	}

	inline void wait(int tid) {
		std::unique_lock<std::mutex> lock(mtx);
		while (count == 0) {
			// cout << "thread " << tid << " wait" << endl;
			//wait on the mutex until notify is called
			cv.wait(lock);
			// cout << "thread " << tid << " run" << endl;
		}
		count--;
	}

private:
	std::mutex mtx;
	std::condition_variable cv;
	int count;
};

int64_t current_size = 0;
const int64_t size_limit = 1000000000000LLU, pc_count = 20;
pthread_mutex_t lock;
pthread_cond_t empty, full;
//Semaphore empty{1000000}, full{0};

void prodConTest() {
	pthread_cond_init(&empty, nullptr);
	pthread_cond_init(&full, nullptr);

	std::vector<std::thread> workers;
	uint64_t start = get_sys_clock();
	for (int i = 0; i < pc_count; i++) {
		workers.emplace_back(std::thread([](int tid) {
			// producer
			ping(tid);
			int count = 0;
			while (true) {
				//        empty.wait(tid);
				pthread_mutex_lock(&lock);
				while (current_size >= size_limit && !terminated.load()) pthread_cond_wait(&empty, &lock);
				current_size++;
				workround++;
				pthread_cond_signal(&full);
				//        full.notify(tid);
				pthread_mutex_unlock(&lock);
				if (++count % 10000 == 0 && terminated.load()) break;
			}
			outlock.lock();
			std::cout << "producer: " << tid << " " << count << std::endl;
			outlock.unlock();
			}, i));
		workers.emplace_back(std::thread([](int tid) {
			// consumer
			ping(tid);
			size_t count = 0;
			while (true) {
				//        full.wait(tid);
				pthread_mutex_lock(&lock);
				while (current_size <= 0 && !terminated.load()) pthread_cond_wait(&full, &lock);
				current_size--;
				workround++;
				pthread_cond_signal(&empty);
				//        empty.notify(tid);
				pthread_mutex_unlock(&lock);
				if (++count % 10000 == 0 && terminated.load()) break;
			}
			outlock.lock();
			std::cout << "consumer: " << tid << " " << count << std::endl;
			outlock.unlock();
			}, pc_count + i));
	}
	while (get_sys_clock() - start < timetotal) usleep(10000);
	terminated.store(true);
	for (int i = 0; i < 2 * pc_count; i++) workers[i].join();
	std::cout << "* " << (double)workround * 1000 / timetotal << " " << current_size << std::endl;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cout << "com 0~3" << std::endl;
	}
	int n = 3;
	// n = std::atoi(argv[1]);
	switch (n) {
	case 0: {
		simpleAlg1();
		break;
	}
	case 1: {
		simpleAlg2();
		break;
	}
	case 2: {
		simpleAlg3();
		break;
	}
	case 3: {
		simpleAlg4();
		break;
	}
	case 4: {
		philoTest();
		break;
	}
	case 5: {
		prodConTest();
		break;
	}
	}
	return 0;
}