#include <set>
#include "vulkan_device.h"

/*
* pick suitable physical device supporting given extensions, swapchain, queues
* 
* @param instance - vulkan instance to use
* @param surface - abstracted handle to the native surface, used for swapchain / queue support check
* @param requiredExtensions - list of required extensions from user
*/
void VulkanDevice::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, 
	const std::vector<const char*>& requiredExtensions) {
	this->surface = surface;
	this->requiredExtensions = requiredExtensions;

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("can't find GPUs with vulkan support");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	//check all physical devices
	for (const auto& device : devices) {
		QueueFamilyIndices srcIndices;
		SwapchainSupportDetails srcDetails;
		if (checkPhysicalDevice(device, surface, requiredExtensions, srcIndices, srcDetails)) {
			physicalDevice = device;
			indices = srcIndices;
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find suitable GPU");
	}

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &availableFeatures);

	LOG("initialized:\tphysical device");
}

/*
* destroys logical device - must be called in application destructor
*/
void VulkanDevice::cleanup() {
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyDevice(device, nullptr);
}

/*
* create command pool - use no flags
*/
void VulkanDevice::createCommandPool() {
	if (commandPool != VK_NULL_HANDLE) {
		throw std::runtime_error("VulkanDeivce::createCommandPool() called multiple times");
	}

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = indices.graphicsFamily.value();
	poolInfo.flags = 0;

	VK_CHECK_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));
	LOG("created:\tcommand pool");
}

/*
* find suitable memory type
* 
* @param typeFilter - compatible bit field corrensponding to memoryType
* @param properties - required memory properties
* 
* @return uint32_t - index of suitable memory type
*/
uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("VulkanDevice::findMemoryType() - failed to find suitable memory type");
}

/*
* creates logical device using given extensions in pickPhysicalDevice
*/
void VulkanDevice::createLogicalDevice() {
	if (device != VK_NULL_HANDLE) {
		throw std::runtime_error("VulkanDeivce::createLogicalDevice() called multiple times");
	}

	QueueFamilyIndices indices = findQueueFamilyIndices(physicalDevice, surface);
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(),
		indices.presentFamily.value() };

	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	float queuePriority = 1.f;

	for (uint32_t queueFamily : uniqueQueueFamilies) {
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = queueFamily;
		queueInfo.queueCount = 1; //don't really need more than one
		queueInfo.pQueuePriorities = &queuePriority;
		queueInfos.push_back(queueInfo);
	}

	VkDeviceCreateInfo deviceInfo{};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pQueueCreateInfos = queueInfos.data();
	deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	VkPhysicalDeviceFeatures deviceFeatures{};
	if (availableFeatures.samplerAnisotropy == VK_TRUE) {
		deviceFeatures.samplerAnisotropy = VK_TRUE;
	}
	deviceInfo.pEnabledFeatures = &deviceFeatures;
	deviceInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	deviceInfo.ppEnabledExtensionNames = requiredExtensions.data();
	deviceInfo.enabledLayerCount = 0;
	deviceInfo.ppEnabledLayerNames = nullptr;

	VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

	//get queue handles
	vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

	LOG("created:\tlogical device");
}

/*
* checks if current physical device is suitable (queues / extensions / swapchain support)
* 
* @param physicalDevice - current physical device to check
* @param surface - abstracted handle to the native surface, used to check swapchain / present queue support
* @param requiredExtensions - list of required device extensions providede by user
* @param dstIndices - used to return valid QueueFamilyIndices struct if all tests are passed
* @param dstSwapchainDetails - used to return valid SwapchainSupportDetails struct if all tests are passed
*/
bool VulkanDevice::checkPhysicalDevice(VkPhysicalDevice physicalDevice,
	VkSurfaceKHR surface,
	const std::vector<const char*>& requiredExtensions,
	QueueFamilyIndices& dstIndices, 
	SwapchainSupportDetails& dstSwapchainDetails) {
	//queue family indices
	QueueFamilyIndices indices = findQueueFamilyIndices(physicalDevice, surface);

	//extension support check
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	for (auto& requiredEXT : requiredExtensions) {
		auto extensionIt = std::find_if(availableExtensions.begin(), availableExtensions.end(),
			[&requiredEXT](const auto& availableEXT) {
				return strcmp(requiredEXT, availableEXT.extensionName) == 0;
			});

		if (extensionIt == availableExtensions.end()) {
			return false;
			break;
		}
	}

	//swapchain support check
	SwapchainSupportDetails details = querySwapchainSupport(physicalDevice, surface);
	bool swapchainSupport = !details.formats.empty() && !details.presentModes.empty();

	if (indices.isComplete() && swapchainSupport) {
		dstIndices = indices;
		dstSwapchainDetails = details;
		return true;
	}

	return false;
}

/*
* helper function for checkPhysicalDevice
* 
* @param physicalDevice - current physical device to check
* @param surface - abstracted handle to the native surface
* 
* @return QueueFamilyIndices - struct with valid queue family indices (graphics / present)
*/
VulkanDevice::QueueFamilyIndices VulkanDevice::findQueueFamilyIndices(VkPhysicalDevice physicalDevice,
	VkSurfaceKHR surface) {
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	//TODO: find one queue family supporting both (graphics / present)
	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		//graphics family
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		//present family
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
		if (presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.isComplete()) {
			break;
		}

		i++;
	}

	return indices;
}

/*
* helper function for checkPhysicalDevice, returns valid SwapchainSupportDetails struct
* 
* @param physicalDevice - current physical device to check
* @param surface - abstracted handle to the native surface
* 
* @return SwapchainSupportDetails - struct with vaild queue family indices
*/
VulkanDevice::SwapchainSupportDetails VulkanDevice::querySwapchainSupport(
	VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
	SwapchainSupportDetails details{};

	//surface capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

	//surface formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
	if (formatCount) {
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
	}

	//present modes
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	if (presentModeCount) {
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount,
			details.presentModes.data());
	}

	return details;
}

/*
* compile & create shader module
*
* @param size - required buffer size
* @param usage - required buffer usage
* @param properties - bit field specifying required memory properties
* @param buffer - return buffer handle
* @param bufferMemory - return buffer memory handle
*/
void VulkanDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	VkBuffer& buffer, VkDeviceMemory& bufferMemory) const {
	//buffer creation
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));

	//buffer allocation
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memRequirements.size;
	memAllocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
	VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &bufferMemory));
	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

/*
* copy data to another buffer
*
* @param srcBuffer - source buffer to be copied
* @param dstBuffer - destination buffer
* @param size - buffer size to copy
*/
void VulkanDevice::copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer,
	VkDeviceSize size) const {
	VkCommandBuffer commandBuffer = beginOneTimeSubmitCommandBuffer();

	//copy buffer
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endOneTimeSubmitCommandBuffer(commandBuffer);
}

/*
* create & allocate image
* 
* @param extent - image extent 3d
* @param format - image format
* @param tiling - image tiling mode
* @param usage - image usage bits
* @param properties - image memory property bits
* @param image - return image handle
* @param imageMemory - return image memory handle
*/
void VulkanDevice::createImage(VkExtent3D extent, VkFormat format, VkImageTiling tiling,
	VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) const{
	//image creation
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	if (extent.depth == 1) {
		if (extent.height == 1) {
			imageInfo.imageType = VK_IMAGE_TYPE_1D;
		}
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
	}
	else {
		imageInfo.imageType = VK_IMAGE_TYPE_3D;
	}
	imageInfo.extent = extent;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &image));

	//image memory allocation
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
	VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory));

	vkBindImageMemory(device, image, imageMemory, 0);
}

void VulkanDevice::copyBufferToImage(VkBuffer buffer, VkImage image,
	VkOffset3D offset, VkExtent3D extent) const {
	VkCommandBuffer commandBuffer = beginOneTimeSubmitCommandBuffer();
	
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = offset;
	region.imageExtent = extent;

	vkCmdCopyBufferToImage(
		commandBuffer,
		buffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // current layout of the image
		1,
		&region
	);

	endOneTimeSubmitCommandBuffer(commandBuffer);
}

/*
* create & begin one-time submit command buffer
* 
* @return command buffer ready to be recorded
*/
VkCommandBuffer VulkanDevice::beginOneTimeSubmitCommandBuffer() const {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	return commandBuffer;
}

/*
* submit command buffer to the queue
* end & destroy one-time submit command buffer
* 
* @param commandBuffer - recorded command buffer to submit
*/
void VulkanDevice::endOneTimeSubmitCommandBuffer(VkCommandBuffer commandBuffer) const {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(graphicsQueue));
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}
