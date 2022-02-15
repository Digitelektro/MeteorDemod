#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <chrono>
#include <iostream>


class ThreadPool
{
private:
    class JobCounter {
    public:
        JobCounter (int count_ = 0)
            : mCount(count_)
        {
        }

        void operator++(int) {
            std::unique_lock<std::mutex> lock(mMutex);
            mCount++;
            mCv.notify_one();
        }

        void operator--(int) {
            std::unique_lock<std::mutex> lock(mMutex);
            if(mCount > 0) {
                mCount--;
            }
            mCv.notify_one();
        }

        void wait() {
            std::unique_lock<std::mutex> lock(mMutex);
            while(mCount > 0) {
                mCv.wait(lock);
            }
        }

        int getCount() const {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCount;
        }

    private:
        mutable std::mutex mMutex;
        std::condition_variable mCv;
        int mCount;
    };

public:
    typedef std::function<void()> JobFunction_t;

public:
    ThreadPool(int numOfThreads);
    ~ThreadPool();

    ThreadPool(const ThreadPool &other) = delete;
    ThreadPool(ThreadPool &&other) = delete;
    ThreadPool &operator=(const ThreadPool &other) = delete;
    ThreadPool &operator=(ThreadPool &&other) = delete;

    void start();
    void stop();
    void waitForAllJobsDone(){
        mJobCounter.wait();
    };

    void addJob(JobFunction_t task);

    template<typename T>
    void addJob(void(T::*handler)(), T *instance) {
        addJob(std::bind(handler, instance));
    }

    int jobsInProgress() const {
        return mJobCounter.getCount();
    }

private:
    int mNumberOfThreads;
    bool mIsRuning;
    std::vector<std::thread> mThreads;
    std::queue<JobFunction_t> mJobsQueue;
    std::mutex mQueueMutex;
    std::condition_variable mConditionVariable;
    JobCounter mJobCounter;

private:
    void threadLoop();
};

class ThreadPoolTest {
public:
    ThreadPoolTest() : mThreadPool(std::thread::hardware_concurrency()) {

    }

    void start() {
        mThreadPool.start();
        for(int i = 0; i < 20; i++) {
            mThreadPool.addJob([i](){
                std::cout << "Thread started :" << i << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                std::cout << "Thread end :" << i << std::endl;
            });
        }
        mThreadPool.waitForAllJobsDone();
    }

    void finish() {
        mThreadPool.stop();
    }

private:
    ThreadPool mThreadPool;

};

#endif // THREADPOOL_H
