#include "core/vulkan_app_base.h"
#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "core/vulkan_mesh.h"

class VulkanApp : public VulkanAppBase {
public:
	/*
	* constructor - get window size & title
	*/
	VulkanApp(int width, int height, const std::string& appName)
		: VulkanAppBase(width, height, appName) {
		enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	}

	/*
	* destructor - destroy vulkan objects created in this level
	*/
	~VulkanApp() {

	}

	/*
	* application initialization - also contain base class initApp()
	*/
	virtual void initApp() override {
		//request ray tracing properties
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{ 
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
		VkPhysicalDeviceProperties2 properties2{};
		properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties2.pNext = &rtProperties;
		vkGetPhysicalDeviceProperties2(devices.physicalDevice, &properties2);

		//load bunny mesh
		mesh.load("../../meshes/bunny.obj");

		//create vertex & index buffers
		createBuffer(mesh.vertices.data(), mesh.vertices.bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer);
		createBuffer(mesh.indices.data(), sizeof(mesh.indices[0]) * mesh.indices.size(),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer);

		Mesh::BlasInput blasInput = mesh.getVkGeometryKHR(devices.device, vertexBuffer, indexBuffer);
	}

	/*
	* draw
	*/
	virtual void draw() override {

	}

	/*
	* create framebuffers
	*/
	virtual void createFramebuffers() override {

	}

	/*
	* record command buffer
	*/
	virtual void recordCommandBuffer() override {

	}

private:
	/** vertex & index buffer */
	VkBuffer vertexBuffer, indexBuffer;
	/** bunny mesh */
	Mesh mesh;

	/** @brief build buffer - used to create vertex / index buffer */
	/*
	* build buffer - used to create vertex / index buffer
	*
	* @param bufferData - data to transfer
	* @param bufferSize
	* @param usage - buffer usage (vertex / index bit)
	* @param buffer - buffer handle
	* @param bufferMemory - buffer memory handle
	*/
	void createBuffer(const void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags usage,
		VkBuffer& buffer) {
		//create staging buffer
		VkBuffer stagingBuffer;
		VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
			bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

		//suballocate
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices.memoryAllocator.allocateBufferMemory(
			stagingBuffer, properties);

		hostVisibleMemory.MapData(devices.device, bufferData);

		//create vertex & index buffer
		VkBufferCreateInfo bufferCreateInfo = vktools::initializers::bufferCreateInfo(
			bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &bufferCreateInfo, nullptr, &buffer));

		//suballocation
		devices.memoryAllocator.allocateBufferMemory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		//host visible -> device local
		devices.copyBuffer(devices.commandPool, stagingBuffer, buffer, bufferSize);

		devices.memoryAllocator.freeBufferMemory(stagingBuffer, properties);
		vkDestroyBuffer(devices.device, stagingBuffer, nullptr);
	}
};

//entry point
RUN_APPLICATION_MAIN(VulkanApp, 800, 600, "ray tracing");
