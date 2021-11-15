/*
* reference: https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfscenerendering/gltfscenerendering.h
*/
#pragma once
#include "vulkan_utils.h"
#include "vulkan_texture.h"
#include "tiny_gltf.h"

/*
* load gltf file and parse node / images / textures / materials
*/
struct VulkanDevice;
class VulkanGLTF {
public:
	/** handle to the vulkan devices */
	VulkanDevice* devices = nullptr;
	/** path to the model */
	std::string path;

	/** @breif load gltf scene and assign resources */
	void loadScene(VulkanDevice* devices, const std::string& path, VkBufferUsageFlags usage);
	/** dtor */
	~VulkanGLTF() { cleanup(); }
	/** @brief release all resources */
	void cleanup();

	/*
	* image
	*/
	std::vector<Texture2D> images;
	/** @brief load images from the model */
	void loadImages(tinygltf::Model& input);

	/*
	* texture (reference image via image index)
	*/
	std::vector<uint32_t> textures;
	/** @brief parse image references from the model */
	void loadTextures(tinygltf::Model& input);

	/*
	* material
	*/
	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.f);
		uint32_t baseColorTextureIndex = 0;
		uint32_t normalTextureIndex = 0;
		std::string alphaMode = "OPAQUE";
		float alphaCutoff = 0;
		bool doubleSided = false;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	};
	std::vector<Material> materials;
	/** @brief parse material info from the model */
	void loadMaterials(tinygltf::Model& input);

	/*
	* vertex / index data
	*/
	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
		glm::vec4 tangent;
	};
	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t vertexStart;
		uint32_t vertexCount;
		int32_t materialIndex;
	};

	/** object in gltf scene graph */
	struct Node {
		Node* parent = nullptr;
		std::vector<Node> children;
		std::vector<Primitive> mesh;
		glm::mat4 matrix;
		std::string name;
		bool visible = true;
	};
	std::vector<Node> nodes;

	/** @brief parse mesh data (vertex / index) */
	void loadNode(const tinygltf::Node& inputNode,
		const tinygltf::Model& input,
		Node* parent,
		std::vector<uint32_t>& indexBuffer,
		std::vector<glm::vec3>& vertexBuffer,
		std::vector<glm::vec3>& normalBuffer,
		std::vector<glm::vec2>& uvBuffer,
		std::vector<glm::vec3>& colorBuffer,
		std::vector<glm::vec4>& tangentBuffer
	);

	/*
	* draw
	*/
	/** @brief draw certain node and its children */
	void drawNode(VkCommandBuffer cmdBuf, VkPipelineLayout pipelineLayout, Node& node);
	/** @brief draw all node */
	void draw(VkCommandBuffer cmdBuf, VkPipelineLayout pipelineLayout);

	/*
	* buffers
	*/
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkBuffer normalBuffer = VK_NULL_HANDLE;
	VkBuffer uvBuffer = VK_NULL_HANDLE;
	VkBuffer colorBuffer = VK_NULL_HANDLE;
	VkBuffer tangentBuffer = VK_NULL_HANDLE;

private:
	/** @brief get local matrix from the node */
	glm::mat4 getLocalMatrix(const tinygltf::Node& inputNode) const;
	/** @brief get vertex / index info from the input primitive */
	void addPrimitive(const tinygltf::Primitive& inputPrimitive,
		const tinygltf::Model& input,
		std::vector<uint32_t>& indexBuffer,
		std::vector<glm::vec3>& vertexBuffer,
		std::vector<glm::vec3>& normalBuffer,
		std::vector<glm::vec2>& uvBuffer,
		std::vector<glm::vec3>& colorBuffer,
		std::vector<glm::vec4>& tangentBuffer,
		Node& node
	);
};
