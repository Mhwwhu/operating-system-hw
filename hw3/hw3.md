# 第三次作业
马昊文 2022302111138
## 问题一
1. 算法一：
	- 算法一满足有限等待，因为每一个线程是轮转访问处理机的，这样就能保证每个线程在一次轮转中都获得一次处理机，不会无限等待。
	- 满足忙则等待，因为一个线程在没有轮到它的时候无法进入临界区。
	- 不满足空闲让进，因为这些线程必须按顺序轮流进入临界区。
	- 不满足让权等待，因为这些进程在无法进入临界区的时候会继续占用处理机轮询能否访问。
2. 算法二：
	- 不满足忙则等待，可以看到开启10个线程时，最终的workaround小于所有count之和，这意味着同时存在多个线程进入了临界区。从代码逻辑分析，
		```cpp 
		size_t other = (tid + 1) % total_thread;
		while (flag[other].load());
		flag[tid].store(true);
		workround++;
		flag[tid].store(false);
		```
		因为只要tid为other的线程不在临界区，其他线程便可访问临界区，故不满足忙则等待。
	- 不满足让权等待，原因同算法一。
	- 满足有限等待，因为该系统同一时刻至少存在一个线程不在临界区，不会产生死锁，而且每个线程访问临界区时间有限，只要一个线程的下一个线程离开临界区，它就可以进入。
	- 满足空闲让进，因为临界区没有线程的时候，flag全部为false，此时只要有线程申请进入临界区便可被允许。
3. 算法三
	- 不满足有限等待和空闲让进。因为可能存在死锁，且死锁时所有线程有可能都未进入临界区。
	- 不满足让权等待，原因同算法一
	- 不满足忙则等待，因为只要tid为other的线程不在临界区，其他线程便可能访问临界区，存在多个线程同时访问临界区的情况。
4. 算法四
	- 不满足忙则等待，因为只要当前线程的other的flag为false，就可以进入临界区，存在个线程同时访问临界区的情况。
	- 满足空闲让进，当所有线程都不在临界区时，每个线程都有进入临界区的机会，且一定有线程能够进入临界区。
	- 不满足让权等待，原因同算法一
	- 满足有限等待，因为每个线程访问临界区的时间有限，只要当前线程的other线程离开临界区或其他后来的线程也准备进入临界区，当前线程便可进入临界区。
## 问题二
1. 算法二 vs 算法四
	
	当线程数为2时，算法四成为Peterson算法，而算法二存在两个线程同时进入临界区的可能。二者在多余两个线程的情况下均不能正常工作，但是对于双线程的场景，算法四更好。

2. 适应多线程的算法
	```cpp
	int level[total_thread];
	int wating[total_thread];
	void myAlg()
	{
		memset(level, -1, sizeof(level));
		memset(wating, -1, sizeof(wating));
		for (size_t i = 0; i < total_thread; i++) flag[i].store(false);
		std::vector<std::thread> workers;
		uint64_t start = get_sys_clock();
		int totalCount = 0;
		for (int i = 0; i < total_thread; i++) {
			workers.emplace_back(std::thread([&](int tid) {
				ping(tid);
				size_t count = 0;
				while (true) {
					for (int lev = 0; lev < total_thread; lev++)
					{
						level[tid] = lev;
						wating[lev] = tid;
						while (wating[lev] == tid)
						{
							int j;
							for (j = 0; j < total_thread; j++) {
								if (level[j] >= lev && j != tid) break;
								if (wating[lev] != tid) break;
							}
							if (j == total_thread) break;
							usleep(0);
						}
					}
					workround++;
					level[tid] = -1;
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
	```
	该算法使用了等待队列，申请进入临界区的线程先进入等待队列，当没有其他线程在此线程之前或者有线程挤占此线程在队列中的位置时，将该线程向队首移动，直到移出队列进入临界区。
	
	该算法满足有限等待、空闲让进、忙则等待和让权等待。其中让权等待是通过usleep(0)实现的。