#pragma once
#include "utils.h"

VkResult createDebugUtilsMessengerEXT(VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator);

void destroyDebugUtilsMessengerEXT(VkInstance instance,
	const VkAllocationCallbacks* pAllocator);

void setupDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
