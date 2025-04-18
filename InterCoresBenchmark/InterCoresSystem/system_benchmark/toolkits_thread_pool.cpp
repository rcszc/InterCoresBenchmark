// easy_workspool.
#include <chrono>
#include "toolkits_thread_pool.hpp"

using namespace std;

namespace SystemThreads {
    size_t ThisThreadID() {
        size_t ReturnThreadID = NULL;

        thread::id ThreadID = this_thread::get_id();
        hash<thread::id> Hasher;
        ReturnThreadID = Hasher(ThreadID);

        return ReturnThreadID;
    }

    void SystemThreadsPool::ThreadWorkersExecution(uint32_t workers_num) {
        // start threads(workers).
        for (size_t i = 0; i < workers_num; ++i) {
            ThreadWorkers.emplace_back([this] {
                // loop execution task.
                while (true) {
                    function<void()> WorkTaskExecution;
                    unique_lock<mutex> Lock(PoolMutex);

                    WorkersCondition.wait(Lock, [this] { return STOP_POOL_FLAG || !PoolTasks.empty(); });
                    if (STOP_POOL_FLAG && PoolTasks.empty()) break; // loop exit.
                    WorkTaskExecution = move(PoolTasks.front());
                    // queue => delete task.
                    PoolTasks.pop();

                    ++WorkingThreadsCount;
                    WorkTaskExecution();
                    --WorkingThreadsCount;
                }
            });
            size_t ThreadID = (size_t)hash<thread::id>{}(ThreadWorkers.back().get_id());
            PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_POOL,
                "create execution worker, thread hash: %u", ThreadID);
        }
    }

    void SystemThreadsPool::ThreadWorkersClose() {
        {
            unique_lock<mutex> Lock(PoolMutex);
            STOP_POOL_FLAG = true;
        }
        try {
            WorkersCondition.notify_all();
            for (thread& Worker : ThreadWorkers) {
                // close all workers(threads).
                Worker.join();
            }
        }
        catch (exception& err) {
            PSAG_LOGGER::PushLogger(LogError, SYSTEM_LOG_TAG_POOL,
                "close execution workers, err info: %s", err.what());
        }
    }

    uint32_t SystemThreadsPool::GetWorkingThreadsCount() {
        return WorkingThreadsCount;
    }

    uint32_t SystemThreadsPool::GetTaskQueueCount() {
        uint32_t TasksCount = NULL;
        {
            unique_lock<mutex> Lock(PoolMutex);
            TasksCount = (uint32_t)PoolTasks.size();
        }
        return TasksCount;
    }

    void SystemThreadsPool::ResizeWorkers(uint32_t resize) {
        {
            unique_lock<mutex> Lock(PoolMutex);
            WorkersCondition.notify_all();
        }
        ThreadWorkersClose();

        ThreadWorkers.resize(resize);
        STOP_POOL_FLAG = false;
        ThreadWorkersExecution(resize);
    }
}