project "ImGuiTextSelect"
   kind "StaticLib"
   language "C++"
   cppdialect "C++17"
   staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

   files
   {
      "textselect.cpp",
      "textselect.hpp"
   }

   includedirs
   {
      "../imgui",
      "../utfcpp"
   }

   filter "system:windows"
      systemversion "latest"
      defines
      {
         "_CRT_SECURE_NO_WARNINGS"
      }

   filter "configurations:Debug"
      runtime "Debug"
      symbols "on"

   filter "configurations:Release"
      runtime "Release"
      optimize "on"

   filter "configurations:Dist"
      runtime "Release"
      optimize "on"