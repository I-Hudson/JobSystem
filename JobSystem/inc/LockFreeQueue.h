#pragma once

#include <assert.h>
#include <memory>

// Source: Dmitry Vyukov's MPMC
// http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue

namespace Insight::JS
{
	class IJob;

	template<typename T>
	class LockFreeQueue
	{
	public:
		LockFreeQueue(size_t buffer_size)
			: buffer_(new cell_t[buffer_size])
			, buffer_mask_(buffer_size - 1)
			, m_capacity(buffer_size)
		{
			assert((buffer_size >= 2) && ((buffer_size & (buffer_size - 1)) == 0));
			for (size_t i = 0; i != buffer_size; i += 1)
				buffer_[i].sequence_.store(i, std::memory_order_relaxed);
			enqueue_pos_.store(0, std::memory_order_relaxed);
			dequeue_pos_.store(0, std::memory_order_relaxed);
		}

		~LockFreeQueue()
		{
			delete[] buffer_;
		}

		uint32_t size()
		{
			return m_size.load(memory_order_acquire);
		}

		uint32_t capacity() const { return m_capacity; }

		bool enqueue(const T& data)
		{
			cell_t* cell;
			size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
			for (;;)
			{
				cell = &buffer_[pos & buffer_mask_];
				size_t seq =
					cell->sequence_.load(std::memory_order_acquire);
				intptr_t dif = (intptr_t)seq - (intptr_t)pos;
				if (dif == 0)
				{
					if (enqueue_pos_.compare_exchange_weak
					(pos, pos + 1, std::memory_order_relaxed))
						break;
				}
				else if (dif < 0)
					return false;
				else
					pos = enqueue_pos_.load(std::memory_order_relaxed);
			}
			cell->data_ = data;
			cell->sequence_.store(pos + 1, std::memory_order_release);
			m_size.fetch_add(1, std::memory_order_release);
			return true;
		}

		bool dequeue(T& data)
		{
			cell_t* cell;
			size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
			for (;;)
			{
				cell = &buffer_[pos & buffer_mask_];
				size_t seq =
					cell->sequence_.load(std::memory_order_acquire);
				intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
				if (dif == 0)
				{
					if (dequeue_pos_.compare_exchange_weak
					(pos, pos + 1, std::memory_order_relaxed))
						break;
				}
				else if (dif < 0)
					return false;
				else
					pos = dequeue_pos_.load(std::memory_order_relaxed);
			}
			data = cell->data_;
			// As we are using sharedpointer make sure we are removing strong references where we need to.
			// Make sure to reset the shared pointer in the queue.
			if (std::is_same_v<std::shared_ptr<IJob>, T>)
			{
				cell->data_ = { };
			}
			cell->sequence_.store
			(pos + buffer_mask_ + 1, std::memory_order_release);
			m_size.fetch_sub(1, std::memory_order_release);
			return true;
		}

	private:
		struct cell_t
		{
			std::atomic<size_t>   sequence_;
			T                     data_;
		};

		static size_t const     cacheline_size = 64;
		typedef char            cacheline_pad_t[cacheline_size];

		size_t				m_capacity;
		std::atomic<size_t>     m_size;

		cacheline_pad_t         pad0_;
		cell_t* const           buffer_;
		size_t const            buffer_mask_;
		cacheline_pad_t         pad1_;
		std::atomic<size_t>     enqueue_pos_;
		cacheline_pad_t         pad2_;
		std::atomic<size_t>     dequeue_pos_;
		cacheline_pad_t         pad3_;
	};
}