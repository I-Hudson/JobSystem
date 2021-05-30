#pragma once

#include "TLS.h"
#include <thread>

/// <summary>
/// Single CPU thread.
/// </summary>
class Thread
{
public:
	using Callback = void(*)(Thread*);

	Thread() = default;
	Thread(const Thread&) = delete;
	virtual ~Thread() = default; // Note: destructor does not despawn Thread

	// Spawns Thread with given Callback & Userdata
	bool Spawn(Callback callback, void* userData = nullptr);
	void SetAffinity(size_t i);

	// Waits for Thread
	void Join();

	// Getter
	inline TLS* GetTLS() { return &m_tls; };
	inline Callback GetCallback() const { return m_callback; };
	inline void* GetUserdata() const { return m_userData; };
	inline bool HasSpawned() const { return m_id != std::thread::id(); };
	inline const std::thread::id GetID() const { return m_id; };

	// Static Methods
	static void SleepFor(uint32_t ms);

private:
	Thread(std::thread handle, std::thread::id id)
		: m_handle(std::move(handle)), m_id(id)
	{ }

private:
	std::thread m_handle;
	std::thread::id m_id;
	TLS m_tls;

	Callback m_callback = nullptr;
	void* m_userData = nullptr;
};