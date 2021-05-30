#pragma once

#include <stdint.h>
#include <thread>
#include <array>
#include "Job.h"

class Thread;
struct TLS;
class Fiber;

struct JobSystemManagerOptions
{
	JobSystemManagerOptions()
		: NumThreads(std::thread::hardware_concurrency())
	{ }
	~JobSystemManagerOptions() = default;

	// Threads & Fibers
	uint8_t NumThreads;						// Amount of Worker Threads, default = amount of Cores
	bool ThreadAffinity = true;				// Lock each Thread to a processor core, requires NumThreads == amount of cores

	// Worker Queue Sizes
	size_t HighPriorityQueueSize = 512;		// High Priority
	size_t NormalPriorityQueueSize = 2048;	// Normal Priority
	size_t LowPriorityQueueSize = 4096;		// Low Priority
	size_t JobFinishedQueue = 4096;			// Finished queue

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

	JobSystemManager(const JobSystemManagerOptions& = JobSystemManagerOptions());
	~JobSystemManager();

	// Initialize & Run Manager
	ReturnCode Init();

	template<typename Func, typename... Args>
	JobPtr CreateJob(JobPriority priority, Func func, Args... args)
	{
		std::unique_ptr<IJobFuncWrapper> funcWrapper = std::make_unique<JobFuncWrapper<Func, Args...>>(func, std::move(args)...);
		IJob* job = new IJob(priority, std::move(funcWrapper));
		//ScheduleJob(priority, job);
		return job;
	}

	// Shutdown all Jobs/Threads/Fibers
	// blocking => wait for threads to exit
	void Shutdown(bool blocking);

	// Jobs
	void ScheduleJob(const JobPtr job);
	void ScheduleJob(JobPriority priority, const JobPtr job, bool GetParentJob);

	void WaitForAll();

	// Small update function.
	void Update(uint32_t jobsToFree = -1);

	// Getter
	inline bool IsShuttingDown() const { return m_shuttingDown.load(std::memory_order_acquire); };
	const uint8_t GetNumThreads() const { return m_numThreads; };
	inline const std::thread::id& GetMainThreadId() const { return m_mainThreadId; }

protected:
	std::atomic_bool m_shuttingDown = false;

	// Threads
	uint8_t m_numThreads;
	Thread* m_threads = nullptr;
	bool m_threadAffinity = false;
	std::thread::id m_mainThreadId;

	// Thread
	uint8_t GetCurrentThreadIndex() const;
	Thread* GetCurrentThread();

	LockFreeQueue<JobPtr> m_highPriorityQueue;
	LockFreeQueue<JobPtr> m_normalPriorityQueue;
	LockFreeQueue<JobPtr> m_lowPriorityQueue;
	LockFreeQueue<JobPtr> m_finishedQueue;

	LockFreeQueue<JobPtr>* GetQueueByPriority(JobPriority priority);
	bool GetNextJob(JobPtr& job);

private:
	Callback m_mainCallback = nullptr;
	bool m_shutdownAfterMain = true;

	static void ThreadCallback_Worker(Thread*);

	friend class BaseCounter;
};