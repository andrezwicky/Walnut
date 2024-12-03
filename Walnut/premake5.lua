project "Walnut"
   kind "StaticLib"
   language "C++"
   cppdialect "C++17"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files { "src/**.h", "src/**.cpp" }

   includedirs
   {
      "src",
      "res",

      "../vendor/imgui",
      "../vendor/glfw/include",
      "../vendor/stb_image",
      "../vendor/ImGuiTextSelect",
      "../vendor/utfcpp",
      "../vendor/nfd",

      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.glm}",
      "%{IncludeDir.yaml_cpp}"
   }

   links
   {
       "ImGui",
       "GLFW",
       "yaml-cpp",
       "ImGuiTextSelect",
       "nfd", "ole32", "uuid", "shell32",

       "%{Library.Vulkan}"
   }

   targetdir ("bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   filter "system:windows"
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS", "YAML_CPP_STATIC_DEFINE" }

   filter "configurations:Debug"
      defines { "WL_DEBUG" }
      libdirs { "../vendor/lib/debug" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "WL_RELEASE" }
      libdirs { "../vendor/lib/release" }
      runtime "Release"
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      defines { "WL_DIST" }
      libdirs { "../vendor/lib/release" }
      runtime "Release"
      optimize "On"
      symbols "Off"