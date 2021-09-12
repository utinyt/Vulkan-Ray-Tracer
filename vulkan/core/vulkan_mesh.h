#pragma once
#include <array>
#include "vulkan_utils.h"
#include "glm/glm.hpp"

struct Mesh {
	Mesh(){}
	Mesh(const std::string& path);
	void load(const std::string& path);

	/*
	* input used to build bottom-level acceleration structure
	*/
	struct BlasInput {
		std::vector<VkAccelerationStructureGeometryKHR> asGeometry;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
	};

	/** @brief convert mesh to ray tracing geometry used to build the BLAS */
	BlasInput getVkGeometryKHR(VkDevice device, VkBuffer vertexBuffer, VkBuffer indexBuffer) const;

	std::vector<VkVertexInputBindingDescription> getBindingDescription() const;
	std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const;

	struct Buffer {
		Buffer() {};
		~Buffer() {
			cleanup();
		}
		void allocate(size_t bufferSize);
		void push(const void* data, size_t dataSize);
		void cleanup();
		void* data() const;

		char* buffer = nullptr;
		size_t bufferSize = 0;
		size_t currentOffset = 0;
	} vertices;

	/** vector of index - uint32_t by default */
	std::vector<uint32_t> indices;
	/** vertex size (or stride) */
	size_t vertexSize = 0;
	/** vertex count */
	size_t vertexCount = 0;

private:
	bool hasNormalAttribute = false;
	bool hasTexcoordAttribute = false;
};
