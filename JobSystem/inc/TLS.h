#pragma once

#include <stdint.h>
#include <atomic>
#include <vector>
#include "Fiber.h"

enum class FiberDestination : uint8_t
{
	None,
	Waiting,
	Pool
};

struct TLS
{
	TLS() = default;
	~TLS() = default;

	uint8_t ThreadIndex = UINT8_MAX;
	bool SetAffinity = false;

	Fiber ThreadFiber;

	uint16_t CurrentFiberIndex = UINT16_MAX;

	uint16_t PreviousFiberIndex = UINT16_MAX;
	std::atomic_bool* PreviousFiberStored = nullptr;
	FiberDestination PreviousFiberDestination = FiberDestination::None;

	std::vector<std::pair<uint16_t, std::atomic_bool*>> ReadyFibers;
};