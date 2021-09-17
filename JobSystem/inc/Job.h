#pragma once

#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "Thread.h"
#include "JobFuncWrapper.h"
#include "LockFreeQueue.h"

namespace Insight::JS
{
	class JobSystemManager;
	class JobWaitList;
	class JobQueue;
	class JobSystem;
	class IJob;

	using JobPtr = IJob*;
	using JobSharedPtr = std::shared_ptr<IJob>;

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

	/// <summary>
	/// Wait for all jobs within the list.
	/// </summary>
	class JobWaitList
	{
	public:
		void AddJobToWaitOn(JobSharedPtr job);
		void Wait();
	private:
		std::vector<JobSharedPtr> m_jobsToWaitOn;
	};

	class IJob : public NonCopyable
	{
	public:
		IJob() = default;
		IJob(JobPriority priority, std::unique_ptr<IJobFuncWrapper> funcWrapper);
		IJob(JobPriority priority, std::unique_ptr<IJobFuncWrapper> funcWrapper, JobPtr parentJob);
		virtual ~IJob();

		bool IsQueued() const { return m_state.load() == JobState::Queued; }
		bool IsStarted() const { return m_state.load() == JobState::Queued; }
		bool IsRunning() const { return m_state.load() == JobState::Running; }
		bool IsWaiting() const { return m_state.load() == JobState::Running; }
		bool IsFinished() const { return m_state.load() == JobState::Finished; }
		bool IsCancled() const { return m_state.load() == JobState::Canceled; }
		JobState GetState() { return m_state.load(); }

		void Wait();

		template<typename Func, typename... Args>
		auto Then(Func func, Args... args)
		{
			using ResultType = std::invoke_result_t<Func, Args...>;
			std::unique_ptr<JobResult<ResultType>> jobResult = std::make_unique<JobResult<ResultType>>();
			std::unique_ptr<IJobFuncWrapper> funcWrapper = std::make_unique<JobFuncWrapper<ResultType, Func, Args...>>(jobResult.get(), func, std::move(args)...);
			JobWithResultSharedPtr<ResultType> job = std::make_shared<JobWithResult<ResultType>>(m_priority, std::move(funcWrapper), this, std::move(jobResult));
			m_childrenJobs.push_back(job);
			return job;
		}

	private:
		virtual void Call();
		void ReleaseLock();

		void SetState(JobState state) { m_state.store(state); }

	protected:
		std::atomic<JobState> m_state;
		uint16_t m_currentChildJob = 0;
		std::vector<JobSharedPtr> m_childrenJobs;
		JobPtr m_parentJob = nullptr;
		JobPriority m_priority;
		//std::mutex m_mutex;
		std::condition_variable m_conditionVariable;
		std::atomic_bool m_locked;
		std::unique_ptr<IJobFuncWrapper> m_funcWrapper;

	private:
		friend class JobWaitList;
		friend JobQueue;
		friend JobSystem;
		friend class JobSystemManager;
	};

	/// <summary>
	/// Job with a return. 
	/// </summary>
	/// <typeparam name="ResultType"></typeparam>
	template<typename ResultType>
	class JobWithResult : public IJob
	{
	public:
		JobWithResult()
			: IJob(JobPriority::Low, nullptr, nullptr)
			, m_result(nullptr)
		{ }
		JobWithResult(JobPriority priority, std::unique_ptr<IJobFuncWrapper> funcWrapper, std::unique_ptr<JobResult<ResultType>> jobResult)
			: IJob(priority, std::move(funcWrapper))
			, m_result(std::move(jobResult))
		{ }
		JobWithResult(JobPriority priority, std::unique_ptr<IJobFuncWrapper> funcWrapper, JobPtr parentJob, std::unique_ptr<JobResult<ResultType>> jobResult)
			: IJob(priority, std::move(funcWrapper), parentJob)
			, m_result(std::move(jobResult))
		{ }

		virtual ~JobWithResult() override
		{
			m_result.reset();
		}

		bool IsValid() const { return m_result != nullptr; }
		bool IsReady() const { return m_result->IsReady(); }
		JobResult<ResultType>& GetResult() { return *m_result; }

	private:
		std::unique_ptr<JobResult<ResultType>> m_result = nullptr;
	};

	template<typename ResultType>
	using JobWithResultSharedPtr = std::shared_ptr<JobWithResult<ResultType>>;
}