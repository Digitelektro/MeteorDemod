#include "threadpool.h"

ThreadPool::ThreadPool(int numOfThreads)
    : mNumberOfThreads(numOfThreads)
    , mIsRuning(false) {}

ThreadPool::~ThreadPool() {
    if(mIsRuning) {
        stop();
    }
}

void ThreadPool::start() {
    mIsRuning = true;

    for(int i = 0; i < mNumberOfThreads; i++) {
        mThreads.push_back(std::thread(&ThreadPool::threadLoop, this));
    }
}

void ThreadPool::stop() {
    waitForAllJobsDone();

    {
        std::unique_lock<std::mutex> lock(mQueueMutex);
        mIsRuning = false;
    }

    mConditionVariable.notify_all(); // wake up all threads.

    for(std::thread& th : mThreads) {
        if(th.joinable()) {
            th.join();
        }
    }

    mThreads.clear();
}

void ThreadPool::addJob(JobFunction_t task) {
    {
        std::unique_lock<std::mutex> lock(mQueueMutex);
        mJobCounter++;
        mJobsQueue.push(task);
    }

    mConditionVariable.notify_one();
}

void ThreadPool::threadLoop() {
    JobFunction_t job;

    while(true) {
        {
            std::unique_lock<std::mutex> lock(mQueueMutex);

            mConditionVariable.wait(lock, [this]() {
                return !mJobsQueue.empty() || !mIsRuning;
            });

            if(!mIsRuning) {
                break;
            }

            job = mJobsQueue.front();
            mJobsQueue.pop();
        }
        job();

        mJobCounter--;
    }
}
