#pragma once
#include <stdexcept>
#include <iostream>
#include <vector>
#include <fstream>
#include <vulkan/vulkan.h>

/*
* checks vulkan function result and throws runtime error if it is failed
* 
* @param func - function call to check
*/
#define VK_CHECK_RESULT(func){									\
	if (func != VK_SUCCESS) {									\
		std::cerr << __FILE__ << ", line " << __LINE__ << ": ";	\
		std::string str = #func;								\
		throw std::runtime_error(str + " call has been failed");\
	}															\
}

/*
* simple stderr print
* 
* @param str - string to print
*/
#define LOG(str){					\
	std::cerr << str << std::endl;	\
}

namespace vktools {
	/** @brief read binary file and store to a char vector */
	std::vector<char> readFile(const std::string& filename);
	/** @brief compile & create shader module */
	VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
	/** @brief transit image layout */
	void setImageLayout(VkCommandBuffer commandBuffer, VkImage image,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	/** @brief create & return image view */
	VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType viewType,
		VkFormat format, VkImageAspectFlags aspectFlags);
	/** @brief return suitable image format */
	VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features);
	/** @brief check if the format has stencil component */
	bool hasStencilComponent(VkFormat format);

	namespace initializers {
		inline VkBufferCreateInfo bufferCreateInfo(VkDeviceSize size,
			VkBufferUsageFlags usage, VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE) {
			VkBufferCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			info.size = size;
			info.usage = usage;
			info.sharingMode = sharingMode;
			return info;
		}
	}
}
