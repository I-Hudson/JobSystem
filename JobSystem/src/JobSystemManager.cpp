#include "JobSystemManager.h"
#include "Thread.h"
#include "Fiber.h"
#include <thread>

JobSystemManager::JobSystemManager(const JobSystemManagerOptions& options)
	: m_numThreads(options.NumThreads)
	, m_threadAffinity(options.ThreadAffinity)
	, m_numFibers(options.NumFibers)
	, m_highPriorityQueue(options.HighPriorityQueueSize)
	, m_normalPriorityQueue(options.NormalPriorityQueueSize)
	, m_lowPriorityQueue(options.LowPriorityQueueSize)
	, m_shutdownAfterMain(options.ShutdownAfterMainCallback)
{ }

JobSystemManager::~JobSystemManager()
{
	delete[] m_threads;
	delete[] m_fibers;
	delete[] m_idleFibers;
}

JobSystemManager::ReturnCode JobSystemManager::Init()
{
	if (m_threads || m_fibers) 
	{
		return ReturnCode::AlreadyInitialized;
	}

	// Threads
	m_threads = new Thread[m_numThreads];
		
	// Current (Main) Thread
	//Thread* mainThread = &m_threads[0];
	//mainThread->FromCurrentThread();
	m_mainThreadId = std::this_thread::get_id();

	//if (m_threadAffinity)
	//{
	//	mainThread->SetAffinity(1);
	//}

	//auto mainThreadTLS = mainThread->GetTLS();
	//mainThreadTLS->ThreadFiber.FromCurrentThread();

	// Create Fibers
	// This has to be done after Thread is converted to Fiber!
	if (m_numFibers == 0) 
	{
		return ReturnCode::InvalidNumFibers;
	}

	m_fibers = new Fiber[m_numFibers];
	m_idleFibers = new std::atomic_bool[m_numFibers];
	for (uint16_t i = 0; i < m_numFibers; i++)
	{
		m_fibers[i].SetCallback(FiberCallback_Worker);
		m_idleFibers[i].store(true, std::memory_order_relaxed);
	}

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

		//if (i > 0) // 0 is Main Thread
		{
			ttls->SetAffinity = m_threadAffinity;

			if (!m_threads[i].Spawn(ThreadCallback_Worker, this)) {
				return ReturnCode::OSError;
			}
		}
	}

	// Main
	//if (callback == nullptr)
	//{
	//	return ReturnCode::NullCallback;
	//}

	// Setup main Fiber
	//mainThreadTLS->CurrentFiberIndex = FindFreeFiber();
	//auto mainFiber = &m_fibers[mainThreadTLS->CurrentFiberIndex];
	//mainFiber->SetCallback(FiberCallback_Main);
	//mainThreadTLS->ThreadFiber.SwitchTo(mainFiber, this);

	// Wait for all Threads to shut down
	//for (uint8_t i = 0; i < m_numThreads; ++i)
	//{
	//	m_threads[i].Join();
	//}

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
}

void JobSystemManager::ScheduleJob(JobPriority priority, const IJob* job)
{
	auto queue = GetQueueByPriority(priority);
	if (!queue) 
	{
		return;
	}

	if (job->GetCounter()) 
	{
		job->GetCounter()->Increment();
	}

	if (!queue->enqueue(const_cast<IJob*>(job)))
	{
		throw std::exception("Job Queue is full!");
	}
}

void JobSystemManager::WaitForCounter(BaseCounter* counter, uint32_t targetValue)
{
	if (counter == nullptr || counter->GetValue() == targetValue) 
	{
		return;
	}

	auto tls = GetCurrentTLS();
	auto fiberStored = new std::atomic_bool(false);

	if (counter->AddWaitingFiber(tls->CurrentFiberIndex, targetValue, fiberStored)) 
	{
		// Already done
		delete fiberStored;
		return;
	}

	// Update TLS
	tls->PreviousFiberIndex = tls->CurrentFiberIndex;
	tls->PreviousFiberDestination = FiberDestination::Waiting;
	tls->PreviousFiberStored = fiberStored;

	// Switch to idle Fiber
	tls->CurrentFiberIndex = FindFreeFiber();
	tls->ThreadFiber.SwitchTo(&m_fibers[tls->CurrentFiberIndex], this);

	// Cleanup
	CleanupPreviousFiber();
}

void JobSystemManager::WaitForSingle(JobPriority priority, IJob* job)
{
	TinyCounter ctr(this);
	ctr.Init();
	job->SetCounter(&ctr);

	ScheduleJob(priority, job);
	if (std::this_thread::get_id() != GetMainThreadId())
	{
		WaitForCounter(&ctr);
	}
	else
	{
		// This is the main thread. We need to wait another way.
		while (job->IsQueued() || job->IsRunning()) { }
	}
}

uint16_t JobSystemManager::FindFreeFiber()
{
	while (true)
	{
		for (uint16_t i = 0; i < m_numFibers; i++)
		{
			if (!m_idleFibers[i].load(std::memory_order_relaxed) ||
				!m_idleFibers[i].load(std::memory_order_acquire)) 
			{
				continue;
			}

			bool expected = true;
			if (std::atomic_compare_exchange_weak_explicit(&m_idleFibers[i], &expected, false, std::memory_order_release, std::memory_order_relaxed))
			{
				return i;
			}
		}

		// TODO: Add Debug Counter and error message
	}
}

void JobSystemManager::CleanupPreviousFiber(TLS* tls)
{
	if (tls == nullptr)
	{
		tls = GetCurrentTLS();
	}

	switch (tls->PreviousFiberDestination)
	{
		case FiberDestination::None:
			return;

		case FiberDestination::Pool:
			m_idleFibers[tls->PreviousFiberIndex].store(true, std::memory_order_release);
			break;

		case FiberDestination::Waiting:
			tls->PreviousFiberStored->store(true, std::memory_order_relaxed);
			break;

		default:
			break;
	}

	// Cleanup TLS
	tls->PreviousFiberIndex = UINT16_MAX;
	tls->PreviousFiberDestination = FiberDestination::None;
	tls->PreviousFiberStored = nullptr;
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

TLS* JobSystemManager::GetCurrentTLS()
{
	std::thread::id idx = std::this_thread::get_id();
	for (uint8_t i = 0; i < m_numThreads; ++i)
	{
		if (m_threads[i].GetID() == idx)
		{
			return m_threads[i].GetTLS();
		}
	}
	return nullptr;
}

LockFreeQueue<IJob*>* JobSystemManager::GetQueueByPriority(JobPriority priority)
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

bool JobSystemManager::GetNextJob(IJob*& job, TLS* tls)
{
	// High Priority Jobs always come first
	if (m_highPriorityQueue.dequeue(job))
	{
		return true;
	}

	// Ready Fibers
	if (tls == nullptr) 
	{
		tls = GetCurrentTLS();
	}

	for (auto it = tls->ReadyFibers.begin(); it != tls->ReadyFibers.end(); ++it)
	{
		uint16_t fiberIndex = it->first;

		// Make sure Fiber is stored
		if (!it->second->load(std::memory_order_relaxed))
		{
			continue;
		}

		// Erase
		delete it->second;
		tls->ReadyFibers.erase(it);

		// Update TLS
		tls->PreviousFiberIndex = tls->CurrentFiberIndex;
		tls->PreviousFiberDestination = FiberDestination::Pool;
		tls->CurrentFiberIndex = fiberIndex;

		// Switch to Fiber
		tls->ThreadFiber.SwitchTo(&m_fibers[fiberIndex], this);
		CleanupPreviousFiber(tls);

		break;
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

	// Setup Thread Fiber
	tls->ThreadFiber.FromCurrentThread();

	// Fiber
	tls->CurrentFiberIndex = manager->FindFreeFiber();

	auto fiber = &manager->m_fibers[tls->CurrentFiberIndex];
	tls->ThreadFiber.SwitchTo(fiber, manager);
}

void JobSystemManager::FiberCallback_Worker(Fiber* fiber)
{
	auto manager = reinterpret_cast<JobSystemManager*>(fiber->GetUserData());
	manager->CleanupPreviousFiber();
	IJob* job;

	while (!manager->IsShuttingDown()) 
	{
		auto tls = manager->GetCurrentTLS();

		if (manager->GetNextJob(job, tls)) 
		{
			job->Call();
			continue;
		}

		Thread::SleepFor(1);
	}

	// Switch back to Thread
	fiber->SwitchBack();
}

void JobSystemManager::FiberCallback_Main(Fiber* fiber)
{
	auto manager = reinterpret_cast<JobSystemManager*>(fiber->GetUserData());

	// Main
	manager->m_mainCallback(manager);

	// Shutdown after Main
	if (!manager->m_shutdownAfterMain)
	{
		// Switch to idle Fiber
		auto tls = manager->GetCurrentTLS();
		tls->CurrentFiberIndex = manager->FindFreeFiber();

		auto fiber = &manager->m_fibers[tls->CurrentFiberIndex];
		tls->ThreadFiber.SwitchTo(fiber, manager);
	}

	// Shutdown
	manager->Shutdown(false);

	// Switch back to Main Thread
	fiber->SwitchBack();
}