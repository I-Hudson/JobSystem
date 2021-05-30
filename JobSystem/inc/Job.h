#pragma once

#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "Thread.h"
#include "JobFuncWrapper.h"
#include "LockFreeQueue.h"

class JobSystemManager;
class JobWaitList;
class IJob;

using JobPtr = IJob*;//std::shared_ptr<IJob>;

struct NonCopyable
{
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

enum class JobPriority : uint8_t
{
	High,		// Jobs are executed ASAP
	Normal,
	Low
};

enum class JobState
{
	Queued,
	Running,
	Waiting,
	Finished,
	Canceled,
};

class IJob : public NonCopyable
{
public:
	IJob() = default;
	IJob(JobPriority priority, std::unique_ptr<IJobFuncWrapper> funcWrapper);
	IJob(JobPriority priority, std::unique_ptr<IJobFuncWrapper> funcWrapper, JobPtr parentJob);
	~IJob();

	bool IsQueued() const { return m_state.load() == JobState::Queued; }
	bool IsStarted() const { return m_state.load() == JobState::Queued; }
	bool IsRunning() const { return m_state.load() == JobState::Running; }
	bool IsWaiting() const { return m_state.load() == JobState::Running; }
	bool IsFinished() const { return m_state.load() == JobState::Finished; }
	bool IsCancled() const { return m_state.load() == JobState::Canceled; }
	JobState GetState() { return m_state.load(); }

	virtual void Call();
	void ReleaseLock();

	void SetState(JobState state) { m_state.store(state); }

	void Wait();
	JobPtr Then(std::unique_ptr<IJobFuncWrapper> funcWrapper);

	template<typename Func, typename... Args>
	JobPtr Then(Func func, Args... args)
	{
		std::unique_ptr<IJobFuncWrapper> funcWrapper = std::make_unique<JobFuncWrapper<Func, Args...>>(func, std::move(args)...);
		return Then(std::move(funcWrapper));
	}

protected:
	std::atomic<JobState> m_state;
	uint16_t m_currentChildJob = 0;
	std::vector<JobPtr> m_childrenJobs;
	JobPtr m_parentJob = nullptr;
	JobPriority m_priority;

	//std::mutex m_mutex;
	std::condition_variable m_conditionVariable;
	std::atomic_bool m_locked;

	std::unique_ptr<IJobFuncWrapper> m_funcWrapper;

private:
	friend class JobWaitList;
	friend class JobSystemManager;
};

/// <summary>
/// Wait for all jobs within the list.
/// </summary>
class JobWaitList
{
public:
	void AddJobToWaitOn(JobPtr job);
	void Wait();
private:
	std::vector<JobPtr> m_jobsToWaitOn;
};