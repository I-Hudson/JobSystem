#pragma once

#include <stdint.h>
#include <thread>
#include <array>
#include "Job.h"

namespace Insight::JS
{
	class Thread;
	struct TLS;
	class Fiber;
	class JobSystemManager;
	class JobSystem;

	struct JobQueueOptions
	{
		// Worker Queue Sizes
		size_t HighPriorityQueueSize = 512;		// High Priority
		size_t NormalPriorityQueueSize = 2048;	// Normal Priority
		size_t LowPriorityQueueSize = 4096;		// Low Priority
		size_t JobFinishedQueue = 4096;			// Finished queue
	};

	class JobQueue
	{
	public:
		JobQueue(JobQueueOptions options = JobQueueOptions());

		void Update(uint32_t const& jobsToFree);

		uint32_t GetPendingJobsCount() const;

		// Jobs
		void ScheduleJob(const JobSharedPtr job);
		void ScheduleJob(JobPriority priority, const JobSharedPtr& job, bool GetParentJob);

		//JobQueue& operator=(JobQueue const& other)
		//{
		//	Release();
		//	m_highPriorityQueue = other.m_highPriorityQueue;
		//	m_normalPriorityQueue = other.m_normalPriorityQueue;
		//	m_lowPriorityQueue = other.m_lowPriorityQueue;
		//	m_finishedQueue = other.m_finishedQueue;
		//	return *this;
		//}

	private:
		moodycamel::ReaderWriterQueue<JobSharedPtr>* GetQueueByPriority(JobPriority priority);
		bool GetNextJob(JobSharedPtr& job);

		void Release();

	private:
		moodycamel::ReaderWriterQueue<JobSharedPtr> m_highPriorityQueue;
		moodycamel::ReaderWriterQueue<JobSharedPtr> m_normalPriorityQueue;
		moodycamel::ReaderWriterQueue<JobSharedPtr> m_lowPriorityQueue;
		moodycamel::ReaderWriterQueue<JobSharedPtr> m_finishedQueue;

		friend JobSystem;
		friend JobSystemManager;
	};

	/// <summary>
	/// Single job system. Holds threads and a job queue.
	/// </summary>
	class JobSystem
	{
	public:
		JobSystem();
		JobSystem(JobSystemManager* manager, std::thread::id mainThreadId);
		JobSystem(JobSystem && other);

		template<typename Func, typename... Args>
		static auto CreateJob(JobPriority priority, Func func, Args... args)
		{
			using ResultType = std::invoke_result_t<Func, Args...>;
			std::unique_ptr<JobResult<ResultType>> jobResult = std::make_unique<JobResult<ResultType>>();
			std::unique_ptr<IJobFuncWrapper> funcWrapper = std::make_unique<JobFuncWrapper<ResultType, Func, Args...>>(jobResult.get(), func, std::move(args)...);
			JobWithResultSharedPtr<ResultType> job = std::make_shared<JobWithResult<ResultType>>(priority, std::move(funcWrapper), std::move(jobResult));
			return job;
		}

		void ReserveThreads(uint32_t numThreads);
		void Release();

		// Jobs
		void ScheduleJob(const JobSharedPtr job);
		void ScheduleJob(JobPriority priority, const JobSharedPtr& job, bool GetParentJob);

		void WaitForAll() const;

		// Small update function.
		void Update(uint32_t  const& jobsToFree = 64);

		uint32_t GetPendingJobsCount() const;

		// Getter
		const uint32_t GetNumThreads() const { return m_numThreads; };
		inline const std::thread::id& GetMainThreadId() const { return m_mainThreadId; }

		JobSystem& operator=(JobSystem const& other) = delete;
		JobSystem& operator=(JobSystem && other)
		{
			m_manager = std::move(other.m_manager);
			Release();
			m_numThreads = std::move(other.m_numThreads);
			m_threads = std::move(other.m_threads);
			m_mainThreadId = std::move(other.m_mainThreadId);
			m_queue = std::move(other.m_queue);
			return *this;
		}

	private:
		void AddThreads(std::vector<Thread*> threads);
		void RemoveThreads();

		void ClearQueue();

		void Shutdown(bool blocking);

	private:
		JobSystemManager* m_manager = nullptr;

		// Threads
		uint32_t m_numThreads = 0;
		std::vector<Thread*> m_threads;
		std::thread::id m_mainThreadId;
		JobQueue m_queue;

		// Thread
		uint8_t GetCurrentThreadIndex() const;
		Thread* GetCurrentThread() const;

		moodycamel::ReaderWriterQueue<JobSharedPtr>* GetQueueByPriority(JobPriority priority);
		bool GetNextJob(JobSharedPtr& job);

		friend JobSystemManager;
		friend class BaseCounter;
	};

	struct JobSystemManagerOptions
	{
		JobSystemManagerOptions()
			: NumThreads(std::thread::hardware_concurrency())
		{ }
		~JobSystemManagerOptions() = default;

		// Threads & Fibers
		uint32_t NumThreads;						// Amount of Worker Threads, default = amount of Cores
		bool ThreadAffinity = true;				// Lock each Thread to a processor core, requires NumThreads == amount of cores

		// Other
		bool ShutdownAfterMainCallback = true;	// Shutdown everything after Main Callback returns?
	};

	class JobSystemManager
	{
	public:
		enum class ReturnCode : uint8_t
		{
			Succes = 0,

			UnknownError,
			OSError,				// OS-API call failed
			NullCallback,			// callback is nullptr

			AlreadyInitialized,		// Manager has already initialized
			InvalidNumFibers,		// Fiber count is 0 or too high
			ErrorThreadAffinity,	// ThreadAffinity is enabled, but Worker Thread Count > Available Cores
		};
		using Callback = void(*)(JobSystemManager*);

		JobSystemManager(const JobSystemManagerOptions & = JobSystemManagerOptions());
		~JobSystemManager();

		static JobSystemManager& Instance()
		{
			static JobSystemManager manager;
			return manager;
		}

		// Initialize & Run Manager
		ReturnCode Init();

		/// <summary>
		/// Create a new job system and reserving threads from the main pool.
		/// </summary>
		std::shared_ptr<JobSystem> CreateLocalJobSystem(uint32_t numThreads);
		void ReseveThreads(uint32_t const& numThreads);
		void ReseveThreads(JobSystem& jobSystem, uint32_t const& numThreads);
		void ReleaseJobSystem(JobSystem& jobSystem);

		template<typename Func, typename... Args>
		static auto CreateJob(JobPriority priority, Func func, Args... args)
		{
			using ResultType = std::invoke_result_t<Func, Args...>;
			std::unique_ptr<JobResult<ResultType>> jobResult = std::make_unique<JobResult<ResultType>>();
			std::unique_ptr<IJobFuncWrapper> funcWrapper = std::make_unique<JobFuncWrapper<ResultType, Func, Args...>>(jobResult.get(), func, std::move(args)...);
			JobWithResultSharedPtr<ResultType> job = std::make_shared<JobWithResult<ResultType>>(priority, std::move(funcWrapper), std::move(jobResult));
			return job;
		}

		// Shutdown all Jobs/Threads/Fibers
		// blocking => wait for threads to exit
		void Shutdown(bool blocking);

		// Jobs
		void ScheduleJob(const JobSharedPtr job);
		void ScheduleJob(JobPriority priority, const JobSharedPtr& job, bool GetParentJob);

		void WaitForAll() const;

		// Small update function.
		void Update(uint32_t  const& jobsToFree = 64);

		// Getter
		inline bool IsShuttingDown() const { return m_shuttingDown.load(std::memory_order_acquire); };
		const uint32_t GetNumThreads() const { return m_numThreads; };
		inline const std::thread::id& GetMainThreadId() const { return m_mainThreadId; }
		uint32_t GetPendingJobsCount() const;

		//inline uint32_t GetUseableThreads() const { return static_cast<uint32_t>(m_useableThreads.size()); }
		//inline uint32_t GetReservedThreads() const { return static_cast<uint32_t>(m_reservedThreads.size()); }


	protected:
		std::atomic_bool m_shuttingDown = false;

		// Threads
		uint32_t m_numThreads;
		Thread* m_allThreads = nullptr;
		bool m_threadAffinity = false;
		std::thread::id m_mainThreadId;

		// Thread
		uint32_t GetCurrentThreadIndex() const;
		Thread* GetCurrentThread() const;

		moodycamel::ReaderWriterQueue<JobSharedPtr>* GetQueueByPriority(JobPriority priority);
		bool GetNextJob(JobSharedPtr& job);

		JobSystem m_mainJobSystem;
		std::vector<std::shared_ptr<JobSystem>> m_jobSystems;

	private:
		Callback m_mainCallback = nullptr;
		bool m_shutdownAfterMain = true;

		static void ThreadCallback_Worker(Thread* thread);

		friend class BaseCounter;
	};
}