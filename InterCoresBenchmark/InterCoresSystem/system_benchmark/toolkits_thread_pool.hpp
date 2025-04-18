// toolkits_thread_pool.hpp 2025_04_11 RCSZ.
// scale async thread pool. 

#ifndef __TOOLKITS_THREAD_POOL_HPP
#define __TOOLKITS_THREAD_POOL_HPP
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <typeinfo>

#include "../psag_system_logger.hpp"

namespace SystemThreads {
    StaticStrLABEL SYSTEM_LOG_TAG_POOL = "THREADS_POOL";

    // run time type information.
    struct RTTI_OBJECT_INFO {
        std::string ObjectName;
        std::size_t ObjectHash;
    };

    // get this_thread uid hash.
    size_t ThisThreadID();

    template<typename nClass>
    inline RTTI_OBJECT_INFO __OBJECT_RTTI(nClass TASK_OBJ) {
        RTTI_OBJECT_INFO ReturnInfo = {};
        const std::type_info& GetInfo = typeid(TASK_OBJ);

        ReturnInfo.ObjectName = GetInfo.name();
        ReturnInfo.ObjectHash = GetInfo.hash_code();

        return ReturnInfo;
    }

    class SystemThreadsPool {
    protected:
        std::vector<std::thread>          ThreadWorkers;
        std::queue<std::function<void()>> PoolTasks;
        std::mutex                        PoolMutex;
        std::condition_variable           WorkersCondition;
        std::atomic<uint32_t>             WorkingThreadsCount{ NULL };

        void ThreadWorkersExecution(uint32_t workers_num);
        void ThreadWorkersClose();

        // current create object info.
        RTTI_OBJECT_INFO OBJECT_INFO = {};
        bool STOP_POOL_FLAG = false;
    public:
        SystemThreadsPool(uint32_t init_workers) {
            ThreadWorkersExecution(init_workers);
            PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_POOL, 
                "create thread pool workers, number: %u", init_workers);
        };
        ~SystemThreadsPool() {
            ThreadWorkersClose();
            PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_POOL, 
                "close thread pool workers.");
        };

        // threads pool: push => tasks queue.
        template<typename InClass, typename... ArgsParam>
        std::future<std::shared_ptr<InClass>> PushTask(ArgsParam... Args) {
            auto TaskObject = std::make_shared<std::packaged_task<std::shared_ptr<InClass>()>>(
                [Args = std::make_tuple(std::forward<ArgsParam>(Args)...), this]() mutable {
                    try {
                        return std::apply([](auto&&... Args) {
                            return std::make_shared<InClass>(std::forward<decltype(Args)>(Args)...);
                        }, std::move(Args));
                    }
                    catch (...) {
                        PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_POOL,
                            "failed create object, thread hash: %u", ThisThreadID());
                        return std::shared_ptr<InClass>(nullptr);
                    }
                });
            // create object => get object info.
            OBJECT_INFO = __OBJECT_RTTI(TaskObject);

            std::future<std::shared_ptr<InClass>> ResultFuture = TaskObject->get_future();
            {
                std::unique_lock<std::mutex> Lock(PoolMutex);
                if (STOP_POOL_FLAG) {
                    // disable push operate.
                    PSAG_LOGGER::PushLogger(LogInfo, SYSTEM_LOG_TAG_POOL,
                        "failed thread pool stop, thread hash: %u", ThisThreadID());
                    return ResultFuture;
                }
                PoolTasks.emplace([TaskObject]() { (*TaskObject)(); });
            }
            WorkersCondition.notify_one();
            return ResultFuture;
        }

        RTTI_OBJECT_INFO GetCurrentOBJI() {
            std::unique_lock<std::mutex> Lock(PoolMutex);
            return OBJECT_INFO;
        }

        uint32_t GetWorkingThreadsCount();
        uint32_t GetTaskQueueCount();
        void     ResizeWorkers(uint32_t resize);
    };
}
#endif