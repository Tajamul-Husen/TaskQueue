#pragma once

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <optional>


namespace TaskQueue
{
    template <typename T>
    class Queue
    {
    public:
        Queue() = default;
        ~Queue() = default;

        Queue(Queue& queue) = delete;
        Queue(const Queue& queue) = delete;

        void Enqueue(T item)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Queue.push(item);
            m_ConditionVariable.notify_one();
        };

        std::optional<T> Dequeue()
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_ConditionVariable.wait(lock, [&](){ return m_UnblockDequeue || !m_Queue.empty(); });

            if (m_UnblockDequeue || m_Queue.empty()) return {};

            T item = m_Queue.front();
            m_Queue.pop();
            return item;
        };

        bool IsEmpty()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Queue.empty();
        };

        size_t Size()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Queue.size();
        };

        void Unblock()
        {
            if (m_UnblockDequeue) return;

            std::unique_lock<std::mutex> lock(m_Mutex);
            m_UnblockDequeue = true;
            m_ConditionVariable.notify_all();
        }

    private:
        std::queue<T> m_Queue;
        std::mutex m_Mutex;
        std::condition_variable m_ConditionVariable;
        bool m_UnblockDequeue = false;
    };

    template <typename T>
    class Worker
    {
    public:
        Worker() = default;

        Worker(Queue<T> &queue) : m_Queue(queue)
        {
            m_Thread = std::thread(&Worker::Process, this);
        };

        ~Worker()
        {
            Sync();
        };

        void Sync()
        {
            m_Stopped = true;
            m_Queue.Unblock();
            Join();
        }

        bool IsIdle()
        {
            return m_Idle;
        }

    private:
        void Join()
        {
            if (m_Thread.joinable())
                m_Thread.join();
        }

        void Process()
        {
            m_Stopped = false;
            std::thread::id threadId = std::this_thread::get_id();

            while (!m_Stopped)
            {
                m_Idle = true;

                std::cout << "[" << threadId << "][Waiting]\n";
                auto item = m_Queue.Dequeue();

                if (m_Stopped) break;

                if (!item.has_value()) break;

                std::cout << "[" << threadId << "][Executing]\n";
                m_Idle = false;

                auto task = item.value();
                task();

                std::cout << "[" << threadId << "][Finished]\n";
            };

            std::cout << "[" << threadId << "][Synced]\n";
        }

    private:
        Queue<T> &m_Queue;
        std::thread m_Thread;
        std::atomic<bool> m_Idle{true};
        static bool m_Stopped;
    };

    template <typename T>
    bool Worker<T>::m_Stopped = true;

    class WorkerPool
    {
        using Task = std::function<void(void)>;

    public:
        WorkerPool() = default;

        WorkerPool(uint8_t poolSize)
        {
            m_TotalWorkerCount = std::min(poolSize, (uint8_t)std::thread::hardware_concurrency());
            m_Workers.reserve(m_TotalWorkerCount);

            for (uint8_t i = 0; i < m_TotalWorkerCount; i++)
                m_Workers.emplace_back(std::make_unique<Worker<Task>>(m_Queue));
        };

        ~WorkerPool()
        {
            Sync();
        };

        template <typename TaskFunc, typename... Args>
        void PostTask(TaskFunc task, Args... args)
        {
            m_Queue.Enqueue(std::bind(std::forward<TaskFunc>(task), std::forward<Args>(args)...));
        }

        void Sync()
        {
            for (auto &worker : m_Workers)
                worker->Sync();
        }

        uint8_t GetTotalWorker() { return m_TotalWorkerCount; };

        uint8_t GetActiveWorker()
        {
            uint8_t count = 0;
            for (auto &worker : m_Workers)
                count = count + ((worker->IsIdle()) ? 0 : 1);

            return count;
        };

    private:
        std::vector<std::unique_ptr<Worker<Task>>> m_Workers;
        Queue<Task> m_Queue;
        uint8_t m_TotalWorkerCount = 0;
    };

};

