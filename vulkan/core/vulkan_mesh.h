#pragma once
#include <array>
#include "vulkan_utils.h"
#include "glm/glm.hpp"
#include "gltf_scene.h"

struct Mesh {
	Mesh() {}
	Mesh(const std::string& path);
	/** @brief load obj from a file */
	void loadObj(const std::string& path);
	static GltfScene loadGltf(const std::string& path);

	/** @brief return binding description */
	std::vector<VkVertexInputBindingDescription> getBindingDescription() const;
	/** @brief return attribute description */
	std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() const;

	/** storage buffer for vertex & index data */
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
