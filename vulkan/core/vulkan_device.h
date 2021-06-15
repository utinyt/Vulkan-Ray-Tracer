#pragma once
#include <optional>
#include "vulkan_utils.h"

struct VulkanDevice {
	VulkanDevice() {}
	void pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
		const std::vector<const char*>& requiredExtensions);
	void createLogicalDevice();
	void cleanup();
	void createCommandPool();
	
	/** @brief return suitable memory type */
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	/** @brief create buffer & buffer memory */
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;
	/** @brief copy data to another buffer */
	void copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer,
		VkDeviceSize size) const;
	/** @brief create image & image memory */
	void createImage(VkExtent3D extent, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) const;
	/** @brief copy data to an image */
	void copyBufferToImage(VkBuffer buffer, VkImage image, VkOffset3D offset, VkExtent3D extent) const;
	/** @brief create & start one-time submit command buffer */
	VkCommandBuffer beginOneTimeSubmitCommandBuffer() const;
	/** @brief submit command to the queue, end & destroy one-time submit command buffer */
	void endOneTimeSubmitCommandBuffer(VkCommandBuffer commandBuffer) const;

	/** GPU handle */
	VkPhysicalDevice physicalDevice;
	/** logical device handle */
	VkDevice device = VK_NULL_HANDLE;
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
	/** handle to the graphics queue */
	VkQueue graphicsQueue;
	/** handle to the present queue (usually the same as graphics queue)*/
	VkQueue presentQueue;
	/** memory properties of the current physical device */
	VkPhysicalDeviceMemoryProperties memProperties;
	/** current physical device properties */
	VkPhysicalDeviceProperties properties;
	/** available device features */
	VkPhysicalDeviceFeatures availableFeatures;
	/** command pool - graphics */
	VkCommandPool commandPool = VK_NULL_HANDLE;

	/** swapchain support details - used for swapchain creation*/
	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

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
