#include "Fiber.h"
#ifdef _WIN32
#include <Windows.h>
#include <exception>
#endif

static void LaunchFiber(Fiber* fiber)
{
	auto callback = fiber->GetCallback();
	if (callback == nullptr) {
		throw std::exception("LaunchFiber: callback is nullptr");
	}

	callback(fiber);
}

Fiber::Fiber()
{
#ifdef _WIN32
	m_fiber = CreateFiber(0, (LPFIBER_START_ROUTINE)LaunchFiber, this);
	m_threadIsFiber = false;
#endif
}

Fiber::~Fiber()
{
#ifdef _WIN32
	if (m_fiber && !m_threadIsFiber)
	{
		DeleteFiber(m_fiber);
	}
#endif
}

void Fiber::FromCurrentThread()
{
#ifdef _WIN32
	if (m_fiber && !m_threadIsFiber)
	{
		DeleteFiber(m_fiber);
	}
	m_fiber = ConvertThreadToFiber(nullptr);
	m_threadIsFiber = true;
#endif
}

void Fiber::SetCallback(FiberCallback callback)
{
	if (callback == nullptr)
	{
		throw std::exception("[Fiber::SetCallback] Callback can't be null.");
	}
	m_callback = callback;
}

void Fiber::SwitchTo(Fiber* fiber, void* userdata)
{
	if (fiber == nullptr || fiber->m_fiber == nullptr)
	{
		throw std::exception("[Fiber::SwitchTo] Invalid fiber.");
	}

	fiber->m_userData = userdata;
	fiber->m_returnFiber = this;

#ifdef _WIN32
	SwitchToFiber(fiber->m_fiber);
#endif
}

void Fiber::SwitchBack()
{
	if (m_returnFiber && m_returnFiber->m_fiber)
	{
		SwitchToFiber(m_returnFiber->m_fiber);
	}
	else
	{
		throw std::exception("[Fiber::SwitchBack] Unable to switch back from fiber.");
	}
}
