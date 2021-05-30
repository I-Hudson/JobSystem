#pragma once

#include <stdint.h>
#include <atomic>
#include <vector>

struct TLS
{
	TLS() = default;
	~TLS() = default;

	uint8_t ThreadIndex = UINT8_MAX;
	bool SetAffinity = false;
};