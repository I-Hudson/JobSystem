#include "Job.h"

#include <windows.h>

IJob::IJob(std::unique_ptr<IJobFuncWrapper> funcWrapper)
	: m_state(JobState::Queued)
	, m_parentJob(nullptr)
	, m_funcWrapper(std::move(funcWrapper))
{ }

IJob::IJob(std::unique_ptr<IJobFuncWrapper> funcWrapper, IJob* parentJob)
	: m_state(JobState::Queued)
	, m_parentJob(parentJob)
	, m_funcWrapper(std::move(funcWrapper))
{ }

IJob::~IJob()
{
	m_parentJob = nullptr;
	m_childrenJobs.clear();
	m_funcWrapper.reset();
}

void IJob::Call()
{
	m_state.store(JobState::Running);

	m_funcWrapper->Call();

	if (m_counter) 
	{
		m_counter->Decrement();
	}

	m_state.store(JobState::Finished);
}
