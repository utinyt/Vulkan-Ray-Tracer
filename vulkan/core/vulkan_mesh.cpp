#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "vulkan_mesh.h"

/*
* constructor - simply calls load()
* 
* @param path - path to the mesh file
*/
Mesh::Mesh(const std::string& path) {
	load(path);
}

/*
* parse vertex data
*
* @param path - path to the mesh file
*/
void Mesh::load(const std::string& path) {
	vertices.cleanup();
	indices.clear();

	//model load
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string error;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &error, path.c_str())) {
		throw std::runtime_error(error);
	}

	//vertex size
	hasNormalAttribute = !attrib.normals.empty();
	hasTexcoordAttribute = !attrib.texcoords.empty();
	size_t vertexSize = sizeof(glm::vec3);
	if (hasNormalAttribute) {
		vertexSize += sizeof(glm::vec3);
	}
	if (hasTexcoordAttribute) {
		vertexSize += sizeof(glm::vec2);
	}

	//vertex count
	size_t vertexCount = 0;
	for (const auto& shape : shapes) {
		vertexCount += shape.mesh.indices.size();
	}

	vertices.allocate(vertexSize * vertexCount);

	//copy data to the (cpu) buffer
	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			glm::vec3 pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};
			vertices.push(&pos, sizeof(pos));

			if (hasNormalAttribute) {
				glm::vec3 normal = {
				attrib.normals[3 * index.normal_index + 0],
				attrib.normals[3 * index.normal_index + 1],
				attrib.normals[3 * index.normal_index + 2]
				};
				vertices.push(&normal, sizeof(normal));
			}

			if (hasTexcoordAttribute) {
				glm::vec2 texcoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				attrib.texcoords[2 * index.texcoord_index + 1]
				};
				vertices.push(&texcoord, sizeof(texcoord));
			}

			indices.push_back(static_cast<uint32_t>(indices.size()));
		}
	}
}

VkVertexInputBindingDescription Mesh::getBindingDescription() const{
	if (vertices.buffer == nullptr) {
		throw std::runtime_error("Mesh::getBindingDescription(): current mesh is empty");
	}
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;

	uint32_t stride = sizeof(glm::vec3); //position
	if (hasNormalAttribute) {
		stride += sizeof(glm::vec3); //normal
	}
	if (hasTexcoordAttribute) {
		stride += sizeof(glm::vec2); //texcoord
	}

	bindingDescription.stride = stride;
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Mesh::getAttributeDescriptions() const{
	if (vertices.buffer == nullptr) {
		throw std::runtime_error("Mesh::getAttributeDescriptions(): current mesh is empty");
	}

	size_t attributeCount = 1 + hasNormalAttribute + hasTexcoordAttribute;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(attributeCount);

	//position
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = 0;
	uint32_t currentAttributeIndex = 1;
	uint32_t offset = sizeof(glm::vec3);

	//normal
	if (hasNormalAttribute) {
		attributeDescriptions[currentAttributeIndex].binding = 0;
		attributeDescriptions[currentAttributeIndex].location = currentAttributeIndex;
		attributeDescriptions[currentAttributeIndex].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[currentAttributeIndex].offset = offset;
		currentAttributeIndex++;
		offset += sizeof(glm::vec3);
	}

	//texcoord
	if (hasTexcoordAttribute) {
		attributeDescriptions[currentAttributeIndex].binding = 0;
		attributeDescriptions[currentAttributeIndex].location = currentAttributeIndex;
		attributeDescriptions[currentAttributeIndex].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[currentAttributeIndex].offset = offset;
		currentAttributeIndex++;
		offset += sizeof(glm::vec2);
	}
	
	return attributeDescriptions;
}

/*
* buffer memory allocation
* 
* @param bufferSize
*/
void Mesh::Buffer::allocate(size_t bufferSize) {
	this->bufferSize = bufferSize;
	buffer = (char*)malloc(bufferSize);
}

/*
* push data to the buffer
* 
* @param data
* @param dataSize
*/
void Mesh::Buffer::push(void* data, size_t dataSize) {
	if (currentOffset + dataSize > bufferSize) {
		throw std::overflow_error("Mesh::Buffer::push(): buffer overrun");
	}
	memcpy(buffer + currentOffset, data, dataSize);
	currentOffset += dataSize;
}

/*
* deallocate buffer
*/
void Mesh::Buffer::cleanup() {
	free(buffer);
}

/*
* return buffer pointer
* 
* @return void* - pointer to the buffer
*/
void* Mesh::Buffer::data() const {
	return reinterpret_cast<void*>(buffer);
}