#include "core/vulkan_app_base.h"
#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "core/vulkan_mesh.h"
#include "core/vulkan_descriptor_set_bindings.h"
#include "core/vulkan_texture.h"

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
		//BLAS
		for (auto& as : blas) {
			vkfp::vkDestroyAccelerationStructureKHR(devices.device, as.accel, nullptr);
			devices.memoryAllocator.freeBufferMemory(as.buffer);
			vkDestroyBuffer(devices.device, as.buffer, nullptr);
		}

		//instance buffer
		devices.memoryAllocator.freeBufferMemory(instanceBuffer);
		vkDestroyBuffer(devices.device, instanceBuffer, nullptr);

		//TLAS
		vkfp::vkDestroyAccelerationStructureKHR(devices.device, tlas.accel, nullptr);
		devices.memoryAllocator.freeBufferMemory(tlas.buffer);
		vkDestroyBuffer(devices.device, tlas.buffer, nullptr);

		//vertex & index buffers
		devices.memoryAllocator.freeBufferMemory(vertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, vertexBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(indexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, indexBuffer, nullptr);

		//offscreen framebuffer
		for (auto& framebuffer : offscreenFramebuffers) {
			vkDestroyFramebuffer(devices.device, framebuffer, nullptr);
		}

		//offscreen renderpass
		vkDestroyRenderPass(devices.device, offscreenRenderPass, nullptr);

		//offscreen images
		for (auto& offscreen : offscreens) {
			offscreen.offscreenColorBuffer.cleanup();
			offscreen.offscreenDepthBuffer.cleanup();
		}

		//descriptors
		vkDestroyDescriptorPool(devices.device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, rtDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, rtDescriptorSetLayout, nullptr);
	}

	/*
	* application initialization - also contain base class initApp()
	*/
	virtual void initApp() override {
		VulkanAppBase::initApp();
		////request ray tracing properties
		//VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{ 
		//	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
		//VkPhysicalDeviceProperties2 properties2{};
		//properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		//properties2.pNext = &rtProperties;
		//vkGetPhysicalDeviceProperties2(devices.physicalDevice, &properties2);

		//load bunny mesh
		mesh.load("../../meshes/bunny.obj");

		//create vertex & index buffers
		VkBufferUsageFlags rtFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | 
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | 
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		createBuffer(mesh.vertices.data(), mesh.vertices.bufferSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rtFlags,
			vertexBuffer);
		createBuffer(mesh.indices.data(), sizeof(mesh.indices[0]) * mesh.indices.size(),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rtFlags,
			indexBuffer);

		//BLAS
		std::vector<Mesh::BlasInput> allBlas;
		allBlas.push_back(mesh.getVkGeometryKHR(devices.device, vertexBuffer, indexBuffer));
		buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

		size_t meshSize = 1;

		//TLAS
		std::vector<VkAccelerationStructureInstanceKHR> tlas;
		tlas.reserve(meshSize);
		for (uint32_t i = 0; i < static_cast<uint32_t>(meshSize); ++i) {
			VkAccelerationStructureInstanceKHR rayInst;
			rayInst.transform = vktools::toTransformMatrixKHR(glm::mat4(1.f));
			rayInst.instanceCustomIndex = i;
			rayInst.accelerationStructureReference = getBlasDeviceAddress(i);
			rayInst.instanceShaderBindingTableRecordOffset = 0; // we will use the same hit group for all object
			rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			rayInst.mask = 0xFF;
			tlas.emplace_back(rayInst);
		}
		buildTlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

		createOffscreenRender();
		createRtDescriptorSet();
		createDescriptorSet();

		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updateRtDescriptorSet(i);
		}
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

	/*
	* resize
	*/
	virtual void resizeWindow() override {
		VulkanAppBase::resizeWindow();
		createOffscreenRender();
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updateRtDescriptorSet(i);
		}
	}

private:
	struct AccelKHR {
		VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
		VkBuffer buffer;
	};

	struct BuildAccelerationStructure {
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		const VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfo = nullptr;
		VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		AccelKHR as{};
	};

	/** vertex & index buffer */
	VkBuffer vertexBuffer = VK_NULL_HANDLE, indexBuffer = VK_NULL_HANDLE;
	/** bunny mesh */
	Mesh mesh;
	/** bottom-level acceleration structures */
	std::vector<AccelKHR> blas;
	/** top-level acceleration structure */
	AccelKHR tlas{};
	/** instance buffer for tlas */
	VkBuffer instanceBuffer = VK_NULL_HANDLE;

	/** descriptor set layout bindings */
	DescriptorSetBindings descriptorSetBindings;
	/** descriptor set layout */
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	/** decriptor pool */
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	/** descriptor sets */
	std::vector<VkDescriptorSet> descriptorSets;

	/** descriptor set layout bindings */
	DescriptorSetBindings rtDescriptorSetBindings;
	/** descriptor set layout */
	VkDescriptorSetLayout rtDescriptorSetLayout = VK_NULL_HANDLE;
	/** decriptor pool */
	VkDescriptorPool rtDescriptorPool = VK_NULL_HANDLE;
	/** descriptor sets */
	std::vector<VkDescriptorSet> rtDescriptorSets;

	struct OffscreenImages {
		/** offscreen depth image */
		Texture2D offscreenDepthBuffer;
		/** offscreen color image */
		Texture2D offscreenColorBuffer;
	};
	
	std::vector<OffscreenImages> offscreens;
	/** offscreen depth format */
	VkFormat offscreenDepthFormat{ VK_FORMAT_X8_D24_UNORM_PACK32 };
	/** offscreen color format */
	VkFormat offscreenColorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };
	/** offscreen renderpass */
	VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;
	/** offscreen framebuffers */
	std::vector<VkFramebuffer> offscreenFramebuffers;

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

	AccelKHR createAcceleration(VkAccelerationStructureCreateInfoKHR& info) {
		AccelKHR resultAs;
		resultAs.buffer = devices.createBuffer(info.size,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

		info.buffer = resultAs.buffer;
		vkfp::vkCreateAccelerationStructureKHR(devices.device, &info, nullptr, &resultAs.accel);
		return resultAs;
	}

	void buildBlas(const std::vector<Mesh::BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags) {
		uint32_t nbBlas = static_cast<uint32_t>(input.size());
		VkDeviceSize asTotalSize{ 0 };
		uint32_t nbCompactions{ 0 };
		VkDeviceSize maxScratchSize{ 0 };

		std::vector<BuildAccelerationStructure> buildAs(input.size());
		
		for (uint32_t blasIndex = 0; blasIndex < nbBlas; ++blasIndex) {
			//fill VkAccelerationStructureBuildGeometryInfoKHR partially for querying the build sizes
			buildAs[blasIndex].buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildAs[blasIndex].buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildAs[blasIndex].buildInfo.flags = flags;
			buildAs[blasIndex].buildInfo.geometryCount = static_cast<uint32_t>(input[blasIndex].asGeometry.size());
			buildAs[blasIndex].buildInfo.pGeometries = input[blasIndex].asGeometry.data();

			//build range info
			buildAs[blasIndex].buildRangeInfo = input[blasIndex].asBuildOffsetInfo.data();

			//finding sizes to create acceleration structures and scratch
			std::vector<uint32_t> maxPrimCount(input[blasIndex].asBuildOffsetInfo.size());
			for (size_t i = 0; i < maxPrimCount.size(); ++i) {
				maxPrimCount[i] = input[blasIndex].asBuildOffsetInfo[i].primitiveCount;
			}
			vkfp::vkGetAccelerationStructureBuildSizesKHR(devices.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&buildAs[blasIndex].buildInfo, maxPrimCount.data(), &buildAs[blasIndex].buildSizesInfo);

			asTotalSize += buildAs[blasIndex].buildSizesInfo.accelerationStructureSize;
			maxScratchSize = std::max(maxScratchSize, buildAs[blasIndex].buildSizesInfo.buildScratchSize);
			nbCompactions += buildAs[blasIndex].buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
		}
		
		//create scratch buffer
		VkBuffer scratchBuffer;
		VkBufferCreateInfo scratchBufferInfo = vktools::initializers::bufferCreateInfo(maxScratchSize,
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &scratchBufferInfo, nullptr, &scratchBuffer));
		devices.memoryAllocator.allocateBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);//memProperty ????

		VkDeviceAddress scratchAddress = vktools::getBufferDeviceAddress(devices.device, scratchBuffer);

		//querying the real size of BLAS
		VkQueryPool queryPool{ VK_NULL_HANDLE };
		if (nbCompactions > 0) {
			assert(nbCompactions == nbBlas); //don't allow mix of on/off compaction
			VkQueryPoolCreateInfo queryPoolCreateInfo{};
			queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			queryPoolCreateInfo.queryCount = nbBlas;
			queryPoolCreateInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
			vkCreateQueryPool(devices.device, &queryPoolCreateInfo, nullptr, &queryPool);
		}

		//split BLAS creation into chunks of ~256MB to avoid pipeline stalling
		std::vector<uint32_t> indices;
		VkDeviceSize batchSize{ 0 };
		VkDeviceSize batchLimit{ 256'000'000 };
		
		for (uint32_t blasIndex = 0; blasIndex < nbBlas; ++blasIndex) {
			indices.push_back(blasIndex);
			batchSize += buildAs[blasIndex].buildSizesInfo.accelerationStructureSize;

			//over the limit or last blas element
			if (batchSize >= batchLimit || blasIndex == nbBlas - 1) {
				VkCommandBuffer cmdBuf = devices.beginOneTimeSubmitCommandBuffer();
				cmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
				devices.endOneTimeSubmitCommandBuffer(cmdBuf);

				if (queryPool) {
					VkCommandBuffer cmdBuf = devices.beginOneTimeSubmitCommandBuffer();
					cmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
					devices.endOneTimeSubmitCommandBuffer(cmdBuf);
				}

				//reset
				batchSize = 0;
				indices.clear();
			}
		}

		//keep all the created acceleration structures
		for (auto& s : buildAs) {
			blas.emplace_back(s.as);
		}

		//cleanup
		vkDestroyQueryPool(devices.device, queryPool, nullptr);
		devices.memoryAllocator.freeBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, scratchBuffer, nullptr);
	}

	void cmdCreateBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
		std::vector<BuildAccelerationStructure>& buildAs, VkDeviceAddress scratchAddress, VkQueryPool queryPool) {
		if (queryPool) {
			vkResetQueryPool(devices.device, queryPool, 0, static_cast<uint32_t>(indices.size()));
		}
		uint32_t queryCount = 0;

		for (uint32_t blasIndex : indices) {
			//actual allocation of buffer and acceleration structure
			VkAccelerationStructureCreateInfoKHR createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			createInfo.size = buildAs[blasIndex].buildSizesInfo.accelerationStructureSize;
			buildAs[blasIndex].as = createAcceleration(createInfo);

			buildAs[blasIndex].buildInfo.dstAccelerationStructure = buildAs[blasIndex].as.accel;
			buildAs[blasIndex].buildInfo.scratchData.deviceAddress = scratchAddress;

			//build the blas
			vkfp::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[blasIndex].buildInfo, &buildAs[blasIndex].buildRangeInfo);

			//since scratch buffer is resued across builds, we need a barrier to ensure one build
			VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vkCmdPipelineBarrier(cmdBuf,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				0,
				1, &barrier,
				0, nullptr,
				0, nullptr);

			if (queryPool) {
				//add a query to find the real amount of memory needed, used for compaction
				vkfp::vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &buildAs[blasIndex].buildInfo.dstAccelerationStructure,
					VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCount++);
			}
		}
	}

	/*
	* create and replace a new acceleration structure and buffer based on the size retrieved by the query
	*/
	void cmdCompactBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
		std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool) {
		uint32_t queryCount = 0;
		std::vector<AccelKHR> cleanupAs;

		//get the compacted size result back
		std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
		vkGetQueryPoolResults(devices.device, queryPool, 0, (uint32_t)compactSizes.size(),
			compactSizes.size() * sizeof(VkDeviceSize), compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);

		for (uint32_t blasIndex : indices) {
			cleanupAs.push_back(buildAs[blasIndex].as);
			buildAs[blasIndex].buildSizesInfo.accelerationStructureSize = compactSizes[queryCount++]; //new reduced size

			//create a compact version of the AS
			VkAccelerationStructureCreateInfoKHR asCreateInfo{};
			asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
			asCreateInfo.size = buildAs[blasIndex].buildSizesInfo.accelerationStructureSize;
			asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildAs[blasIndex].as = createAcceleration(asCreateInfo);

			//copy the original blas to a compact version
			VkCopyAccelerationStructureInfoKHR copyInfo{};
			copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
			copyInfo.src = buildAs[blasIndex].buildInfo.dstAccelerationStructure;
			copyInfo.dst = buildAs[blasIndex].as.accel;
			copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
			vkfp::vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
		}

		//destroy old acceleration structures
		for (auto& as : cleanupAs) {
			vkfp::vkDestroyAccelerationStructureKHR(devices.device, as.accel, nullptr);
			devices.memoryAllocator.freeBufferMemory(as.buffer);
		}
	}

	/*
	* return the device address of a Blas previously created
	*/
	VkDeviceAddress getBlasDeviceAddress(uint32_t blasIndex) {
		assert((size_t)blasIndex < blas.size());
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = blas[blasIndex].accel;
		return vkfp::vkGetAccelerationStructureDeviceAddressKHR(devices.device, &addressInfo);
	}

	void buildTlas(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
		VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		bool update = false) {
		//cannot call buildTlas twice except to update
		assert(tlas.accel == VK_NULL_HANDLE || update);

		//create a buffer holding the actual instance data for use by the AS builder
		VkDeviceSize instanceDescsSizeInBytes = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

		//allocate the instance buffer and copy its contents from host to device memory
		if (update) {
			devices.memoryAllocator.freeBufferMemory(instanceBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			vkDestroyBuffer(devices.device, instanceBuffer, nullptr);
		}

		//create staging buffer
		VkBuffer stagingBuffer;
		VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
			instanceDescsSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

		//suballocate
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices.memoryAllocator.allocateBufferMemory(
			stagingBuffer, properties);

		hostVisibleMemory.MapData(devices.device, instances.data());

		//create vertex & index buffer
		VkBufferCreateInfo bufferCreateInfo = vktools::initializers::bufferCreateInfo(
			instanceDescsSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &bufferCreateInfo, nullptr, &instanceBuffer));

		//suballocation
		devices.memoryAllocator.allocateBufferMemory(instanceBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkCommandBuffer cmdBuf= devices.beginOneTimeSubmitCommandBuffer();

		//copy buffer
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = instanceDescsSizeInBytes;
		vkCmdCopyBuffer(cmdBuf, stagingBuffer, instanceBuffer, 1, &copyRegion);

		//make sure the copy of the instance buffer are copied before triggering the acceleration structure build
		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);

		//create VkAccelerationStructureGeometryInstanceDataKHR
		//This wraps a device pointer to the above uploaded instances
		VkAccelerationStructureGeometryInstancesDataKHR instancesVk{};
		instancesVk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		instancesVk.arrayOfPointers = VK_FALSE;
		instancesVk.data.deviceAddress = vktools::getBufferDeviceAddress(devices.device, instanceBuffer);

		//put the above into a VkAccelerationStructureGeometryKHR
		//we need to put the instances struct in a union and label it as instance data
		VkAccelerationStructureGeometryKHR topAsGeometry{};
		topAsGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		topAsGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topAsGeometry.geometry.instances = instancesVk;

		//query the needed memory for the tlas and scratch space
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfo.flags = flags;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &topAsGeometry;
		buildInfo.mode = update
			? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
			: VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		uint32_t count = (uint32_t)instances.size();
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		vkfp::vkGetAccelerationStructureBuildSizesKHR(devices.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&buildInfo, &count, &sizeInfo);

		//create tlas
		if (update == false) {
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			createInfo.size = sizeInfo.accelerationStructureSize;
			tlas = createAcceleration(createInfo);
		}
		//allocate the scratch memory
		VkBuffer scratchBuffer = VK_NULL_HANDLE;
		VkBufferCreateInfo scratchBufferInfo = vktools::initializers::bufferCreateInfo(sizeInfo.buildScratchSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | 
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &scratchBufferInfo, nullptr, &scratchBuffer));
		devices.memoryAllocator.allocateBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		//update build info
		buildInfo.srcAccelerationStructure = update ? tlas.accel : VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = tlas.accel;
		buildInfo.scratchData.deviceAddress = vktools::getBufferDeviceAddress(devices.device, scratchBuffer);

		//build offset info : n instances
		VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{ static_cast<uint32_t>(instances.size()), 0, 0, 0 };
		const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

		//build the tlas
		vkfp::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);

		devices.endOneTimeSubmitCommandBuffer(cmdBuf);
		devices.memoryAllocator.freeBufferMemory(stagingBuffer, properties);
		vkDestroyBuffer(devices.device, stagingBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, scratchBuffer, nullptr);
	}

	void createRtDescriptorSet() {
		rtDescriptorSetBindings.addBinding(0,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR); //tlas
		rtDescriptorSetBindings.addBinding(1,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR); //output image

		//create rt descriptor pool & layout
		uint32_t nbRtDescriptorSet = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		rtDescriptorPool = rtDescriptorSetBindings.createDescriptorPool(devices.device, nbRtDescriptorSet);
		rtDescriptorSetLayout = rtDescriptorSetBindings.createDescriptorSetLayout(devices.device);

		//allocate rt descriptor sets
		std::vector<VkDescriptorSetLayout> layout(nbRtDescriptorSet, rtDescriptorSetLayout);
		rtDescriptorSets.assign(nbRtDescriptorSet, {});
		VkDescriptorSetAllocateInfo descInfo{};
		descInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descInfo.descriptorPool = rtDescriptorPool;
		descInfo.descriptorSetCount = nbRtDescriptorSet;
		descInfo.pSetLayouts = layout.data();
		VK_CHECK_RESULT(vkAllocateDescriptorSets(devices.device, &descInfo, rtDescriptorSets.data()));

		for (uint32_t i = 0; i < nbRtDescriptorSet; ++i) {
			VkWriteDescriptorSetAccelerationStructureKHR descAsInfo{};
			descAsInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			descAsInfo.accelerationStructureCount = 1;
			descAsInfo.pAccelerationStructures = &tlas.accel;
			VkDescriptorImageInfo imageInfo{ {}, offscreens[i].offscreenColorBuffer.imageView, VK_IMAGE_LAYOUT_GENERAL};

			std::vector<VkWriteDescriptorSet> writes;
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 0, &descAsInfo));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 1, &imageInfo));
			vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		}
	}

	OffscreenImages createOffscreenImages() {
		OffscreenImages images{};

		//create offscreen color buffer
		VkImage offscreenColorImage = devices.createImage({ swapchain.extent.width,swapchain.extent.height, 1 },
			offscreenColorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkImageView offscreenColorImageView = vktools::createImageView(devices.device, offscreenColorImage,
			VK_IMAGE_VIEW_TYPE_2D, offscreenColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		VkSampler sampler;
		VkSamplerCreateInfo samplerInfo = vktools::initializers::samplerCreateInfo(
			devices.availableFeatures, devices.properties);
		VK_CHECK_RESULT(vkCreateSampler(devices.device, &samplerInfo, nullptr, &sampler));

		images.offscreenColorBuffer.init(&devices, offscreenColorImage, offscreenColorImageView, sampler);

		//create offscreen depth buffer
		VkImage offscreenDepthImage = devices.createImage({ swapchain.extent.width,swapchain.extent.height, 1 },
			offscreenDepthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkImageView offscreenDepthImageView = vktools::createImageView(devices.device, offscreenDepthImage,
			VK_IMAGE_VIEW_TYPE_2D, offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

		images.offscreenDepthBuffer.init(&devices, offscreenDepthImage, offscreenDepthImageView);

		//setting image layout
		VkCommandBuffer cmdBuf = devices.beginOneTimeSubmitCommandBuffer();
		vktools::setImageLayout(cmdBuf, images.offscreenColorBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		vktools::setImageLayout(cmdBuf, images.offscreenDepthBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
		devices.endOneTimeSubmitCommandBuffer(cmdBuf);

		return images;
	}

	void createOffscreenRender() {
		for (auto& offscreen : offscreens) {
			offscreen.offscreenColorBuffer.cleanup();
			offscreen.offscreenDepthBuffer.cleanup();
		}
		offscreens.clear();

		size_t nbImages = static_cast<size_t>(MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < nbImages; ++i) {
			offscreens.push_back(createOffscreenImages());
		}

		//create renderpass for the offscreen
		if (!offscreenRenderPass) {
			offscreenRenderPass = vktools::createRenderPass(devices.device,
				{ offscreenColorFormat },
				offscreenDepthFormat,
				1,
				true,
				true,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_GENERAL);
		}

		//create framebuffers for the offscreen
		if (offscreenFramebuffers.empty()) {
			offscreenFramebuffers.assign(nbImages, {});
		}
		else {
			for (auto& framebuffer : offscreenFramebuffers) {
				vkDestroyFramebuffer(devices.device, framebuffer, nullptr);
			}
		}
		
		for (size_t i = 0; i < nbImages; ++i) {
			std::array<VkImageView, 2> attachments{ offscreens[i].offscreenColorBuffer.imageView,
				offscreens[i].offscreenDepthBuffer.imageView };

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass		= offscreenRenderPass;
			framebufferInfo.attachmentCount = 2;
			framebufferInfo.pAttachments	= attachments.data();
			framebufferInfo.width			= swapchain.extent.width;
			framebufferInfo.height			= swapchain.extent.height;
			framebufferInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(devices.device, &framebufferInfo, nullptr, &offscreenFramebuffers[i]));
		}
	}

	void createDescriptorSet() {
		uint32_t nbTexture = 0;

		//camera matrix
		descriptorSetBindings.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		//scene description
		descriptorSetBindings.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		//textures
		/*descriptorSetBindings.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nbTexture,
			VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);*/

		uint32_t nbDescriptorSet = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		descriptorSetLayout = descriptorSetBindings.createDescriptorSetLayout(devices.device);
		descriptorPool = descriptorSetBindings.createDescriptorPool(devices.device, nbDescriptorSet);
		
		VkDescriptorSetAllocateInfo descInfo{};
		std::vector<VkDescriptorSetLayout> layout(nbDescriptorSet, descriptorSetLayout);
		descInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descInfo.descriptorPool = descriptorPool;
		descInfo.descriptorSetCount = nbDescriptorSet;
		descInfo.pSetLayouts = layout.data();
		descriptorSets.reserve(nbDescriptorSet);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(devices.device, &descInfo, descriptorSets.data()));
	}

	void updateRtDescriptorSet(size_t currentFrame) {
		VkDescriptorImageInfo imageInfo{ {}, offscreens[currentFrame].offscreenColorBuffer.imageView,
			VK_IMAGE_LAYOUT_GENERAL };
		VkWriteDescriptorSet wds = rtDescriptorSetBindings.makeWrite(rtDescriptorSets[currentFrame], 1, &imageInfo);
		vkUpdateDescriptorSets(devices.device, 1, &wds, 0, nullptr);
	}
};

//entry point
RUN_APPLICATION_MAIN(VulkanApp, 800, 600, "ray tracing");
