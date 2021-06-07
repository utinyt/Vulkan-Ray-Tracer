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
	LOG("initialized:\tphysical device");
}

/*
* destroys logical device - must be called in application destructor
*/
void VulkanDevice::cleanup() {
	vkDestroyDevice(device, nullptr);
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
