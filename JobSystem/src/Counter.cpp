#include "Counter.h"
#include "JobSystemManager.h"
#include "TLS.h"
#include <exception>

BaseCounter::BaseCounter(JobSystemManager* mgr, uint8_t numWaitingFibers, WaitingFibers* waitingFibers, std::atomic_bool* freeWaitingSlots)
	: m_manager(mgr)
	, m_numWaitingFibers(numWaitingFibers)
	, m_waitingFibers(waitingFibers)
	, m_freeWaitingSlots(freeWaitingSlots)
{ }

void BaseCounter::Init()
{
	for (uint8_t i = 0; i < m_numWaitingFibers; ++i)
	{
		std::atomic_bool& atomic = m_freeWaitingSlots[i];
		atomic.store(true);
	}
}

BaseCounter::uint_t BaseCounter::Increment(uint_t by)
{
	uint_t prev = m_counter.fetch_add(by);
	CheckWaitingFibers(prev + by);

	return prev;
}

BaseCounter::uint_t BaseCounter::Decrement(uint_t by)
{
	uint_t prev = m_counter.fetch_sub(by);
	CheckWaitingFibers(prev - by);

	return prev;
}

BaseCounter::uint_t BaseCounter::GetValue() const
{
	return m_counter.load(std::memory_order_seq_cst);
}

/// <summary>
/// TODO: LOOK INTO THIS AND ASK WHY WE HAVE A LIMIT ON WAITING FIBERS
/// </summary>
/// <param name=""></param>
/// <param name=""></param>
/// <param name=""></param>
/// <returns></returns>
bool BaseCounter::AddWaitingFiber(uint16_t fiberIndex, uint_t targetValue, std::atomic_bool* fiberStored)
{
	for (uint8_t i = 0; i < m_numWaitingFibers; ++i)
	{
		// Acquire Free Waiting Slot
		bool expected = true;
		if (!std::atomic_compare_exchange_strong_explicit(&m_freeWaitingSlots[i], &expected, false, std::memory_order_seq_cst, std::memory_order_relaxed)) 
		{
			continue;
		}

		// Setup Slot
		auto slot = &m_waitingFibers[i];
		slot->FiberIndex = fiberIndex;
		slot->FiberStored = fiberStored;
		slot->TargetValue = targetValue;

		slot->InUse.store(false);

		// Check if we are done already
		uint_t counter = m_counter.load(std::memory_order_relaxed);
		if (slot->InUse.load(std::memory_order_acquire))
		{
			return false;
		}

		if (slot->TargetValue == counter) 
		{
			expected = false;
			if (!std::atomic_compare_exchange_strong_explicit(&slot->InUse, &expected, true, std::memory_order_seq_cst, std::memory_order_relaxed))
			{
				return false;
			}

			m_freeWaitingSlots[i].store(true, std::memory_order_release);
			return true;
		}

		return false;
	}

	// Waiting Slots are full
	throw std::exception("Counter waiting slots are full!");
}

void BaseCounter::CheckWaitingFibers(uint_t value)
{
	for (size_t i = 0; i < m_numWaitingFibers; i++)
	{
		if (m_freeWaitingSlots[i].load(std::memory_order_acquire)) 
		{
			continue;
		}

		auto waitingSlot = &m_waitingFibers[i];
		if (waitingSlot->InUse.load(std::memory_order_acquire)) 
		{
			continue;
		}

		if (waitingSlot->TargetValue == value) 
		{
			bool expected = false;
			if (!std::atomic_compare_exchange_strong_explicit(&waitingSlot->InUse, &expected, true, std::memory_order_seq_cst, std::memory_order_relaxed)) 
			{
				continue;
			}

			m_manager->GetCurrentTLS()->ReadyFibers.emplace_back(waitingSlot->FiberIndex, waitingSlot->FiberStored);
			m_freeWaitingSlots[i].store(true, std::memory_order_release);
		}
	}
}

TinyCounter::TinyCounter(JobSystemManager* manager)
	: BaseCounter(manager, 1, &m_waitingFiber, &m_freeWaitingSlot)
{ }

Counter::Counter(JobSystemManager* manager)
	: BaseCounter(manager, MAX_WAITING, m_impl_waitingFibers, m_impl_freeWaitingSlots)
{ }
