#include "JobSystem.h"
#include <iostream>
#include <Windows.h>

//the following are UBUNTU/LINUX, and MacOS ONLY terminal color codes.
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

std::string gen_random(const int len) {

	std::string tmp_s;
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	srand((unsigned)time(NULL) * _getpid());

	tmp_s.reserve(len);

	for (int i = 0; i < len; ++i) 
	{
		tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
	}


	return tmp_s;

}

std::vector<std::string> FillVector(std::string str)
{
	std::vector<std::string> tmp;
	for (size_t i = 0; i < 16; i++)
	{
		if (!str.empty())
		{
			tmp.push_back(str);
		}
		else
		{
			tmp.push_back(gen_random(12));
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	return tmp;
}

int main(int* argv, char** argc)
{
	using namespace Insight;

	JS::JobSystemManagerOptions options;
	options.NumThreads = 1;

	JS::JobSystemManager jobSystem(options);
	if (jobSystem.Init() != JS::JobSystemManager::ReturnCode::Succes)
	{
		std::cout << "Something went wrong." << '\n';
	}

	std::shared_ptr<JS::JobSystem> localJS = jobSystem.CreateLocalJobSystem(1);

	bool addingJobs = false;
	while (true)
	{
		if (GetKeyState(VK_RETURN) & 0x8000)
		{
			//if (jobSystem.GetPendingJobsCount() > 0)
			//{
			//	continue;
			//}

			std::vector<std::string> modles = FillVector("MODULES - 0");
			std::vector<std::string> modles1 = FillVector("MODULES - 1");
			std::vector<std::string> modles2 = FillVector("MODULES - 2");
			std::vector<std::string> modles3 = FillVector("MODULES - 3");
			localJS->Release();
			jobSystem.ReseveThreads(1);
			auto startingJob = jobSystem.CreateJob(JS::JobPriority::Normal, [modles]() -> void
			{
				for (auto& str : modles)
				{
					std::cout << str << '\n';
				}
			});
			Insight::JS::JobWithResultSharedPtr<float> endJob = startingJob->Then([]()
			{
				return 42.7865413f;
			});

			jobSystem.ScheduleJob(startingJob);
			std::cout << "Job result is ready: " << endJob->IsReady() << '\n';
			std::cout << "Job result: " << endJob->GetResult().GetResult() << '\n';
/*			auto job1 = jobSystem.CreateJob(JS::JobPriority::Normal, [&modles1]()
			{
				for (auto& str : modles1)
				{
					std::cout << str << '\n';
				}
				return 1.0f;
			});
			auto job2 = jobSystem.CreateJob(JS::JobPriority::Normal, [&modles2]()
			{
				for (auto& str : modles2)
				{
					std::cout << str << '\n';
				}
				return 1;
			});			
			auto job3 = jobSystem.CreateJob(JS::JobPriority::Normal, [&modles3]()
			{
				for (auto& str : modles3)
				{
					std::cout << str << '\n';
				}
			});		*/	
			//jobSystem.ScheduleJob(job);
			//jobSystem.ScheduleJob(job3);
			//jobSystem.ScheduleJob(job2);
			//jobSystem.ScheduleJob(job1);
			//JS::JobWaitList waitList;
			//waitList.AddJobToWaitOn(job);
			//waitList.AddJobToWaitOn(job1);
			//waitList.AddJobToWaitOn(job2);
			//waitList.AddJobToWaitOn(job3);
			//waitList.Wait();

			/*JobPtr job = jobSystem.CreateJob(JobPriority::Low, []()
			{
				std::cout << MAGENTA << "worker thread output" << RESET << '\n';
				std::cout << '\t' << GREEN << "Thread ID: " << std::this_thread::get_id() << RESET << '\n';
			});
			for (size_t i = 0; i < 500; ++i)
			{
				job->Then([]()
				{
					std::cout << MAGENTA << "worker thread output. Second" << RESET << '\n';
					std::cout << '\t' << GREEN << "Thread ID: " << std::this_thread::get_id() << RESET << '\n';
				});
			}
			jobSystem.ScheduleJob(job);
			job->Wait();*/
		}

		if (GetKeyState(VK_SHIFT) & 0x8000)
		{
			if (localJS->GetPendingJobsCount() > 0)
			{
				continue;
			}
			localJS->Release();

 			std::vector<std::string> modles = FillVector("Local thread job");
			for (size_t i = 0; i < 1000; ++i)
			{
				auto job = localJS->CreateJob(JS::JobPriority::Normal, [modles]()
							  {
								  for (auto& str : modles)
								  {
									  std::cout << str << '\n';
								  }
							  });
				localJS->ScheduleJob(job);
			}
			localJS->ReserveThreads(1);
			localJS->WaitForAll();
		}

		if (GetKeyState(VK_NUMPAD0) & 0x8000)
		{
			//JS::JobSystem js;
			//js = localJS;
		}


		if (GetKeyState(VK_SPACE) & 0x8000)
		{
			break;
		}

		//std::cout << "Update Loop" << '\n';
		jobSystem.Update(1);
	}
	localJS->Release();

	std::cout << "Local remaning jobs: " << localJS->GetPendingJobsCount() << '\n';
	std::cout << "Main remaning jobs: " << jobSystem.GetPendingJobsCount() << '\n';

	jobSystem.Shutdown(true);
	//job.reset();

	return 1;
}