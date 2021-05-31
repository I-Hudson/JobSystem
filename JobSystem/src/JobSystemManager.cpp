#include "JobSystemManager.h"
#include "Thread.h"
#include <thread>

namespace js
{
	JobSystemManager::JobSystemManager(const JobSystemManagerOptions& options)
		: m_numThreads(options.NumThreads)
		, m_threadAffinity(options.ThreadAffinity)
		, m_highPriorityQueue(options.HighPriorityQueueSize)
		, m_normalPriorityQueue(options.NormalPriorityQueueSize)
		, m_lowPriorityQueue(options.LowPriorityQueueSize)
		, m_finishedQueue(options.LowPriorityQueueSize)
		, m_shutdownAfterMain(options.ShutdownAfterMainCallback)
	{ }

	JobSystemManager::~JobSystemManager()
	{
		delete[] m_threads;
	}

	JobSystemManager::ReturnCode JobSystemManager::Init()
	{
		if (m_threads)
		{
			return ReturnCode::AlreadyInitialized;
		}

		// Threads
		m_threads = new Thread[m_numThreads];

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
			auto ttls = m_threads[i].GetTLS();
			ttls->ThreadIndex = i;
			ttls->SetAffinity = m_threadAffinity;

			if (!m_threads[i].Spawn(ThreadCallback_Worker, this))
			{
				return ReturnCode::OSError;
			}
		}

		// Done
		return ReturnCode::Succes;
	}

	void JobSystemManager::Shutdown(bool blocking)
	{
		assert(std::this_thread::get_id() == GetMainThreadId() && "[JobSystemManager::Shutdown] Shutdown must be called on the 'MainThread'.");
		m_shuttingDown.store(true, std::memory_order_release);
		if (blocking)
		{
			for (uint8_t i = 0; i < m_numThreads; ++i)
			{
				m_threads[i].Join();
			}
		}
		Update();
	}

	void JobSystemManager::ScheduleJob(const JobSharedPtr job)
	{
		ScheduleJob(job->m_priority, job, false);
	}

	void JobSystemManager::ScheduleJob(JobPriority priority, const JobSharedPtr& job, bool GetParentJob)
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

	void JobSystemManager::WaitForAll()
	{

	}

	void JobSystemManager::Update(uint32_t jobsToFree)
	{
		uint16_t jobsFreed = 0;
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

	uint8_t JobSystemManager::GetCurrentThreadIndex() const
	{
		std::thread::id idx = std::this_thread::get_id();
		for (uint8_t i = 0; i < m_numThreads; ++i)
		{
			if (m_threads[i].GetID() == idx)
			{
				return i;
			}
		}
		return UINT8_MAX;
	}

	Thread* JobSystemManager::GetCurrentThread()
	{
		std::thread::id idx = std::this_thread::get_id();
		for (uint8_t i = 0; i < m_numThreads; ++i)
		{
			if (m_threads[i].GetID() == idx)
			{
				return &m_threads[i];
			}
		}
		return nullptr;
	}

	LockFreeQueue<JobSharedPtr>* JobSystemManager::GetQueueByPriority(JobPriority priority)
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

	bool JobSystemManager::GetNextJob(JobSharedPtr& job)
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

	void JobSystemManager::ThreadCallback_Worker(Thread* thread)
	{
		auto manager = reinterpret_cast<JobSystemManager*>(thread->GetUserdata());
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
			if (manager->GetNextJob(job))
			{
				job->Call();
				continue;
			}
			Thread::SleepFor(1);
		}
	}
}