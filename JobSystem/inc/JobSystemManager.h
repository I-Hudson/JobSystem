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
	uint16_t NumFibers = 25;				// Amount of Fibers
	bool ThreadAffinity = true;				// Lock each Thread to a processor core, requires NumThreads == amount of cores

	// Worker Queue Sizes
	size_t HighPriorityQueueSize = 512;	// High Priority
	size_t NormalPriorityQueueSize = 2048;	// Normal Priority
	size_t LowPriorityQueueSize = 4096;	// Low Priority

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

	// Shutdown all Jobs/Threads/Fibers
	// blocking => wait for threads to exit
	void Shutdown(bool blocking);

	// Jobs
	void ScheduleJob(JobPriority priority, const IJob* job);

	// Counter
	void WaitForCounter(BaseCounter*, uint32_t = 0);
	void WaitForSingle(JobPriority, IJob* job);

	// Getter
	inline bool IsShuttingDown() const { return m_shuttingDown.load(std::memory_order_acquire); };
	const uint8_t GetNumThreads() const { return m_numThreads; };
	const uint16_t GetNumFibers() const { return m_numFibers; };
	inline const std::thread::id& GetMainThreadId() const { return m_mainThreadId; }

	template<typename Func, typename... Args>
	void WaitForSingle(JobPriority prio, Func func, Args... args)
	{
		std::unique_ptr<IJobFuncWrapper> funcWrapper = std::make_unique<JobFuncWrapper<Func, Args...>>(func, std::move(args)...);
		IJob* job = new IJob(std::move(funcWrapper));
		WaitForSingle(prio, job);
		delete job;
	}

protected:
	std::atomic_bool m_shuttingDown = false;

	// Threads
	uint8_t m_numThreads;
	Thread* m_threads = nullptr;
	bool m_threadAffinity = false;
	std::thread::id m_mainThreadId;

	// Fibers
	uint16_t m_numFibers;
	Fiber* m_fibers = nullptr;
	std::atomic_bool* m_idleFibers = nullptr;

	uint16_t FindFreeFiber();
	void CleanupPreviousFiber(TLS* tls = nullptr);

	// Thread
	uint8_t GetCurrentThreadIndex() const;
	Thread* GetCurrentThread();
	TLS* GetCurrentTLS();

	LockFreeQueue<IJob*> m_highPriorityQueue;
	LockFreeQueue<IJob*> m_normalPriorityQueue;
	LockFreeQueue<IJob*> m_lowPriorityQueue;

	LockFreeQueue<IJob*>* GetQueueByPriority(JobPriority priority);
	bool GetNextJob(IJob*& job, TLS* tls);

private:
	Callback m_mainCallback = nullptr;
	bool m_shutdownAfterMain = true;

	static void ThreadCallback_Worker(Thread*);
	static void FiberCallback_Worker(Fiber*);
	static void FiberCallback_Main(Fiber*);

	friend class BaseCounter;
};