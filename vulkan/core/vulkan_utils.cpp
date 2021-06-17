#include "vulkan_utils.h"

namespace vktools {
	/*
	* read binary file
	*
	* @param filename - name of the file
	* @return raw binary data stored in a vector
	*/
	std::vector<char> readFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("failed to open file: " + filename);
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	/*
	* create shader module
	*
	* @param code - compiled shader code (raw binary data, .spv)
	* @return VkShaderModule - constructed shader module
	*/
	VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
		VkShaderModuleCreateInfo shaderModuleInfo{};
		shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderModuleInfo.codeSize = code.size();
		shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		VK_CHECK_RESULT(vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &shaderModule));
		return shaderModule;
	}

	/*
	* set pipeline barrier for image layout transition
	* record command to the command buffer
	*
	* @param commandBuffer - command buffer to record
	* @param image - image to transit layout
	* @param format
	* @param oldLayout
	* @param newLayout
	*/
	void setImageLayout(VkCommandBuffer commandBuffer, VkImage image,
		VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkImageMemoryBarrier imageBarrier{};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.oldLayout = oldLayout;
		imageBarrier.newLayout = newLayout;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.image = image;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.levelCount = 1;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags srcStage;
		VkPipelineStageFlags dstStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			imageBarrier.srcAccessMask = 0;
			imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			std::invalid_argument("VulkanTextureBase::setImageLayout(): unsupported layout transition");
		}

		vkCmdPipelineBarrier(commandBuffer,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier);
	}

	/*
	* create & return image view
	* 
	* @param image - image handle
	* @param viewtype - specify image view type (1D / 2D / 3D / ...)
	* @param format
	* @return VkImageView - created image view
	*/
	VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType viewType,
		VkFormat format, VkImageAspectFlags aspectFlags) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = viewType;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &imageView));
		return imageView;
	}

	/*
	* find suitable image format
	* 
	* @param physicalDevice
	* @param candidates - list of formats to choose from
	* @param tiling - image tiling mode
	* @param features - format feature
	* @return VkFormat - suitable format
	*/
	VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice,
		const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : candidates) {
			VkFormatProperties properties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

			if (tiling == VK_IMAGE_TILING_LINEAR &&
				(properties.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
				(properties.optimalTilingFeatures & features) == features) {
				return format;
			}
		}
		throw std::runtime_error("findSupportedFormat(): can't find supported format");
	}

	/*
	* check if the format has stencil component
	* 
	* @param format
	* @return bool
	*/
	bool hasStencilComponent(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
			format == VK_FORMAT_D24_UNORM_S8_UINT;
	}
}
