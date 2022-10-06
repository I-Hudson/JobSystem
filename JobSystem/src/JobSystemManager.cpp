#include "JobSystemManager.h"
#include "Thread.h"
#include <thread>
#include <iostream>
#include <algorithm>

namespace Insight::JS
{
	JobQueue::JobQueue(JobQueueOptions options)
		: m_highPriorityQueue(options.HighPriorityQueueSize)
		, m_normalPriorityQueue(options.NormalPriorityQueueSize)
		, m_lowPriorityQueue(options.LowPriorityQueueSize)
		, m_jobRunningQueue(options.LowPriorityQueueSize)
	{ }

	void JobQueue::Update(uint32_t const& jobsToFree)
	{ }

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
			throw std::overflow_error("Job Queue is full!");
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
			job = nullptr;
		}

		return m_highPriorityQueue.dequeue(job) ||
			   m_normalPriorityQueue.dequeue(job) ||
			   m_lowPriorityQueue.dequeue(job);
	}

	void JobQueue::Release()
	{
		JobSharedPtr job;
		while(m_highPriorityQueue.dequeue(job) || m_normalPriorityQueue.dequeue(job) || m_lowPriorityQueue.dequeue(job))
		{
			job->SetState(JobState::Canceled);
			job->ReleaseLock();
		}
	}

	/// <summary>
	/// JobSystem
	/// </summary>
	
	JobSystem::JobSystem()
	{
		//assert(false && "[JobSystem::JobSystem] JobSytem must be created from JobSystemManager using 'JobSystemManager::Instance()->CreateLocalJobSystem(numThreads)'.");
	}

	JobSystem::JobSystem(JobSystemManager* manager, std::thread::id mainThreadId)
		: m_mainThreadId(std::move(mainThreadId))
		, m_manager(manager)
	{
		for (Thread* t : m_threads)
		{
			ThreadData tData = t->GetUserdata();
			t->SetThreadData(tData.Manager, this);
		}
	}

	JobSystem::~JobSystem()
	{ }

	void JobSystem::ReserveThreads(uint32_t numThreads)
	{
		m_manager->ReseveThreads(*this, numThreads);
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
	{
		//TODO:
		while(GetPendingJobsCount() > 0 || GetRunningJobsCount() > 0)
		{
			Thread::SleepFor(1);
		}
	}

	void JobSystem::Update(uint32_t const& jobsToFree)
	{
		m_queue.Update(jobsToFree);
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

	void JobSystem::AddThreads(std::vector<Thread*> threads)
	{
		m_threads.insert(m_threads.end(), threads.begin(), threads.end());
		for (Thread* t : threads)
		{
			t->SetThreadData(m_manager, this);
		}
		m_numThreads = static_cast<uint32_t>(m_threads.size());
	}

	void JobSystem::RemoveThreads()
	{
		m_threads.clear();
	}

	void JobSystem::ClearQueue()
	{
		m_queue.Release();
	}

	void JobSystem::Shutdown(bool blocking)
	{
		assert(std::this_thread::get_id() == GetMainThreadId() && "[JobSystemManager::Shutdown] Shutdown must be called on the 'MainThread'.");
		if (blocking)
		{
			for (uint8_t i = 0; i < m_numThreads; ++i)
			{
				m_threads[i]->Join();
			}
		}
		Update();
	}


	/// <summary>
	/// JobSystemManager
	/// </summary>
	/// <param name="options"></param>
	JobSystemManager::JobSystemManager()
	{ }

	JobSystemManager::~JobSystemManager()
	{
		for (std::shared_ptr<JobSystem> const& js : m_jobSystems)
		{
			assert(js.use_count() == 1 && "[JobSystemManager::~JobSystemManager] Not all job systems have been released before manager is destroyed.");
			js->Release();
		}
		Shutdown(true);
		delete[] m_allThreads;
	}

	JobSystemManager::ReturnCode JobSystemManager::Init(const JobSystemManagerOptions& options)
	{

		if (m_allThreads)
		{
			return ReturnCode::AlreadyInitialized;
		}

		const uint32_t hardware_thread_count = std::thread::hardware_concurrency();
		if (options.NumThreads == 0 || options.NumThreads > hardware_thread_count)
		{
			return ReturnCode::InvalidNumThreads;
		}

		m_current_options = options;

		// Threads
		m_allThreads = new Thread[m_current_options.NumThreads];

		// Current (Main) Thread
		m_mainThreadId = std::this_thread::get_id();

		// Thread Affinity
		if (m_current_options.ThreadAffinity && m_current_options.NumThreads > hardware_thread_count)
		{
			return ReturnCode::ErrorThreadAffinity;
		}

		std::vector<Thread*> spawnedThreads;
		// Spawn Threads
		for (uint8_t i = 0; i < m_current_options.NumThreads; i++)
		{
			TLS* ttls = m_allThreads[i].GetTLS();
			ttls->ThreadIndex = i;
			ttls->SetAffinity = m_current_options.ThreadAffinity;

			m_allThreads[i].SetThreadData(this, &m_mainJobSystem);
			if (!m_allThreads[i].Spawn(ThreadCallback_Worker))
			{
				return ReturnCode::OSError;
			}
			spawnedThreads.push_back(&m_allThreads[i]);
		}
		m_mainJobSystem.m_manager = this;
		m_mainJobSystem.m_mainThreadId = GetMainThreadId();
		m_mainJobSystem.m_numThreads = m_current_options.NumThreads;
		m_mainJobSystem.AddThreads(spawnedThreads);

		// Done
		return ReturnCode::Succes;
	}

	std::shared_ptr<JobSystem> JobSystemManager::CreateLocalJobSystem(uint32_t numThreads)
	{
		std::shared_ptr<JobSystem> jobSystem = std::make_shared<JobSystem>(this, m_mainThreadId);
		ReseveThreads(*jobSystem.get(), numThreads);
		m_jobSystems.push_back(jobSystem);
		return jobSystem;
	}

	bool JobSystemManager::ReseveThreads(uint32_t const& numThreads)
	{
		return ReseveThreads(m_mainJobSystem, numThreads);
	}

	bool JobSystemManager::ReseveThreads(JobSystem& jobSystem, uint32_t const& numThreads)
	{
		if (m_mainJobSystem.GetNumThreads() < numThreads)
		{
			std::cout << "[JobSystemManager::CreateLocalJobSystem] Requested threads more than usable threads." << '\n';
			return false;
		}

		int threadsLeft = static_cast<int>(m_mainJobSystem.GetNumThreads());
		threadsLeft -= numThreads;
		if (threadsLeft <= 0)
		{
			std::cout << "[JobSystemManager::CreateLocalJobSystem] No threads left for main job system." << '\n';
			return false;
		}

		std::vector<Thread*>::iterator itr = m_mainJobSystem.m_threads.begin() + numThreads;
		std::vector<Thread*> jsThreads(numThreads);
		std::move(m_mainJobSystem.m_threads.begin(), itr, jsThreads.begin());
		m_mainJobSystem.m_threads.erase(m_mainJobSystem.m_threads.begin(), itr);
		m_mainJobSystem.m_numThreads = threadsLeft;
		jobSystem.AddThreads(jsThreads);
		return true;
	}

	void JobSystemManager::ReleaseJobSystem(JobSystem& jobSystem)
	{
		m_mainJobSystem.AddThreads(jobSystem.m_threads);
		jobSystem.RemoveThreads();
		jobSystem.ClearQueue();
		m_jobSystems.erase(std::find_if(m_jobSystems.begin(), m_jobSystems.end(), [&jobSystem](std::shared_ptr<JobSystem> const& system) 
						   {
							   return &jobSystem == system.get();
						   }));
	}

	void JobSystemManager::Shutdown(bool blocking)
	{
		m_shuttingDown.store(true, std::memory_order_release);
		for (std::shared_ptr<JobSystem>& js : m_jobSystems)
		{
			js->Shutdown(blocking);
		}
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

	void JobSystemManager::ThreadCallback_Worker(Thread* thread)
	{
		// This is where the thread will be executing.

		ThreadData tData = thread->GetUserdata();
		if (!tData.Manager || !tData.System)
		{
			throw new std::runtime_error("[JobSystemManager::ThreadCallback_Worker] Thread data was not valid.");
		}

		auto tls = thread->GetTLS();
		// Thread Affinity
		if (tls->SetAffinity)
		{
			thread->SetAffinity(tls->ThreadIndex);
		}

		JobSharedPtr job = nullptr;
		JobSystemManager* js_manager = tData.Manager;
		// Thread loop. Every thread will be running this loop looking for new jobs to execute.
		while (!js_manager->IsShuttingDown())
		{
			// Get the user data as the system this thread is assigned to could change.
			JobSystem* system = thread->GetUserdata().System;
			if (system->GetNextJob(job))
			{
				job->Call();
				continue;
			}
			Thread::SleepFor(1);
		}
	}
}