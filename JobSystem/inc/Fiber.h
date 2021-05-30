#pragma once

/// <summary>
/// 
/// </summary>
class Fiber
{
public:
	using FiberCallback = void(*)(Fiber*);

	Fiber();
	Fiber(const Fiber&) = delete;
	~Fiber();

	void FromCurrentThread();

	void SetCallback(FiberCallback);

	void SwitchTo(Fiber* fiber, void* userdata = nullptr);
	void SwitchBack();

	inline FiberCallback GetCallback() const { return m_callback; }
	inline void* GetUserData() const { return m_userData; }
	inline bool IsValid() const { return m_fiber && m_threadIsFiber; }

private:
	Fiber(Fiber* fiber)
		: m_fiber(fiber)
	{ }

private:
	void* m_fiber = nullptr;
	bool m_threadIsFiber = false;
	Fiber* m_returnFiber = nullptr;
	FiberCallback m_callback = nullptr;
	void* m_userData = nullptr;
};