/*
* reference: https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfscenerendering/gltfscenerendering.cpp
*/
#include "vulkan_gltf.h"
#include "glm/gtc/type_ptr.hpp"

/*
* upload vertex / index data to device memory
*/
void uploadBufferToDeviceMemory(VulkanDevice* devices, VkBuffer& buffer, const void* data,
	VkDeviceSize bufferSize, VkBufferUsageFlags usage) {
	//staging buffers
	VkBuffer stagingBuffer;
	MemoryAllocator::HostVisibleMemory stagingMemory = devices->createBuffer(stagingBuffer, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);
	stagingMemory.mapData(devices->device, data);

	//create buffer
	devices->createBuffer(buffer, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	//copy buffer
	VkCommandBuffer cmdBuf = devices->beginCommandBuffer();
	VkBufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;
	vkCmdCopyBuffer(cmdBuf, stagingBuffer, buffer, 1, &bufferCopyRegion);
	devices->endCommandBuffer(cmdBuf);

	devices->memoryAllocator.freeBufferMemory(stagingBuffer,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkDestroyBuffer(devices->device, stagingBuffer, nullptr);
}

/*
* load gltf scene and assign resources 
*/
void VulkanGLTF::loadScene(VulkanDevice* devices, const std::string& path, VkBufferUsageFlags usage) {
	//init
	this->devices = devices;

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err, warn;

	//load gltf file
	bool result = loader.LoadASCIIFromFile(&model, &err, &warn, path);
	std::string str = "Mesh::loadGltf(): ";
	if (!warn.empty()) {
		throw std::runtime_error(str + warn);
	}
	if (!err.empty()) {
		throw std::runtime_error(str + err);
	}
	if (!result) {
		throw std::runtime_error("failed to parse glTF\n");
	}

	//extract path to the model
	this->path = path.substr(0, path.find_last_of('/')) + '/';

	//parse data & fill buffer data
	std::vector<uint32_t> indexBufferData{};
	std::vector<glm::vec3> vertexBufferData{};
	std::vector<glm::vec3> normalBufferData{};
	std::vector<glm::vec2> uvBufferData{};
	std::vector<glm::vec3> colorBufferData{};
	std::vector<glm::vec4> tangentBufferData{};

	if (result) {
		loadImages(model);
		loadMaterials(model);
		loadTextures(model);
		const tinygltf::Scene& scene = model.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); ++i) {
			const tinygltf::Node node = model.nodes[scene.nodes[i]];
			loadNode(node, model, nullptr,
				indexBufferData, vertexBufferData, normalBufferData, uvBufferData, colorBufferData, tangentBufferData);
		}
	}
	else {
		throw std::runtime_error("VulkanGLTF::loadScene(): cannot open the file: " + path);
	}

	size_t indexBufferSize		= indexBufferData.size() * sizeof(uint32_t);
	size_t vertexBufferSize		= vertexBufferData.size() * sizeof(glm::vec3);
	size_t normalBufferSize		= normalBufferData.size() * sizeof(glm::vec3);
	size_t uvBufferSize			= uvBufferData.size() * sizeof(glm::vec2);
	size_t colorBufferSize		= colorBufferData.size() * sizeof(glm::vec3);
	size_t tangentBufferSize	= tangentBufferData.size() * sizeof(glm::vec4);
	uploadBufferToDeviceMemory(devices, indexBuffer, indexBufferData.data(), indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | usage);
	uploadBufferToDeviceMemory(devices, vertexBuffer, vertexBufferData.data(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | usage);
	uploadBufferToDeviceMemory(devices, normalBuffer, normalBufferData.data(), normalBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | usage);
	uploadBufferToDeviceMemory(devices, uvBuffer, uvBufferData.data(), uvBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | usage);
	uploadBufferToDeviceMemory(devices, colorBuffer, colorBufferData.data(), colorBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | usage);
	uploadBufferToDeviceMemory(devices, tangentBuffer, tangentBufferData.data(), tangentBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | usage);
}

/*
* clean up
*/
void VulkanGLTF::cleanup() {
	if (devices == nullptr) {
		return;
	}

	//images
	for (Texture2D& image : images) {
		image.cleanup();
	}

	//pipelines
	for (Material& material : materials) {
		vkDestroyPipeline(devices->device, material.pipeline, nullptr);
	}

	//buffers
	devices->memoryAllocator.freeBufferMemory(vertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, vertexBuffer, nullptr);
	devices->memoryAllocator.freeBufferMemory(indexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, indexBuffer, nullptr);
	devices->memoryAllocator.freeBufferMemory(normalBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, normalBuffer, nullptr);
	devices->memoryAllocator.freeBufferMemory(uvBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, uvBuffer, nullptr);
	devices->memoryAllocator.freeBufferMemory(colorBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, colorBuffer, nullptr);
	devices->memoryAllocator.freeBufferMemory(tangentBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, tangentBuffer, nullptr);
}

/*
* load images from the model 
* 
* @param input - loaded gltf model
*/
void VulkanGLTF::loadImages(tinygltf::Model& input) {
	images.resize(input.images.size());
	for (int i = 0; i < input.images.size(); ++i) {
		tinygltf::Image& srcImage = input.images[i];
		images[i].load(devices, path + srcImage.uri);
	}
}

/*
* parse image references from the model 
* 
* @param input - loaded gltf model
*/
void VulkanGLTF::loadTextures(tinygltf::Model& input) {
	textures.resize(input.textures.size());
	for (int i = 0; i < input.textures.size(); ++i) {
		textures[i] = input.textures[i].source;
	}
}

/*
* parse material info from the model
* 
* @param input - loaded gltf model
*/
void VulkanGLTF::loadMaterials(tinygltf::Model& input) {
	materials.resize(input.materials.size());
	for (size_t i = 0; i < materials.size(); ++i) {
		tinygltf::Material srcMaterial = input.materials[i];

		//base color
		if (srcMaterial.values.find("baseColorFactor") != srcMaterial.values.end()) {
			materials[i].baseColorFactor = glm::make_vec4(srcMaterial.values["baseColorFactor"].ColorFactor().data());
		}
		//base color texture index
		if (srcMaterial.values.find("baseColorTexture") != srcMaterial.values.end()) {
			materials[i].baseColorTextureIndex = srcMaterial.values["baseColorTexture"].TextureIndex();
		}
		//base color texture index
		if (srcMaterial.additionalValues.find("normalTexture") != srcMaterial.additionalValues.end()) {
			materials[i].normalTextureIndex = srcMaterial.additionalValues["normalTexture"].TextureIndex();
		}
		//additional parameters
		materials[i].alphaMode = srcMaterial.alphaMode;
		materials[i].alphaCutoff = srcMaterial.alphaCutoff;
		materials[i].doubleSided = srcMaterial.doubleSided;
	}
}

/* 
* parse mesh data (vertex / index) 
* 
* @param inputNode
* @param input - the whole model
* @param parent
* @param indexBuffer - output index buffer
* @param vertexBuffer - output vertex buffer
*/
void VulkanGLTF::loadNode(const tinygltf::Node& inputNode,
	const tinygltf::Model& input,
	Node* parent,
	std::vector<uint32_t>& indexBuffer,
	std::vector<glm::vec3>& vertexBuffer,
	std::vector<glm::vec3>& normalBuffer,
	std::vector<glm::vec2>& uvBuffer,
	std::vector<glm::vec3>& colorBuffer,
	std::vector<glm::vec4>& tangentBuffer) {
	Node node{};
	node.name = inputNode.name;
	node.matrix = getLocalMatrix(inputNode);

	//recursion - load children
	if (inputNode.children.size() > 0) {
		for (size_t i = 0; i < inputNode.children.size(); ++i) {
			loadNode(input.nodes[inputNode.children[i]], input, &node,
				indexBuffer, vertexBuffer, normalBuffer, uvBuffer, colorBuffer, tangentBuffer);
		}
	}

	//if the node contains mesh data, we load vertices and indices from the buffers
	if (inputNode.mesh > -1) {
		const tinygltf::Mesh& srcMesh = input.meshes[inputNode.mesh];

		//iterate all primitive in current node's mesh
		for (size_t i = 0; i < srcMesh.primitives.size(); ++i) {
			addPrimitive(srcMesh.primitives[i], input,
				indexBuffer, vertexBuffer, normalBuffer, uvBuffer, colorBuffer, tangentBuffer, node);
		}
	}

	if (parent) {
		parent->children.push_back(node);
	}
	else {
		nodes.push_back(node);
	}
}

/*
* draw certain node and its children 
*/
void VulkanGLTF::drawNode(VkCommandBuffer cmdBuf, VkPipelineLayout pipelineLayout, Node& node) {
	if (node.visible == false) {
		return;
	}
	if (node.mesh.size() > 0) {
		//traverse node hierarchy to the top-most parent to get the final matrix
		glm::mat4 nodeMatrix = node.matrix;
		Node* currentParent = node.parent;
		while (currentParent) {
			nodeMatrix = currentParent->matrix * nodeMatrix;
			currentParent = currentParent->parent;
		}

		//pass the final matrix to the vertex shader via push constants
		vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
		for (Primitive& primitive : node.mesh) {
			if (primitive.indexCount > 0) {
				Material& material = materials[primitive.materialIndex];
				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &material.descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(cmdBuf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	}

	//draw all child
	for (auto& child : node.children) {
		drawNode(cmdBuf, pipelineLayout, child);
	}
}

/*
* draw all node 
*/
void VulkanGLTF::draw(VkCommandBuffer cmdBuf, VkPipelineLayout pipelineLayout) {

}

/*
* get local matrix from the node 
* 
* @param inputNode
* 
* @return localMatrix
*/
glm::mat4 VulkanGLTF::getLocalMatrix(const tinygltf::Node& inputNode) const {
	if (inputNode.matrix.size() == 16) {
		return glm::make_mat4(inputNode.matrix.data());
	}

	glm::mat4 localMatrix = glm::mat4(1.f);
	if (inputNode.translation.size() == 3) {
		localMatrix = glm::translate(localMatrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
	}
	if (inputNode.rotation.size() == 4) {
		glm::quat q = glm::make_quat(inputNode.rotation.data());
		localMatrix *= glm::mat4(q);
	}
	if (inputNode.scale.size() == 3) {
		localMatrix = glm::scale(localMatrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
	}
	return localMatrix;
}

/*
* get vertex / index info from the input primitive 
* 
* @param inputPrimitive - input primitive to parse data
* @param input - the whole model
* @param indexBuffer - buffer to store indices
* @param vertexBuffer - buffer to store vertices
*/
void VulkanGLTF::addPrimitive(const tinygltf::Primitive& inputPrimitive,
	const tinygltf::Model& input,
	std::vector<uint32_t>& indexBuffer,
	std::vector<glm::vec3>& vertexBuffer,
	std::vector<glm::vec3>& normalBuffer,
	std::vector<glm::vec2>& uvBuffer,
	std::vector<glm::vec3>& colorBuffer,
	std::vector<glm::vec4>& tangentBuffer,
	Node& node) {
	uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
	uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());

	/*
	* vertices
	*/
	const float* positionBuffer = nullptr;
	const float* normalsBuffer = nullptr;
	const float* texCoordsBuffer = nullptr;
	const float* tangentsBuffer = nullptr;
	size_t vertexCount = 0;

	//get positions
	if (auto posIt = inputPrimitive.attributes.find("POSITION"); posIt != inputPrimitive.attributes.end()) {
		const tinygltf::Accessor& accessor = input.accessors[posIt->second];
		const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
		positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
		vertexCount = accessor.count;
	}
	//get vertex normals
	if (auto normalIt = inputPrimitive.attributes.find("NORMAL"); normalIt != inputPrimitive.attributes.end()) {
		const tinygltf::Accessor& accessor = input.accessors[normalIt->second];
		const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
		normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
	}
	//get vertex texture coordinates
	if (auto uvIt = inputPrimitive.attributes.find("TEXCOORD_0"); uvIt != inputPrimitive.attributes.end()) {
		const tinygltf::Accessor& accessor = input.accessors[uvIt->second];
		const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
		texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
	}
	//get tengents
	if (auto tangentIt = inputPrimitive.attributes.find("TANGENT"); tangentIt != inputPrimitive.attributes.end()) {
		const tinygltf::Accessor& accessor = input.accessors[tangentIt->second];
		const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
		tangentsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
	}

	//append data to model's vertex buffer
	for (size_t i = 0; i < vertexCount; ++i) {
		vertexBuffer.push_back(glm::make_vec3(&positionBuffer[i * 3]));
		normalBuffer.push_back(normalsBuffer ? glm::normalize(glm::make_vec3(&normalsBuffer[i * 3])) : glm::vec3(0.f));
		uvBuffer.push_back(texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[i * 2]) : glm::vec2(0.f));
		colorBuffer.push_back(glm::vec3(1.f));
		tangentBuffer.push_back(tangentsBuffer ? glm::make_vec4(&tangentsBuffer[i * 4]) : glm::vec4(0.f));
	}
	
	/*
	* indices
	*/
	const tinygltf::Accessor& accessor = input.accessors[inputPrimitive.indices];
	const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
	const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

	uint32_t indexCount = static_cast<uint32_t>(accessor.count);

	switch (accessor.componentType) {
	case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
		const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
		for (size_t i = 0; i < accessor.count; ++i) {
			indexBuffer.push_back(buf[i] + vertexStart);
		}
		break;
	}
	case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
		const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
		for (size_t i = 0; i < accessor.count; ++i) {
			indexBuffer.push_back(buf[i] + vertexStart);
		}
		break;
	}
	case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
		const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
		for (size_t i = 0; i < accessor.count; ++i) {
			indexBuffer.push_back(buf[i] + vertexStart);
		}
		break;
	}
	default:
		throw std::runtime_error("VulkanGLTF::addPrimitive(): index component type" + 
			std::to_string(accessor.componentType) + " not supported");
	}

	Primitive primitive{};
	primitive.firstIndex = firstIndex;
	primitive.indexCount = indexCount;
	primitive.vertexStart = vertexStart;
	primitive.vertexCount = vertexCount;
	primitive.materialIndex = inputPrimitive.material;
	node.mesh.push_back(primitive);
}
