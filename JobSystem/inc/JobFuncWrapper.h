#pragma once

struct IJobFuncWrapper
{
	virtual ~IJobFuncWrapper() = default;
	virtual void Call() = 0;
};

template <typename Func, typename... Args>
class JobFuncWrapper : public IJobFuncWrapper
{
public:
	JobFuncWrapper(Func func, Args... args)
		: m_func(func)
		, m_args(std::move(args)...)
	{ }

	virtual ~JobFuncWrapper() = default;

	virtual void Call() override
	{
		std::apply(m_func, m_args);
	}

private:
	Func m_func;
	std::tuple<Args...> m_args;
};