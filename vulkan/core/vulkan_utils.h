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

	/** vk memory allocator */
	class MemoryAllocator {
	public:
		void init(VkDevice device, VkDeviceSize bufferImageGranularity,
			const VkPhysicalDeviceMemoryProperties& memProperties, uint32_t defaultChunkSize = 268435000);
		/** @brief suballocation */
		VkResult allocate(VkBuffer buffer);

	private:
		/** small memory chunk reside in MemoryChunk */
		struct MemoryBlock {
			/** aka block start byte location */
			VkDeviceSize offset = 0;
			VkDeviceSize size = 0;
			VkDeviceSize alignment = 0;
			/** byte location divisible by alignment, also divisible by bufferImageGranularity */
			VkDeviceSize blockEndLocation = 0;
		};

		/** allocated memory block by vkAllocateMemory call */
		struct MemoryChunk {
			/** @brief return suitable memory location (offset) in current memory chunk */
			uint32_t findSuitableMemoryLocation(const VkMemoryRequirements& memRequirements);
			/** @brief sort memory block vector in real memory location order (offset) */
			void sort();

			VkDeviceMemory memoryHandle = VK_NULL_HANDLE;
			VkDeviceSize chunkSize = 0;
			VkDeviceSize currentSize = 0;
			std::vector<MemoryBlock> memoryBlocks;
		};

		/** allocated memory block by vkAllocateMemory */
		struct MemoryPool {
			/** @brief pre-allocate big chunk of memory */
			void allocateChunk(VkDevice device);
			/** @brief clean up all pre-allocated chunk of memory */
			void cleanup(VkDevice device);

			/** default block size -> 256 MiB */
			VkDeviceSize defaultChunkSize = 0;
			/** index of memory type defined in device memory properties */
			uint32_t memoryTypeIndex = -1;
			/** vector of pre-allocated memories */
			std::vector<MemoryChunk> memoryChunks;
		};

		/** logical device handle used for memory allocation */
		VkDevice device = VK_NULL_HANDLE;
		/** used for chunk size */
		VkDeviceSize bufferImageGranularity = 0;
		/** memory pools for each memory types */
		std::vector<MemoryPool> memoryPools;
	};
}
