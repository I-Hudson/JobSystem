#pragma once

#include <atomic>
#include <vector>
#include <memory>
#include "Thread.h"
#include "JobFuncWrapper.h"
#include "LockFreeQueue.h"
#include "Counter.h"

class JobSystemManager;

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
	IJob(std::unique_ptr<IJobFuncWrapper> funcWrapper);
	IJob(std::unique_ptr<IJobFuncWrapper> funcWrapper, IJob * parentJob);
	~IJob();

	bool IsQueued() const { return m_state.load() == JobState::Queued; }
	bool IsStarted() const { return m_state.load() == JobState::Queued; }
	bool IsRunning() const { return m_state.load() == JobState::Running; }
	bool IsWaiting() const { return m_state.load() == JobState::Running; }
	bool IsFinished() const { return m_state.load() == JobState::Finished; }
	bool IsCancled() const { return m_state.load() == JobState::Canceled; }
	JobState GetState() { return m_state.load(); }

	virtual void Call();

	inline void SetCounter(BaseCounter* counter)
	{
		m_counter = counter;
	}
	inline BaseCounter* GetCounter() const
	{
		return m_counter;
	}

protected:
	std::atomic<JobState> m_state;
	std::vector<std::unique_ptr<IJob>> m_childrenJobs;
	IJob* m_parentJob;

	//Counter
	BaseCounter* m_counter;
	std::unique_ptr<IJobFuncWrapper> m_funcWrapper;

private:
	friend class JobSystemManager;
};