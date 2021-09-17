#include "JobSystemManager.h"
#include "Thread.h"
#include <thread>
#include <iostream>

namespace Insight::JS
{
	JobQueue::JobQueue(JobQueueOptions options)
		: m_highPriorityQueue(options.HighPriorityQueueSize)
		, m_normalPriorityQueue(options.NormalPriorityQueueSize)
		, m_lowPriorityQueue(options.LowPriorityQueueSize)
		, m_finishedQueue(options.JobFinishedQueue)
	{ }

	void JobQueue::Update(uint32_t const& jobsToFree)
	{
		uint32_t jobsFreed = 0;
		while (jobsFreed < jobsToFree)
		{
			++jobsFreed;
			JobSharedPtr finishedJob = nullptr;
			if (!m_finishedQueue.dequeue(finishedJob))
			{
				break;
			}
			finishedJob.reset();
		}
	}

	uint32_t JobQueue::GetPendingJobsCount() const
	{
		return m_highPriorityQueue.size() + m_normalPriorityQueue.size() + m_lowPriorityQueue.size();
	}

	void JobQueue::ScheduleJob(const JobSharedPtr job)
	{

	}

	void JobQueue::ScheduleJob(JobPriority priority, const JobSharedPtr & job, bool GetParentJob)
	{
		// Make sure we always schedule the top job in a list.
		JobSharedPtr jobToSchedule = job;
		// TODO: think about if we should propagate up jobs to get the root and enqueue that one.
		// or should the user explicitly schedule jobs.
		//if (GetParentJob)
		//{
		//	while (jobToSchedule->m_parentJob)
		//	{
		//		jobToSchedule = jobToSchedule->m_parentJob;
		//	}
		//}

		auto queue = GetQueueByPriority(priority);
		if (!queue)
		{
			return;
		}

		if (!queue->enqueue(jobToSchedule))
		{
			throw std::exception("Job Queue is full!");
		}
	}

	LockFreeQueue<JobSharedPtr>* JobQueue::GetQueueByPriority(JobPriority priority)
	{
		switch (priority)
		{
			case JobPriority::High:
				return &m_highPriorityQueue;

			case JobPriority::Normal:
				return &m_normalPriorityQueue;

			case JobPriority::Low:
				return &m_lowPriorityQueue;

			default:
				return nullptr;
		}
	}

	bool JobQueue::GetNextJob(JobSharedPtr& job)
	{
		if (job != nullptr)
		{
			//IJob* parentJob = job->m_parentJob;
			//// Check if parent job have move children 
			//if (parentJob)
			//{
			//	if (parentJob->m_currentChildJob < parentJob->m_childrenJobs.size())
			//	{
			//		// Job is a left job.
			//		job->SetState(JobState::Finished);
			//		job->ReleaseLock();
			//		assert(m_finishedQueue.enqueue(job) && "[JobSystemManager::GetNextJob] Job has finished. Finished queue is full.");
			//		//return parentJob->m_childrenJobs.at(parentJob->m_currentChildJob++);
			//	}
			//}

			for (uint16_t i = 0; i < job->m_childrenJobs.size(); ++i)
			{
				++job->m_currentChildJob;
				job->SetState(JobState::Waiting);
				//return job = job->m_childrenJobs[i];
				ScheduleJob(job->m_priority, job->m_childrenJobs[i], false);
			}

			job->SetState(JobState::Finished);
			job->ReleaseLock();
			assert(m_finishedQueue.enqueue(job) && "[JobSystemManager::GetNextJob] Job has finished. Finished queue is full.");
			job = nullptr;
		}

		// High Priority Jobs always come first
		if (m_highPriorityQueue.dequeue(job))
		{
			return true;
		}

		// Normal & Low Priority Jobs
		return	m_normalPriorityQueue.dequeue(job) ||
			m_lowPriorityQueue.dequeue(job);
	}


	/// <summary>
	/// JobSystem
	/// </summary>
	
	JobSystem::JobSystem()
	{
		//assert(false && "[JobSystem::JobSystem] JobSytem must be created from JobSystemManager using 'JobSystemManager::Instance()->CreateLocalJobSystem(numThreads)'.");
	}

	JobSystem::JobSystem(JobSystemManager* manager, std::vector<Thread*> threads, std::thread::id mainThreadId)
		: m_numThreads(static_cast<uint32_t>(threads.size()))
		, m_threads(std::move(threads))
		, m_mainThreadId(std::move(mainThreadId))
		, m_manager(manager)
	{
		for (Thread* t : m_threads)
		{
			ThreadData tData = t->GetUserdata();
			t->SetThreadData(tData.Manager, this);
		}
	}

	void JobSystem::Release()
	{
		//TODO: Give back all our threads to the main thread manager.
		m_manager->ReleaseJobSystem(*this);
	}

	void JobSystem::ScheduleJob(const JobSharedPtr job)
	{
		ScheduleJob(job->m_priority, job, false);
	}

	void JobSystem::ScheduleJob(JobPriority priority, const JobSharedPtr & job, bool GetParentJob)
	{
		m_queue.ScheduleJob(priority, job, GetParentJob);
	}

	void JobSystem::WaitForAll() const
	{ }

	void JobSystem::Update(uint32_t const& jobsToFree)
	{
		m_queue.Update(jobsToFree);
	}

	uint32_t JobSystem::GetPendingJobsCount() const
	{
		return m_queue.GetPendingJobsCount();
	}

	void JobSystem::Shutdown(bool blocking)
	{
		assert(std::this_thread::get_id() == GetMainThreadId() && "[JobSystemManager::Shutdown] Shutdown must be called on the 'MainThread'.");
		m_shuttingDown.store(true, std::memory_order_release);
		if (blocking)
		{
			for (uint8_t i = 0; i < m_numThreads; ++i)
			{
				m_threads[i]->Join();
			}
		}
		Update();
	}

	uint8_t JobSystem::GetCurrentThreadIndex() const
	{
		std::thread::id idx = std::this_thread::get_id();
		for (uint8_t i = 0; i < m_numThreads; ++i)
		{
			Thread& t = *m_threads[i];
			if (t.GetID() == idx)
			{
				return i;
			}
		}
		return UINT8_MAX;
	}

	Thread* JobSystem::GetCurrentThread() const
	{
		std::thread::id idx = std::this_thread::get_id();
		for (uint8_t i = 0; i < m_numThreads; ++i)
		{
			Thread& t = *m_threads[i];
			if (t.GetID() == idx)
			{
				return &t;
			}
		}
		return nullptr;
	}

	LockFreeQueue<JobSharedPtr>* JobSystem::GetQueueByPriority(JobPriority priority)
	{
		return m_queue.GetQueueByPriority(priority);
	}

	bool JobSystem::GetNextJob(JobSharedPtr& job)
	{
		return m_queue.GetNextJob(job);
	}


	/// <summary>
	/// JobSystemManager
	/// </summary>
	/// <param name="options"></param>
	JobSystemManager::JobSystemManager(const JobSystemManagerOptions& options)
		: m_numThreads(options.NumThreads)
		, m_threadAffinity(options.ThreadAffinity)
		, m_shutdownAfterMain(options.ShutdownAfterMainCallback)
	{ }

	JobSystemManager::~JobSystemManager()
	{
		delete[] m_allThreads;
	}

	JobSystemManager::ReturnCode JobSystemManager::Init()
	{
		if (m_allThreads)
		{
			return ReturnCode::AlreadyInitialized;
		}

		// Threads
		m_allThreads = new Thread[m_numThreads];

		// Current (Main) Thread
		m_mainThreadId = std::this_thread::get_id();

		// Thread Affinity
		if (m_threadAffinity && m_numThreads > std::thread::hardware_concurrency())
		{
			return ReturnCode::ErrorThreadAffinity;
		}

		// Spawn Threads
		for (uint8_t i = 0; i < m_numThreads; i++)
		{
			TLS* ttls = m_allThreads[i].GetTLS();
			ttls->ThreadIndex = i;
			ttls->SetAffinity = m_threadAffinity;

			if (!m_allThreads[i].Spawn(ThreadCallback_Worker))
			{
				return ReturnCode::OSError;
			}
			m_allThreads[i].SetThreadData(this, &m_mainJobSystem);
			m_useableThreads.push_back(&m_allThreads[i]);
		}

		// Done
		return ReturnCode::Succes;
	}

	JobSystem JobSystemManager::CreateLocalJobSystem(uint32_t numThreads)
	{
		if (static_cast<int>(m_useableThreads.size()) < numThreads)
		{
			std::cout << "[JobSystemManager::CreateLocalJobSystem] Requested threads more than usable threads." << '\n';
		}

		int threadsLeft = static_cast<int>(m_useableThreads.size());
		threadsLeft -= numThreads;
		if (threadsLeft <= 0)
		{
			std::cout << "[JobSystemManager::CreateLocalJobSystem] No threads left for main job system." << '\n';
		}

		std::vector<Thread*>::iterator itr = m_useableThreads.begin() + numThreads;
		std::vector<Thread*> jsThreads(numThreads);
		std::move(m_useableThreads.begin(), itr, jsThreads.begin());
		m_useableThreads.erase(m_useableThreads.begin(), itr);
		m_reservedThreads.insert(m_reservedThreads.end(), jsThreads.begin(), jsThreads.end());

		return JobSystem(this, std::move(jsThreads), m_mainThreadId);
	}

	void JobSystemManager::ReleaseJobSystem(JobSystem& jobSystem)
	{
		m_useableThreads.insert(jobSystem.m_threads.begin(), jobSystem.m_threads.end(), m_useableThreads.end());
		for (Thread const* t : jobSystem.m_threads)
		{
			m_reservedThreads.erase(std::find(m_reservedThreads.begin(), m_reservedThreads.end(), t));
		}
		jobSystem.m_threads.clear();
	}

	void JobSystemManager::Shutdown(bool blocking)
	{
		m_mainJobSystem.Shutdown(blocking);
	}

	void JobSystemManager::ScheduleJob(const JobSharedPtr job)
	{
		ScheduleJob(job->m_priority, job, false);
	}

	void JobSystemManager::ScheduleJob(JobPriority priority, const JobSharedPtr& job, bool GetParentJob)
	{
		m_mainJobSystem.ScheduleJob(priority, job, GetParentJob);
	}

	void JobSystemManager::WaitForAll() const
	{

	}

	void JobSystemManager::Update(uint32_t const& jobsToFree)
	{
		m_mainJobSystem.Update(jobsToFree);
	}

	uint32_t JobSystemManager::GetCurrentThreadIndex() const
	{
		return m_mainJobSystem.GetCurrentThreadIndex();
	}

	Thread* JobSystemManager::GetCurrentThread() const
	{
		return m_mainJobSystem.GetCurrentThread();
	}

	LockFreeQueue<JobSharedPtr>* JobSystemManager::GetQueueByPriority(JobPriority priority)
	{
		return m_mainJobSystem.GetQueueByPriority(priority);
	}

	bool JobSystemManager::GetNextJob(JobSharedPtr& job)
	{
		return m_mainJobSystem.GetNextJob(job);
	}

	uint32_t JobSystemManager::GetPendingJobsCount() const
	{
		return m_mainJobSystem.GetPendingJobsCount();
	}

	void JobSystemManager::ThreadCallback_Worker(Thread* thread, ThreadData data)
	{
		if (!data.Manager && !data.System)
		{ return; }

		JobSystemManager const* manager = data.Manager;
		JobSystem* system = data.System;
		auto tls = thread->GetTLS();

		// Thread Affinity
		if (tls->SetAffinity)
		{
			thread->SetAffinity(tls->ThreadIndex);
		}

		JobSharedPtr job = nullptr;
		// Thread loop. Every thread will be running this loop looking for new jobs to execute.
		while (!manager->IsShuttingDown())
		{
			if (system->GetNextJob(job))
			{
				job->Call();
				continue;
			}
			Thread::SleepFor(1);
		}
	}
}