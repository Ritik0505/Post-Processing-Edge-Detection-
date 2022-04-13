#pragma once
#define VK_NO_PROTOTYPES

#if defined _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32

#include "glfw3.h"
#include "glfw3native.h"
#include <windows.h>

#include"Vulkan/Vulkan.h"
#include<vector>
#include "Helper.h"

class VulkanBase :
    public VulkanHandles
{
	private:
		HMODULE VulkanLibrary;
		GLFWwindow* window;

		void	InitWindow();

		bool	LoadVulkanLibrary();
		bool	LoadExportedFunctions();
		bool	LoadInstanceLevelEntryPoints();
		bool	LoadGlobalLevelEntryPoints();
		bool	LoadDeviceLevelEntryPoints();
		bool	CheckExtensionsAvailability(const char* extension, const std::vector<VkExtensionProperties> & availability);
		bool	CreateVulkanInstance();
		bool	CheckPhysicalDeviceProperties(VkPhysicalDevice p_device, uint32_t & selectedGraphicsQueueFamilyIndex, uint32_t & selectedPresentationQueueFamilyIndex);
		bool	CreateLogicalDevice();
		bool	GetDeviceQueue();
		bool	CreatePresentationSurface();
		bool	CreateSemaphores();

		uint32_t	GetSwapChainImages(VkSurfaceCapabilitiesKHR & surfaceCapabilites);
		VkSurfaceFormatKHR	GetSwapChainFormat(std::vector<VkSurfaceFormatKHR> & surfaceFormats);
		VkExtent2D	GetSwapChainExtent(VkSurfaceCapabilitiesKHR & surfaceCapabilities);
		VkImageUsageFlags GetSwapChainUsageFlags(VkSurfaceCapabilitiesKHR & surfaceCapabilities);
		VkSurfaceTransformFlagBitsKHR GetSwapChainTransform(VkSurfaceCapabilitiesKHR & surfaceCapabilities);
		VkPresentModeKHR GetSwapChainPresentMode(std::vector<VkPresentModeKHR> & presentModes);

	public:
		VulkanBase();
		~VulkanBase();

		bool	prepareVulkan();
		bool	CreateSwapChain();
		bool	CreateSwapChainImageViews();
		//bool	CreateCommandPool();
		bool	CreateCommandBuffers();
		bool	RecordCommandBuffers();
		GLFWwindow* ReturnWindowHandle();
		void	Clear();
		bool	OnWindowSizeChanged();
		bool	Display();
		//bool	Draw();

	protected:
		//for vulkan handles 
};

