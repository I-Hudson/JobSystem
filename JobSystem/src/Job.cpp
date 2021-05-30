#include "Job.h"
#include "JobSystemManager.h"

namespace js
{
	IJob::IJob(JobPriority priority, std::unique_ptr<IJobFuncWrapper> funcWrapper)
		: m_state(JobState::Queued)
		, m_priority(priority)
		, m_parentJob(nullptr)
		, m_funcWrapper(std::move(funcWrapper))
	{
		m_locked.store(true, std::memory_order_release);
	}

	IJob::IJob(JobPriority priority, std::unique_ptr<IJobFuncWrapper> funcWrapper, JobPtr parentJob)
		: m_state(JobState::Queued)
		, m_priority(priority)
		, m_parentJob(std::move(parentJob))
		, m_funcWrapper(std::move(funcWrapper))
	{
		m_locked.store(true, std::memory_order_release);
	}

	IJob::~IJob()
	{
		m_parentJob = nullptr;
		m_childrenJobs.clear();
		m_funcWrapper.reset();
	}

	void IJob::Call()
	{
		//std::lock_guard<std::mutex> lock(m_mutex);
		m_state.store(JobState::Running);
		m_funcWrapper->Call();
	}

	void IJob::ReleaseLock()
	{
		//m_conditionVariable.notify_one();
		m_locked.store(false, std::memory_order_release);
	}

	void IJob::Wait()
	{
		//std::unique_lock<std::mutex> lock(m_mutex);
		//m_conditionVariable.wait(lock);
		while (m_locked.load(std::memory_order_acquire))
		{ }
	}

	void JobWaitList::AddJobToWaitOn(JobSharedPtr job)
	{
		m_jobsToWaitOn.push_back(job);
	}

	void JobWaitList::Wait()
	{
		bool waitting = true;
		while (waitting)
		{
			waitting = false;
			for (auto& job : m_jobsToWaitOn)
			{
				if (job->m_locked.load(std::memory_order_acquire))
				{
					waitting = true;
					break;
				}
			}
		}
	}
}