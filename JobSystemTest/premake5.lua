project "JobSystemTest"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
	staticruntime "on"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
    debugdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")

    files
	{
		"src/**.h",
		"src/**.hpp",
        "src/**.cpp",
	}

    includedirs 
    {
		"$(ProjectDir)src",
        "%{IncludeDir.JobSystem}",
        "%{IncludeDir.readerwriterqueue}",
	}

    links
    {
        "JobSystem",
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
       symbols "on"


    filter "configurations:Release"
        optimize "on"

    filter "configurations:Dist"
        optimize "full"

    filter { "system:windows", "configurations:Release" }
        buildoptions "/MT"