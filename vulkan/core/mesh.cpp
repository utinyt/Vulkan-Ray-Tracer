#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "mesh.h"

/*
* constructor - parse vertex & index data
* 
* @param path - path to the mesh file
*/
Mesh::Mesh(const std::string& path) {
	load(path);
}

void Mesh::load(const std::string& path) {
	vertices.clear();
	indices.clear();

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string error;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &error, path.c_str())) {
		throw std::runtime_error(error);
	}

	for (const auto& shape : shapes) {
		for (int i = 0; i < shape.mesh.indices.size(); i += 3) {
			Vertex v1{}, v2{}, v3{};

			v1.pos = {
				attrib.vertices[3 * shape.mesh.indices[i].vertex_index + 0],
				attrib.vertices[3 * shape.mesh.indices[i].vertex_index + 1],
				attrib.vertices[3 * shape.mesh.indices[i].vertex_index + 2]
			};

			v2.pos = {
				attrib.vertices[3 * shape.mesh.indices[i+1].vertex_index + 0],
				attrib.vertices[3 * shape.mesh.indices[i+1].vertex_index + 1],
				attrib.vertices[3 * shape.mesh.indices[i+1].vertex_index + 2]
			};

			v3.pos = {
				attrib.vertices[3 * shape.mesh.indices[i+2].vertex_index + 0],
				attrib.vertices[3 * shape.mesh.indices[i+2].vertex_index + 1],
				attrib.vertices[3 * shape.mesh.indices[i+2].vertex_index + 2]
			};
			
			glm::vec3 c0 = v2.pos - v1.pos;
			glm::vec3 c1 = v3.pos - v1.pos;
			glm::vec3 faceNormal = glm::cross(c0, c1);
			faceNormal = glm::normalize(faceNormal);

			v1.normal = faceNormal;
			v2.normal = faceNormal;
			v3.normal = faceNormal;

			vertices.push_back(v1);
			vertices.push_back(v2);
			vertices.push_back(v3);
			indices.push_back(i);
			indices.push_back(i+1);
			indices.push_back(i+2);
		}
	}
}

VkVertexInputBindingDescription Mesh::Vertex::getBindingDescription() {
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Mesh::Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> Mesh::Vertex::getAttributeDescriptions() {
	std::array<VkVertexInputAttributeDescription, 2> attributeDescription;
	attributeDescription[0].binding = 0;
	attributeDescription[0].location = 0;
	attributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription[0].offset = offsetof(Mesh::Vertex, pos);

	attributeDescription[1].binding = 0;
	attributeDescription[1].location = 1;
	attributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription[1].offset = offsetof(Mesh::Vertex, normal);

	return attributeDescription;
}