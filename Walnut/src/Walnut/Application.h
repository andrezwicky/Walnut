#pragma once

#include "Layer.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "vulkan/vulkan.h"
#include "Walnut/Types.h"


void check_vk_result(VkResult err);

struct GLFWwindow;



namespace Walnut
{

	class OffscreenImage;
	class OffscreenPipeline;



	class Application
	{
	public:
		Application(const ApplicationSpecification& applicationSpecification = ApplicationSpecification(), const OffscreenImageSpec& offscreenSpec = OffscreenImageSpec());
		~Application();

		static Application& Get();

		void Run();
		void SetMenubarCallback(const std::function<void()>& menubarCallback) { m_MenubarCallback = menubarCallback; }
		
		template<typename T>
		void PushLayer()
		{
			static_assert(std::is_base_of<Layer, T>::value, "Pushed type is not subclass of Layer!");
			m_LayerStack.emplace_back(std::make_shared<T>())->OnAttach();
		}

		void PushLayer(const std::shared_ptr<Layer>& layer) { m_LayerStack.emplace_back(layer); layer->OnAttach(); }

		void Close();

		float GetTime();
		GLFWwindow* GetWindowHandle() const { return m_WindowHandle; }
		
		OffscreenImage& GetOffScreenImage() { return *m_OffscreenImage; }
		OffscreenImageSpec& GetOffScreenSpec() { return m_OffscreenSpec; }
		OffscreenPipeline& GetOffScreenPipeline() { return *m_OffscreenPipeline; }


		static VkInstance GetInstance();
		static VkPhysicalDevice GetPhysicalDevice();
		static VkDevice GetDevice();
		VkCommandPool GetCommandPool();
		VkQueue GetQueue();

		static VkCommandBuffer GetCommandBuffer(bool begin);
		static VkCommandBuffer GetCommandBufferOffscreen(bool begin);
		static void FlushCommandBuffer(VkCommandBuffer commandBuffer);
		static void FlushCommandBufferOffscreen(VkCommandBuffer commandBuffer);

		static void SubmitResourceFree(std::function<void()>&& func);
	private:
		void Init();
		void Shutdown();
	private:
		ApplicationSpecification m_Specification;
		GLFWwindow* m_WindowHandle = nullptr;
		bool m_Running = false;

		OffscreenImage* m_OffscreenImage = nullptr;
		OffscreenPipeline* m_OffscreenPipeline = nullptr;
		OffscreenImageSpec m_OffscreenSpec;

		float m_TimeStep = 0.0f;
		float m_FrameTime = 0.0f;
		float m_LastFrameTime = 0.0f;

		std::vector<std::shared_ptr<Layer>> m_LayerStack;
		std::function<void()> m_MenubarCallback;
	};

	// Implemented by CLIENT
	Application* CreateApplication(int argc, char** argv);
}