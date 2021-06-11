#pragma once
#include <optional>
#include "vulkan_utils.h"

struct VulkanDevice {
	VulkanDevice() {}
	void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
		const std::vector<const char*>& requiredExtensions);
	void createLogicalDevice();
	void cleanup();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	/** @brief create buffer & buffer memory */
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
	/** @brief copy data to another buffer */
	void copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer,
		VkDeviceSize size) const;

	/** GPU handle */
	VkPhysicalDevice physicalDevice;
	/** logical device handle */
	VkDevice device;
	/** abstracted handle for native platform surface */
	VkSurfaceKHR surface;
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
	};
	/** handle to the graphics queue */
	VkQueue graphicsQueue;
	/** handle to the present queue (usually the same as graphics queue)*/
	VkQueue presentQueue;
	/** memory properties of the current physical device */
	VkPhysicalDeviceMemoryProperties memProperties;

	static SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice physicalDevice,
		VkSurfaceKHR surface);

private:
	static bool checkPhysicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
		const std::vector<const char*>& requiredExtensions,
		QueueFamilyIndices& dstIndices, SwapchainSupportDetails& dstSwapchainDetails);
	static QueueFamilyIndices findQueueFamilyIndices(VkPhysicalDevice physicalDevice,
		VkSurfaceKHR surface);
	

	/** list of required device extensions */
	std::vector<const char*> requiredExtensions;
};