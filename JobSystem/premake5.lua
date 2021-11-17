project "JobSystem"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
	staticruntime "on"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")
    debugdir ("../bin/" .. outputdir .. "/%{prj.name}")

    files
	{
		"inc/**.h",
		"inc/**.hpp",
        "src/**.cpp",
	}

    includedirs 
    {
		"$(ProjectDir)inc",
	}

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
       symbols "on"


    filter "configurations:Release"
        optimize "on"

    filter "configurations:Dist"
        optimize "full"