#pragma once
#include <array>
#include "vulkan_utils.h"
#include "glm/glm.hpp"

struct Mesh {
	Mesh(){}
	Mesh(const std::string& path);
	void load(const std::string& path);

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;

		static VkVertexInputBindingDescription getBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
	};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};
