#pragma once
#include <stdint.h>
#include <atomic>

class JobSystemManager;

class BaseCounter
{
private:
	friend class JobSystemManager;
protected:
	using uint_t = uint32_t;

	// Counter
	std::atomic<uint_t> m_counter = 0;

	// Waiting Fibers
	struct WaitingFibers
	{
		uint16_t FiberIndex = UINT16_MAX;
		std::atomic_bool* FiberStored = nullptr;
		uint_t TargetValue = 0;

		std::atomic_bool InUse = true;
	};

	uint8_t m_numWaitingFibers;
	WaitingFibers* m_waitingFibers;
	std::atomic_bool* m_freeWaitingSlots;

	JobSystemManager* m_manager;

	// Methods
	bool AddWaitingFiber(uint16_t fiberIndex, uint_t targetValue, std::atomic_bool* fiberStored);
	void CheckWaitingFibers(uint_t value);

public:
	BaseCounter(JobSystemManager* mgr, uint8_t numWaitingFibers, WaitingFibers* waitingFibers, std::atomic_bool* freeWaitingSlots);
	virtual ~BaseCounter() = default;

	void Init();

	// Modifiers
	uint_t Increment(uint_t by = 1);
	uint_t Decrement(uint_t by = 1);

	// Counter Value
	uint_t GetValue() const;
};

struct TinyCounter : public BaseCounter
{
	TinyCounter(JobSystemManager* manager);
	~TinyCounter() = default;

	std::atomic_bool m_freeWaitingSlot;
	WaitingFibers m_waitingFiber;
};

class Counter : public BaseCounter
{
public:
	static const constexpr uint8_t MAX_WAITING = 16;

private:
	std::atomic_bool m_impl_freeWaitingSlots[MAX_WAITING];
	WaitingFibers m_impl_waitingFibers[MAX_WAITING];

public:
	Counter(JobSystemManager* manager);
	~Counter() = default;
};