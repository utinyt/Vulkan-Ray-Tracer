#pragma once
#include <stdexcept>
#include <iostream>
#include <vector>
#include <fstream>
#include <vulkan/vulkan.h>
#include "glm/mat4x4.hpp"

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

/*
* proxy functions (extension)
*/
namespace vkfp {
	VkResult vkCreateAccelerationStructureKHR(
		VkDevice                                    device,
		const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkAccelerationStructureKHR* pAccelerationStructure);

	void vkGetAccelerationStructureBuildSizesKHR(
		VkDevice                                    device,
		VkAccelerationStructureBuildTypeKHR         buildType,
		const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo,
		const uint32_t* pMaxPrimitiveCounts,
		VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo);

	void vkCmdBuildAccelerationStructuresKHR(
		VkCommandBuffer                             commandBuffer,
		uint32_t                                    infoCount,
		const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
		const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos);

	void vkCmdWriteAccelerationStructuresPropertiesKHR(
		VkCommandBuffer                             commandBuffer,
		uint32_t                                    accelerationStructureCount,
		const VkAccelerationStructureKHR* pAccelerationStructures,
		VkQueryType                                 queryType,
		VkQueryPool                                 queryPool,
		uint32_t                                    firstQuery);

	void vkCmdCopyAccelerationStructureKHR(
		VkCommandBuffer                             commandBuffer,
		const VkCopyAccelerationStructureInfoKHR* pInfo);

	void vkDestroyAccelerationStructureKHR(
		VkDevice                                    device,
		VkAccelerationStructureKHR                  accelerationStructure,
		const VkAllocationCallbacks* pAllocator);

	VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(
		VkDevice                                    device,
		const VkAccelerationStructureDeviceAddressInfoKHR* pInfo);

	void init(VkInstance instance);
}

namespace vktools {
	/** @brief read binary file and store to a char vector */
	std::vector<char> readFile(const std::string& filename);
	/** @brief compile & create shader module */
	VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
	/** @brief transit image layout */
	void setImageLayout(VkCommandBuffer commandBuffer, VkImage image,
		VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
	/** @brief create & return image view */
	VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType viewType,
		VkFormat format, VkImageAspectFlags aspectFlags);
	/** @brief return suitable image format */
	VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features);
	/** @brief check if the format has stencil component */
	bool hasStencilComponent(VkFormat format);
	/** @brief get buffer address */
	VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer);
	/** @brief convert glm mat4 to VkTransformMatrixKHR */
	VkTransformMatrixKHR toTransformMatrixKHR(const glm::mat4& mat);
	/** @brief create renderpass */
	VkRenderPass createRenderPass(VkDevice device,
		const std::vector<VkFormat>& colorAttachmentFormats,
		VkFormat depthAttachmentFormat,
		uint32_t subpassCount = 1,
		bool clearColor = true,
		bool clearDepth = true,
		VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VkPipelineStageFlags stageFlags = 
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VkAccessFlags dstAccessMask = 
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | 
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

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

		inline VkImageCreateInfo imageCreateInfo(VkExtent3D extent,
			VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage) {
			VkImageCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			if (extent.depth == 1) {
				if (extent.height == 1) {
					info.imageType = VK_IMAGE_TYPE_1D;
				}
				info.imageType = VK_IMAGE_TYPE_2D;
			}
			else {
				info.imageType = VK_IMAGE_TYPE_3D;
			}
			info.extent = extent;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.format = format;
			info.tiling = tiling;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			info.usage = usage;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			return info;
		}

		inline VkSamplerCreateInfo samplerCreateInfo(
			VkPhysicalDeviceFeatures2 availableFeatures,
			VkPhysicalDeviceProperties properties,
			VkFilter filter = VK_FILTER_LINEAR,
			VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE) {

			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.minFilter = filter;
			samplerInfo.magFilter = filter;
			samplerInfo.addressModeU = mode;
			samplerInfo.addressModeV = mode;
			samplerInfo.addressModeW = mode;
			if (availableFeatures.features.samplerAnisotropy == VK_TRUE) {
				samplerInfo.anisotropyEnable = VK_TRUE;
				samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
			}
			else {
				samplerInfo.anisotropyEnable = VK_FALSE;
				samplerInfo.maxAnisotropy = 1.f;
			}
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = 0.f;
			samplerInfo.minLod = 0.f;
			samplerInfo.maxLod = 0.f;
			return samplerInfo;
		}
	}
}
