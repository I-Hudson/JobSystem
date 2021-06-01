#pragma once

#include "JobResult.h"

namespace Insight::JS
{
	struct IJobFuncWrapper
	{
		virtual ~IJobFuncWrapper() = default;
		virtual void Call() = 0;
	};

	template <typename ResultType, typename Func, typename... Args>
	class JobFuncWrapper : public IJobFuncWrapper
	{
	public:
		JobFuncWrapper(JobResult<ResultType>* jobResult, Func func, Args... args)
			: m_jobResult(jobResult)
			, m_func(func)
			, m_args(std::move(args)...)
		{ }

		virtual ~JobFuncWrapper() = default;

		virtual void Call() override
		{
			if constexpr (std::is_void_v<ResultType>)
			{
				std::apply(m_func, m_args);
			}
			else
			{
				m_jobResult->SetResult(std::apply(m_func, m_args));
			}
		}

	private:
		JobResult<ResultType>* m_jobResult;
		Func m_func;
		std::tuple<Args...> m_args;
	};
}