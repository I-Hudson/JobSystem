#include "JobSystem.h"
#include <iostream>
#include <Windows.h>

int main(int* argv, char** argc)
{
	JobSystemManagerOptions options;
	options.NumThreads = 1;

	JobSystemManager jobSystem(options);
	if (jobSystem.Init() != JobSystemManager::ReturnCode::Succes)
	{
		std::cout << "Something went wrong." << '\n';
	}

	while (true)
	{
		if (GetKeyState(VK_RETURN) & 0x8000)
		{
			jobSystem.WaitForSingle(JobPriority::Normal, []() 
			{
				std::cout << "Test print statement";
			});
		}

		if (GetKeyState(VK_SPACE) & 0x8000)
		{
			break;
		}
	}
	jobSystem.Shutdown(true);

	return 1;
}