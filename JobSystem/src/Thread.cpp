#include "Thread.h"
#include <exception>
#include <basetsd.h>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace Insight::JS
{
	static void LaunchThread(void* ptr)
	{
		auto thread = reinterpret_cast<Thread*>(ptr);
		auto callback = thread->GetCallback();

		if (callback == nullptr)
		{
			throw std::exception("[LaunchThread] LaunchThread: callback is nullptr");
		}
		callback(thread);
	}

	bool Thread::Spawn(Callback callback)
	{
		m_callback = callback;

		m_handle = std::thread(LaunchThread, this);
		m_id = m_handle.get_id();
		return HasSpawned();
	}

	void Thread::SetThreadData(JobSystemManager* manager, JobSystem* system)
	{
		{
			std::lock_guard lock(m_userDataMutex);
			m_userData = ThreadData{ manager, system };
		}
	}

	void Thread::SetAffinity(size_t i)
	{
		if (!HasSpawned())
		{
			return;
		}

#ifdef _WIN32
		DWORD_PTR mask = 1ull << i;
		SetThreadAffinityMask(m_handle.native_handle(), mask);
#endif
	}

	void Thread::Join()
	{
		if (!HasSpawned())
		{
			return;
		}
		if (m_handle.joinable())
		{
			m_handle.join();
		}
	}

	ThreadData Thread::GetUserdata()
	{
		ThreadData tData;
		{
			std::lock_guard lock(m_userDataMutex);
			tData = m_userData;
		}
		return tData;
	}

	void Thread::SleepFor(uint32_t ms)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
}