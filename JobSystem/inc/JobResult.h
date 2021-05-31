#pragma once

#include <assert.h>

namespace js
{
	template<typename ResultType, bool>
	struct IJobResult
	{
		IJobResult()
		{ }

		bool IsReady() const { return true; }
		ResultType GetResult() const { /*assert(false && "[JobResult<void>::GetResult] Can not get the result of a void JobResult.");*/ }
	};

	template<typename ResultType>
	struct IJobResult<ResultType, false>
	{
		IJobResult()
			: m_isReady(false)
		{ }
		
		void SetResult(ResultType resultType)
		{
			m_result = resultType;
			m_isReady = true;
		}
		
		bool IsReady() const { return m_isReady; }
		ResultType GetResult() const { return m_result; }

	private:
		ResultType m_result;
		bool m_isReady;
	};

	template<typename ResultType>
	class JobResult : public IJobResult<ResultType, std::is_void_v<ResultType>> { };
}