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

#include <aws/core/utils/threading/ThreadTask.h>
#include <aws/core/utils/threading/Executor.h>
#include <chrono>

using namespace Aws::Utils;
using namespace Aws::Utils::Threading;

ThreadTask::ThreadTask(PooledThreadExecutor& executor) : m_continue(true), m_executor(executor), m_thread(std::bind(&ThreadTask::MainTaskRunner, this))
{
}

ThreadTask::~ThreadTask()
{
    StopProcessingWork();
    m_thread.join();
}

void ThreadTask::MainTaskRunner()
{
	using namespace std::chrono_literals;
	using CLOCK = std::chrono::high_resolution_clock;
    while (m_continue)
    {        
        while (m_continue && m_executor.HasTasks())
        {      
            auto task = m_executor.PopTask();
            if (task)// && CLOCK::now() >= task->time)
            {
				auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(task->time - CLOCK::now());
				if (delay <= 0ms) {
					(*(task->func))();
					Aws::Delete(task->func);               
					Aws::Delete(task);               
					continue;
                } else {
                    m_executor.AddTaskBack(task);
                }

				m_executor.m_sync.WaitOneTimeout(delay);
            }
        }
     
        if(m_continue)
        {
            m_executor.m_sync.WaitOne();
        }
    }
}

void ThreadTask::StopProcessingWork()
{
    m_continue = false;
}
