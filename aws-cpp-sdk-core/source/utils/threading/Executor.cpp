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

#include <aws/core/utils/threading/Executor.h>
#include <aws/core/utils/threading/ThreadTask.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/utils/logging/LogSystemInterface.h>
#include <thread>
#include <cassert>
#include <chrono>
#include <cstdio>

static const char* POOLED_CLASS_TAG = "PooledThreadExecutor";

using namespace Aws::Utils::Threading;

bool DefaultExecutor::SubmitToThread(std::chrono::time_point<std::chrono::high_resolution_clock> time, std::function<void()>&&  fx)
{
	using namespace std::chrono;
	using namespace std::chrono_literals;
	using CLOCK = std::chrono::high_resolution_clock;
	auto delay = duration_cast<milliseconds>(time - CLOCK::now());
    auto main = [delay, fx, this] { 
		if (delay > 0ms) {
			std::this_thread::sleep_for(delay);
		}
        fx(); 
        Detach(std::this_thread::get_id()); 
    };

    State expected;
    do
    {
        expected = State::Free;
        if(m_state.compare_exchange_strong(expected, State::Locked))
        {
            std::thread t(main);
            const auto id = t.get_id(); // copy the id before we std::move the thread
            m_threads.emplace(id, std::move(t));
            m_state = State::Free;
            return true;
        }
    }
    while(expected != State::Shutdown);
    return false;
}

void DefaultExecutor::Detach(std::thread::id id)
{
    State expected;
    do
    {
        expected = State::Free;
        if(m_state.compare_exchange_strong(expected, State::Locked))
        {
            auto it = m_threads.find(id);
            assert(it != m_threads.end());
            it->second.detach();
            m_threads.erase(it);
            m_state = State::Free;
            return;
        }
    } 
    while(expected != State::Shutdown);
}

DefaultExecutor::~DefaultExecutor()
{
    auto expected = State::Free;
    while(!m_state.compare_exchange_strong(expected, State::Shutdown))
    {
        //spin while currently detaching threads finish
        assert(expected == State::Locked);
        expected = State::Free; 
    }

    auto it = m_threads.begin();
    while(!m_threads.empty())
    {
        it->second.join();
        it = m_threads.erase(it);
    }
}

PooledThreadExecutor::PooledThreadExecutor(size_t poolSize, OverflowPolicy overflowPolicy) :
    m_sync(0, poolSize), m_poolSize(poolSize), m_overflowPolicy(overflowPolicy)
{
    for (size_t index = 0; index < m_poolSize; ++index)
    {
        m_threadTaskHandles.push_back(Aws::New<ThreadTask>(POOLED_CLASS_TAG, *this));
    }
}

PooledThreadExecutor::~PooledThreadExecutor()
{
    for(auto threadTask : m_threadTaskHandles)
    {
        threadTask->StopProcessingWork();
    }

    m_sync.ReleaseAll();

    for (auto threadTask : m_threadTaskHandles)
    {
        Aws::Delete(threadTask);
    }

    while(m_submitTasks.size() > 0)
    {
		SubmitTask* task = m_submitTasks.top();
		m_submitTasks.pop();

        if(task)
        {
			Aws::Delete(task->func);
            Aws::Delete(task);
        }
    }

}

bool PooledThreadExecutor::SubmitToThread(std::chrono::time_point<std::chrono::high_resolution_clock> time, std::function<void()>&& fn)
{
    //avoid the need to do copies inside the lock. Instead lets do a pointer push.
	auto t = time;
	SubmitTask* task = Aws::New<SubmitTask>(POOLED_CLASS_TAG);
	task->func = Aws::New<std::function<void()>>(POOLED_CLASS_TAG, std::forward<std::function<void()>>(fn));
	task->time = time;

    {
        std::lock_guard<std::mutex> locker(m_queueLock);

        if (m_overflowPolicy == OverflowPolicy::REJECT_IMMEDIATELY && m_submitTasks.size() >= m_poolSize)
        {
            Aws::Delete(fnCpy);
            return false;
        }

		m_submitTasks.push(task);
    }

    m_sync.Release();

    return true;
}

SubmitTask* PooledThreadExecutor::PopTask()
{
    std::lock_guard<std::mutex> locker(m_queueLock);

    if (m_submitTasks.size() > 0)
    {
        auto task = m_submitTasks.top();
        if (task)
        {           
			m_submitTasks.pop();
            return task;
        }
    }

    return nullptr;
}

SubmitTask * Aws::Utils::Threading::PooledThreadExecutor::PeekTask()
{
    std::lock_guard<std::mutex> locker(m_queueLock);

    if (m_submitTasks.size() > 0)
    {
        auto task = m_submitTasks.top();
        if (task)
        {           
            return task;
        }
    }

    return nullptr;
}


bool PooledThreadExecutor::HasTasks()
{
    std::lock_guard<std::mutex> locker(m_queueLock);
    return m_submitTasks.size() > 0;
}

bool Aws::Utils::Threading::operator<(const SubmitTask & task1, const SubmitTask & task2)
{
	// Aws::Utils::Logging::GetLogSystem()->Log(Aws::Utils::Logging::LogLevel::Info, "MAIN", "Compare");
	// printf("SubmitTask compare: test\n");
	return task1.time < task2.time;
}
