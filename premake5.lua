workspace "JobSystem"
    architecture "x64"
    startproject "JobSystemTest"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }
    
    flags
	{
		"MultiProcessorCompile"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["JobSystem"]         = "$(SolutionDir)JobSystem/inc/"

include "JobSystem"
include "JobSystemTest"