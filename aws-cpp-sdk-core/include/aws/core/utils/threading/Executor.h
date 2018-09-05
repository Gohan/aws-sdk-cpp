/*
  * Copyright 2010-2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
  *
  * Licensed under the Apache License, Version 2.0 (the "License").
  * You may not use this file except in compliance with the License.
  * A copy of the License is located at
  *
  *  http://aws.amazon.com/apache2.0
  *
  * or in the "license" file accompanying this file. This file is distributed
  * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
  * express or implied. See the License for the specific language governing
  * permissions and limitations under the License.
  */

#pragma once

#include <aws/core/Core_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSQueue.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/core/utils/memory/stl/AWSMap.h>
#include <aws/core/utils/threading/Semaphore.h>
#include <functional>
#include <future>
#include <mutex>
#include <atomic>
#include <chrono>

namespace Aws
{
    namespace Utils
    {
        namespace Threading
        {
            class ThreadTask;

            /**
            * Interface for implementing an Executor, to implement a custom thread execution strategy, inherit from this class
            * and override SubmitToThread().
            */
            class AWS_CORE_API Executor
            {
            public:                
                virtual ~Executor() = default;

                /**
                 * Send function and its arguments to the SubmitToThread function.
                 */
                template<class Fn, class ... Args>
                bool Submit(Fn&& fn, Args&& ... args)
                {
					using CLOCK = std::chrono::high_resolution_clock;
					auto now = CLOCK::now();
                    std::function<void()> callable{ std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...) };
                    return SubmitToThread(now, std::move(callable));
                }

                template<class Fn, class ... Args>
				bool SubmitDelay(std::chrono::milliseconds delay, Fn&& fn, Args&& ... args) {
					using CLOCK = std::chrono::high_resolution_clock;
					auto now = CLOCK::now();
                    std::function<void()> callable{ std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...) };
                    return SubmitToThread(now+delay, std::move(callable));
				}

            protected:
                /**
                * To implement your own executor implementation, then simply subclass Executor and implement this method.
                */
                virtual bool SubmitToThread(std::chrono::time_point<std::chrono::high_resolution_clock>, std::function<void()>&&) = 0;
            };


            /**
            * Default Executor implementation. Simply spawns a thread and detaches it.
            */
            class AWS_CORE_API DefaultExecutor : public Executor
            {
            public:
                DefaultExecutor() : m_state(State::Free) {}
                ~DefaultExecutor();
            protected:
                enum class State
                {
                    Free, Locked, Shutdown
                };
                bool SubmitToThread(std::chrono::time_point<std::chrono::high_resolution_clock>, std::function<void()>&&) override;
                void Detach(std::thread::id id);
                std::atomic<State> m_state;
                Aws::UnorderedMap<std::thread::id, std::thread> m_threads;
            };

            enum class OverflowPolicy
            {
                QUEUE_TASKS_EVENLY_ACCROSS_THREADS,
                REJECT_IMMEDIATELY
            };

			struct SubmitTask {
				std::function<void()>* func;
				std::chrono::time_point<std::chrono::high_resolution_clock> time;
			};
			template<typename Type, typename Compare = std::less<Type> >
			struct PointerLess : public std::binary_function<Type *, Type *, bool> {
				bool operator()(const Type *x, const Type *y) const
				{
					return Compare()(*x, *y);
				}
			};
            /**
            * Thread Pool Executor implementation.
            */
            class AWS_CORE_API PooledThreadExecutor : public Executor
            {
            public:
                PooledThreadExecutor(size_t poolSize, OverflowPolicy overflowPolicy = OverflowPolicy::QUEUE_TASKS_EVENLY_ACCROSS_THREADS);
                ~PooledThreadExecutor();

                /**
                * Rule of 5 stuff.
                * Don't copy or move
                */
                PooledThreadExecutor(const PooledThreadExecutor&) = delete;
                PooledThreadExecutor& operator =(const PooledThreadExecutor&) = delete;
                PooledThreadExecutor(PooledThreadExecutor&&) = delete;
                PooledThreadExecutor& operator =(PooledThreadExecutor&&) = delete;

            protected:
                bool SubmitToThread(std::chrono::time_point<std::chrono::high_resolution_clock>, std::function<void()>&&) override;

            private:
                // Aws::Queue<std::function<void()>*> m_tasks;
                Aws::PriorityQueue<SubmitTask*, Vector<SubmitTask*>, PointerLess<SubmitTask>> m_submitTasks;
                std::mutex m_queueLock;
                Aws::Utils::Threading::Semaphore m_sync;
                Aws::Vector<ThreadTask*> m_threadTaskHandles;
                size_t m_poolSize;
                OverflowPolicy m_overflowPolicy;

                /**
                 * Once you call this, you are responsible for freeing the memory pointed to by task.
                 */
                // std::function<void()>* PopTask();
				SubmitTask* PeekTask();
				SubmitTask* PopTask();
                void AddTaskBack(SubmitTask* task);
                bool HasTasks();

                friend class ThreadTask;
            };
			inline bool operator < (const SubmitTask& task1, const SubmitTask& task2);
        } // namespace Threading
    } // namespace Utils
} // namespace Aws
