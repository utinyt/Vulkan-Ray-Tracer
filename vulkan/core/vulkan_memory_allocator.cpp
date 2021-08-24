#include <algorithm>
#include "vulkan_memory_allocator.h"

/*
* get device & chunk size info
*
* @param device - logical device handle
* @param bufferImageGranularity
*/
void MemoryAllocator::init(VkDevice device, VkDeviceSize bufferImageGranularity,
	const VkPhysicalDeviceMemoryProperties& memProperties, uint32_t defaultChunkSize) {
	this->memProperties = memProperties;
	this->device = device;
	this->bufferImageGranularity = bufferImageGranularity;
	memoryPools.resize(memProperties.memoryTypeCount);

	//assign memory type index & chunk size to individual memory pool
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
		uint32_t heapIndex = memProperties.memoryTypes[i].heapIndex;
		VkDeviceSize heapSize = memProperties.memoryHeaps[heapIndex].size;

		//chunk size
		if (heapSize < 1000000000) { //1GB
			memoryPools[i].defaultChunkSize = heapSize / 8;
		}
		else {
			memoryPools[i].defaultChunkSize = defaultChunkSize;
		}

		//memory type index
		memoryPools[i].memoryTypeIndex = i;
	}
}

/*
* (sub)allocate to pre-allocated memory
*
* @param buffer - buffer handle to allocate (bind) memory
* @param properties - buffer properties needed for memory type search
* 
* @return VkResult
*/
VkResult MemoryAllocator::allocateMemory(VkBuffer buffer, VkMemoryPropertyFlags properties) {
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
	uint32_t memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties, memProperties);

	MemoryPool& pool = memoryPools[memoryTypeIndex];
	//find suitable memory chunk
	for (size_t i = 0; i < pool.memoryChunks.size(); ++i) {
		if (pool.memoryChunks[i].currentSize > memRequirements.size) {
			MemoryBlock memoryBlock{};
			if (pool.memoryChunks[i].findSuitableMemoryLocation(memRequirements, bufferImageGranularity, memoryBlock)) {
				pool.memoryChunks[i].addMemoryBlock(device, buffer, memoryBlock);
				return VK_SUCCESS;
			}
		}
	}

	//failed to find suitable memory location - add new memory chunk
	pool.allocateChunk(device);
	MemoryBlock memoryBlock{};
	if (pool.memoryChunks.back().findSuitableMemoryLocation(memRequirements, bufferImageGranularity, memoryBlock)) {
		pool.memoryChunks.back().addMemoryBlock(device, buffer, memoryBlock);
		return VK_SUCCESS;
	}

	return VK_ERROR_OUT_OF_POOL_MEMORY;
}

/*
* free memory block
* 
* @param buffer - buffer to be deallocated
* @param properties - used to identify memory type; 
*	if it is not provided then memory allocator should search the whole memory pools
*/
void MemoryAllocator::freeMemory(VkBuffer buffer, VkMemoryPropertyFlags properties) {
	if (properties == VK_MEMORY_PROPERTY_FLAG_BITS_MAX_ENUM) {
		//memory properties are not provided; search the whole memory pools
		for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < memoryPools.size(); ++memoryTypeIndex) {
			if (findAndEraseMemoryBlock(buffer, memoryTypeIndex)) {
				return;
			}
		}
	}
	else {
		//identify memory type
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
		uint32_t memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties, memProperties);

		if (findAndEraseMemoryBlock(buffer, memoryTypeIndex)) {
			return;
		}
	}

	throw std::runtime_error("MemoryAllocator::freeMemory(): there is no matching buffer");
}

/*
* free all allocated memory
*/
void MemoryAllocator::cleanup() {
	for (auto& memoryPool : memoryPools) {
		memoryPool.cleanup(device);
	}
}

/*
* find a memory in 'memoryTypeBitsRequirements' that includes all of 'requiredProperties'
*
* @param memoryTypeBitsRequirements- bitfield that sets a bit for every memory type that is supported for the resource
* @param requiredProperties - required memory properties
*
* @return uint32_t - index of suitable memory type
*/
uint32_t MemoryAllocator::findMemoryType(uint32_t memoryTypeBitsRequirements,
	VkMemoryPropertyFlags requiredProperties, const VkPhysicalDeviceMemoryProperties& memProperties) {
	for (uint32_t memoryTypeIndex = 0; memoryTypeIndex < memProperties.memoryTypeCount; ++memoryTypeIndex) {
		bool isRequiredMemoryType = memoryTypeBitsRequirements & (1 << memoryTypeIndex);
		if (isRequiredMemoryType && (memProperties.memoryTypes[memoryTypeIndex].propertyFlags & requiredProperties) == requiredProperties) {
			return memoryTypeIndex;
		}
	}

	throw std::runtime_error("VulkanDevice::findMemoryType() - failed to find suitable memory type");
}

/*
* helper function for freeMemory
* 
* @param buffer - buffer handle bound to the memory to be deallocated
*/
bool MemoryAllocator::findAndEraseMemoryBlock(VkBuffer buffer, uint32_t memoryTypeIndex) {
	//find & remove 
	for (auto& memoryChunk : memoryPools[memoryTypeIndex].memoryChunks) {
		auto it = std::find_if(memoryChunk.memoryBlocks.begin(), memoryChunk.memoryBlocks.end(),
			[&buffer](const MemoryBlock& memoryBlock) {
				return buffer == memoryBlock.bufferHandle;
			});

		if (it != memoryChunk.memoryBlocks.end()) {
			memoryChunk.memoryBlocks.erase(it);
			return true;
		}
	}
	return false;
}

/*
* pre-allocate big chunk of memory
*
* @param device - logical device handle needed for vkAllocateMemory
*/
void MemoryAllocator::MemoryPool::allocateChunk(VkDevice device) {
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = defaultChunkSize;
	allocInfo.memoryTypeIndex = memoryTypeIndex;
	MemoryChunk newChunk{ VK_NULL_HANDLE, defaultChunkSize };
	VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &newChunk.memoryHandle));
	memoryChunks.push_back(newChunk);
}

/*
* clean up all pre-allocated chunk of memory
*
* @param device - logical device handle needed for vkAllocateMemory
*/
void MemoryAllocator::MemoryPool::cleanup(VkDevice device) {
	for (auto& memoryChunk : memoryChunks) {
		vkFreeMemory(device, memoryChunk.memoryHandle, nullptr);
	}
}

/*
* return suitable memory location (offset) in current memory chunk
*
* @param memRequirements
* @param bufferImageGranularity
* @param memoryBlock - out parameter containing block layout if return value is true
*
* @return bool - true when found, false when there is none
*/
bool MemoryAllocator::MemoryChunk::findSuitableMemoryLocation(
	const VkMemoryRequirements& memRequirements, VkDeviceSize bufferImageGranularity, MemoryBlock& memoryBlock) {
	//memory chunk is empty
	if (memoryBlocks.empty()) {
		//bufferImageGranularity check
		VkDeviceSize blockEndLocation = memRequirements.size;
		if (bufferImageGranularity > memRequirements.alignment) {
			if (VkDeviceSize remainder = memRequirements.size % bufferImageGranularity; remainder != 0) {
				VkDeviceSize padding = bufferImageGranularity - remainder;
				blockEndLocation += padding;
			}
		}
		memoryBlock = { 0, memRequirements.size, memRequirements.alignment, blockEndLocation };
		return true;
	}

	//iterate all memory blocks
	for (size_t i = 0; i < memoryBlocks.size(); ++i) {
		VkDeviceSize location = memoryBlocks[i].blockEndLocation;
		VkDeviceSize blockSize = memRequirements.size;

		//alignment check
		if (VkDeviceSize remainder = location % memRequirements.alignment; remainder != 0) {
			//some padding is added to take account for alignment
			VkDeviceSize padding = memRequirements.alignment - remainder;
			location += padding;
			blockSize += padding;
		}

		//bufferImageGranularity check
		if (bufferImageGranularity > memRequirements.alignment) {
			if (VkDeviceSize remainder = (location + memRequirements.size) % bufferImageGranularity; remainder != 0) {
				VkDeviceSize padding = bufferImageGranularity - remainder;
				blockSize += padding;
			}
		}

		VkDeviceSize nextBlockEndLocation = (i + 1 == memoryBlocks.size()) ?
			chunkSize : memoryBlocks[i + 1].offset;

		VkDeviceSize spaceInBetween = nextBlockEndLocation - memoryBlocks[i].blockEndLocation;
		if (spaceInBetween > blockSize) {
			memoryBlock = { VK_NULL_HANDLE,
				location,
				memRequirements.size,
				memRequirements.alignment,
				memoryBlocks[i].blockEndLocation + blockSize };
			return true;
		}
	}

	return false;
}

/*
* sort memory block vector in real memory location order
*/
void MemoryAllocator::MemoryChunk::sort() {
	std::sort(memoryBlocks.begin(), memoryBlocks.end(),
		[](const MemoryBlock& l, const MemoryBlock& r) {
			return l.offset < r.offset;
		});
}

/*
* add new memory block to this memory chunk
*
* @param memoryBlock - memory block to add
*/
void MemoryAllocator::MemoryChunk::addMemoryBlock(VkDevice device, VkBuffer buffer,
	MemoryBlock& memoryBlock) {
	memoryBlock.bufferHandle = buffer;
	memoryBlocks.push_back(memoryBlock);
	sort();
	currentSize += (memoryBlock.blockEndLocation - memoryBlock.offset);
	vkBindBufferMemory(device, buffer, memoryHandle, memoryBlock.offset);
}
