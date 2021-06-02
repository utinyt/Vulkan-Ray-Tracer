#pragma once
#include "vulkan_utils.h"
#include "vulkan_device.h"

struct GLFWwindow;
class VulkanSwapchain {
public:
	VulkanSwapchain() {}
	void cleanup();
	void init(const VulkanDevice* devices, GLFWwindow* window);
	void create();

	/** swapchain handle */
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	/** swapchain image format & color space */
	VkSurfaceFormatKHR format;
	/** swapchain extent */
	VkExtent2D extent;
	/** swapchain image count */
	uint32_t imageCount = 0;
	/** swapchain image collection */
	std::vector<VkImage> images;
	/** swapchain image view collection */
	std::vector<VkImageView> imageViews;

private:
	/** abstracted vulkan device collection handle */
	const VulkanDevice* devices;
	/** glfw window handle */
	GLFWwindow* window = nullptr;
};