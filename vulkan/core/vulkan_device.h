#pragma once
#include <optional>
#include "vulkan_utils.h"

struct VulkanDevice {
	VulkanDevice() {}
	void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
		const std::vector<const char*>& requiredExtensions);
	void createLogicalDevice();
	void cleanup();

	/** GPU handle */
	VkPhysicalDevice physicalDevice;
	/** logical device handle */
	VkDevice device;
	/** abstracted handle for native platform surface */
	VkSurfaceKHR surface;
	/** list of required device extensions */
	std::vector<const char*> requiredExtensions;
	/** collection of queue family indices */
	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete() const {
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	} indices;
	/** swapchain support details - used for swapchain creation*/
	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	} swapchainDetails;
	/** handle to the graphics queue */
	VkQueue graphicsQueue;
	/** handle to the present queue (usually the same as graphics queue)*/
	VkQueue presentQueue;

private:
	static bool checkPhysicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
		const std::vector<const char*>& requiredExtensions,
		QueueFamilyIndices& dstIndices, SwapchainSupportDetails& dstSwapchainDetails);
	static QueueFamilyIndices findQueueFamilyIndices(VkPhysicalDevice physicalDevice,
		VkSurfaceKHR surface);
	static SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice physicalDevice,
		VkSurfaceKHR surface);
};