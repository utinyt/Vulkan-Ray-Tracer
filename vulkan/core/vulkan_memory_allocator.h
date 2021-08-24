#pragma once
#include "vulkan_utils.h"

/*
	* Custom memory allocator is implemented to deal with device memory allocation limit in a very NAIVE way
	* There is no defragmentation feature or fancy algorithm for memory block searching
*/
class MemoryAllocator {
public:
	/** contain all info needed for data mapping */
	struct HostVisibleMemory {
	public:
		/** @brief memcpy bufferData to device memory */
		void MapData(VkDevice device, const void* bufferData);
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkDeviceSize size = 0;
		VkDeviceSize offset = 0;
	};

	void init(VkDevice device, VkDeviceSize bufferImageGranularity,
		const VkPhysicalDeviceMemoryProperties& memProperties, uint32_t defaultChunkSize = 268435000); //256 MiB
	/** @brief free all allocated memory */
	void cleanup();
	/** @brief suballocation - add new memory block */
	HostVisibleMemory allocateMemory(VkBuffer buffer, VkMemoryPropertyFlags properties);
	/** @brief free memory block */
	void freeMemory(VkBuffer buffer, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM);
	/** @brief return suitable memory type */
	static uint32_t findMemoryType(uint32_t memoryTypeBitsRequirements,
		VkMemoryPropertyFlags requiredProperties, const VkPhysicalDeviceMemoryProperties& memProperties);

private:
	/** small memory chunk reside in MemoryChunk */
	struct MemoryBlock {
		VkBuffer bufferHandle = VK_NULL_HANDLE;
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
		bool findSuitableMemoryLocation(const VkMemoryRequirements& memRequirements,
			VkDeviceSize bufferImageGranularity, MemoryBlock& memoryBlock);
		/** @brief sort memory block vector in real memory location order (offset) */
		void sort();
		/** @brief add new memory block to this memory chunk */
		void addMemoryBlock(VkDevice device, VkBuffer buffer, MemoryBlock& memoryBlock);

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

	/** device memory properties */
	VkPhysicalDeviceMemoryProperties memProperties;
	/** logical device handle used for memory allocation */
	VkDevice device = VK_NULL_HANDLE;
	/** used for chunk size */
	VkDeviceSize bufferImageGranularity = 0;
	/** memory pools for each memory types */
	std::vector<MemoryPool> memoryPools;

	/** @brief helper function for freeMemory */
	bool findAndEraseMemoryBlock(VkBuffer buffer, uint32_t memoryTypeIndex);
};