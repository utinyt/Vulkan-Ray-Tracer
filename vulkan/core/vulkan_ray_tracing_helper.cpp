#include "vulkan_ray_tracing_helper.h"
#include "vulkan_device.h"

/*
* convert mesh to ray tracing geometry used to build the BLAS
*
* @param device - logical device handle
* @param vertexBuffer
* @param indexBuffer
*
* @return BlasInput
*/
AsGeometry getVkGeometryKHR(VkDevice device, const Mesh& mesh, VkBuffer vertexBuffer, VkBuffer indexBuffer) {
	//triangle data
	VkAccelerationStructureGeometryTrianglesDataKHR asGeometryTrianglesData{};
	asGeometryTrianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	//describe vertex info
	asGeometryTrianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	asGeometryTrianglesData.vertexData.deviceAddress = vktools::getBufferDeviceAddress(device, vertexBuffer);
	asGeometryTrianglesData.vertexStride = mesh.vertexSize;
	asGeometryTrianglesData.maxVertex = static_cast<uint32_t>(mesh.vertexCount);
	//describe index info
	asGeometryTrianglesData.indexType = VK_INDEX_TYPE_UINT32;
	asGeometryTrianglesData.indexData.deviceAddress = vktools::getBufferDeviceAddress(device, indexBuffer);
	//asGeometryTrianglesData.transformData = {}; // null device pointer => identity transform

	//identify above data as containing opaque triangles
	VkAccelerationStructureGeometryKHR asGeometry{};
	asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	//sgeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR; //no any-hit shader invocation
	asGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeometry.geometry.triangles = asGeometryTrianglesData;

	//build range for bottom-level acceleration structure -> whole range
	uint32_t maxPrimitiveCount = static_cast<uint32_t>(mesh.indices.size() / 3);
	VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};
	asBuildRangeInfo.firstVertex = 0;
	asBuildRangeInfo.primitiveCount = maxPrimitiveCount;
	asBuildRangeInfo.primitiveOffset = 0;
	asBuildRangeInfo.transformOffset = 0;

	return { asGeometry, asBuildRangeInfo };
}

/*
* same as above function - but for GLTF model
*/
AsGeometry getVkGeometryKHR(const VulkanGLTF::Primitive& primitive,
	VkDeviceAddress vertexAddress,
	VkDeviceAddress indexAddress,
	VkDeviceSize vertexSize) {
	//triangle data
	VkAccelerationStructureGeometryTrianglesDataKHR asGeometryTrianglesData{};
	asGeometryTrianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	//describe vertex info
	asGeometryTrianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	asGeometryTrianglesData.vertexData.deviceAddress = vertexAddress;
	asGeometryTrianglesData.vertexStride = sizeof(glm::vec3);
	asGeometryTrianglesData.maxVertex = primitive.vertexCount;
	//describe index info
	asGeometryTrianglesData.indexType = VK_INDEX_TYPE_UINT32;
	asGeometryTrianglesData.indexData.deviceAddress = indexAddress;
	//geometryTrianglesData.transformData = {}; // null device pointer => identity transform

	//identify above data as containing opaque triangles
	VkAccelerationStructureGeometryKHR asGeometry{};
	asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	//sgeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR; //no any-hit shader invocation
	asGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeometry.geometry.triangles = asGeometryTrianglesData;

	//build range for bottom-level acceleration structure -> whole range
	uint32_t maxPrimitiveCount = static_cast<uint32_t>(primitive.indexCount / 3);
	VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo{};
	asBuildRangeInfo.firstVertex = 0; primitive.vertexOffset;
	asBuildRangeInfo.primitiveCount = maxPrimitiveCount;
	asBuildRangeInfo.primitiveOffset = primitive.firstIndex * sizeof(uint32_t);
	asBuildRangeInfo.transformOffset = 0;

	return { asGeometry, asBuildRangeInfo };
}

/*
* acculmulate single geometries to BlasGeometries
*/
//void getBlasGeometriesKHR(VulkanGLTF::Node& node,
//	VkDeviceAddress vertexAddress,
//	VkDeviceAddress indexAddress,
//	VkDeviceSize vertexSize,
//	BlasGeometries& blasGeometries) {
//	
//	for (VulkanGLTF::Primitive& primitive : node.mesh) {
//		blasGeometries.push_back(getVkGeometryKHR(primitive, vertexAddress, indexAddress, vertexSize));
//	}
//	for (VulkanGLTF::Node& child : node.children) {
//		getBlasGeometriesKHR(child, vertexAddress, indexAddress, vertexSize, blasGeometries);
//	}
//}

/*
* convert gltf model to ray tracing geometry used to build the BLAS
*
* @param device - logical device handle
* @param gltfModel
*
* @return BlasInput
*/
//BlasGeometries getBlasGeometriesKHR(VkDevice device, VulkanGLTF& gltfModel) {
//	VkDeviceAddress vertexAddress = vktools::getBufferDeviceAddress(device, gltfModel.vertexBuffer);
//	VkDeviceAddress indexAddress = vktools::getBufferDeviceAddress(device, gltfModel.indexBuffer);
//
//	BlasGeometries geometries;
//	for (VulkanGLTF::Node& node : gltfModel.nodes) {
//		getBlasGeometriesKHR(node, vertexAddress, indexAddress, gltfModel.vertexSize, geometries);
//	}
//
//	return geometries;
//}

/*
* create acceleration structure & buffer
*
* @param info - acceleration structure create info
*
* @return AccelKHR - contain created acceleration structure & buffer
*/
AccelKHR createEmptyAccelerationStructure(VulkanDevice* devices, VkAccelerationStructureCreateInfoKHR& info) {
	AccelKHR resultAs;
	devices->createBuffer(resultAs.buffer, info.size,
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

	info.buffer = resultAs.buffer;
	vkfp::vkCreateAccelerationStructureKHR(devices->device, &info, nullptr, &resultAs.accel);
	return resultAs;
}

/*
* build bottom-level acceleration structure
*
* @param input - vector blas input built from mesh
* @param flags -
*	VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR - given acceleration structure build should priortize trace performance
*	VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR -
*/
void buildBlas(VulkanDevice* devices,
	const std::vector<BlasGeometries>& input,
	VkBuildAccelerationStructureFlagsKHR flags,
	std::vector<AccelKHR>& blasHandleOutput) {
	blasHandleOutput.resize(input.size());
	uint32_t nbBlas = static_cast<uint32_t>(input.size());
	uint32_t nbCompactions{ 0 };
	VkDeviceSize maxScratchSize{ 0 };
	std::vector<BlasCreateInfo> blasCreateInfos(input.size());

	for (uint32_t blasIndex = 0; blasIndex < nbBlas; ++blasIndex) {
		//fill VkAccelerationStructureBuildGeometryInfoKHR partially for querying the build sizes
		blasCreateInfos[blasIndex].buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		blasCreateInfos[blasIndex].buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		blasCreateInfos[blasIndex].buildInfo.flags = flags;
		blasCreateInfos[blasIndex].buildInfo.geometryCount = static_cast<uint32_t>(input[blasIndex].asGeometries.size());
		blasCreateInfos[blasIndex].buildInfo.pGeometries = input[blasIndex].asGeometries.data();

		//build range info
		blasCreateInfos[blasIndex].buildRangeInfo = input[blasIndex].asBuildRangeInfos.data();

		//finding sizes to create acceleration structures and scratch
		std::vector<uint32_t> maxPrimCount(input[blasIndex].asBuildRangeInfos.size());
		for (size_t i = 0; i < maxPrimCount.size(); ++i) {
			maxPrimCount[i] = input[blasIndex].asBuildRangeInfos[i].primitiveCount;
		}
		vkfp::vkGetAccelerationStructureBuildSizesKHR(devices->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&blasCreateInfos[blasIndex].buildInfo, maxPrimCount.data(), &blasCreateInfos[blasIndex].buildSizesInfo);

		//keep track of the maximum scratch memory to allocate scratch buffer -> we'll reuse that scratch buffer
		maxScratchSize = std::max(maxScratchSize, blasCreateInfos[blasIndex].buildSizesInfo.buildScratchSize);
		nbCompactions += (blasCreateInfos[blasIndex].buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) != 0;
	}

	//create scratch buffer
	VkBuffer scratchBuffer;
	VkBufferCreateInfo scratchBufferInfo = vktools::initializers::bufferCreateInfo(maxScratchSize, //use the maximum scratch size
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	VK_CHECK_RESULT(vkCreateBuffer(devices->device, &scratchBufferInfo, nullptr, &scratchBuffer));
	devices->memoryAllocator.allocateBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); //memProperties = DEVICE_LOCAL

	VkDeviceAddress scratchAddress = vktools::getBufferDeviceAddress(devices->device, scratchBuffer);

	//querying the real size of BLAS
	VkQueryPool queryPool{ VK_NULL_HANDLE };
	if (nbCompactions > 0) {
		assert(nbCompactions == nbBlas); //don't allow mix of on/off compaction
		VkQueryPoolCreateInfo queryPoolCreateInfo{};
		queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		queryPoolCreateInfo.queryCount = nbBlas;
		queryPoolCreateInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		vkCreateQueryPool(devices->device, &queryPoolCreateInfo, nullptr, &queryPool);
	}

	//split BLAS creation into chunks of ~256MB to avoid pipeline stalling
	std::vector<uint32_t> indices;
	VkDeviceSize batchSize{ 0 };
	VkDeviceSize batchLimit{ 256'000'000 }; // 256 MB

	for (uint32_t blasIndex = 0; blasIndex < nbBlas; ++blasIndex) {
		indices.push_back(blasIndex);
		batchSize += blasCreateInfos[blasIndex].buildSizesInfo.accelerationStructureSize;

		//over the limit or last blas element
		if (batchSize >= batchLimit || blasIndex == nbBlas - 1) {
			VkCommandBuffer cmdBuf = devices->beginCommandBuffer();
			cmdCreateBlas(devices, cmdBuf, indices, blasCreateInfos, scratchAddress, queryPool, blasHandleOutput);
			devices->endCommandBuffer(cmdBuf);

			if (queryPool) {
				VkCommandBuffer cmdBuf = devices->beginCommandBuffer();
				auto cleanupAs = cmdCompactBlas(devices, cmdBuf, indices, blasCreateInfos, queryPool, blasHandleOutput);
				devices->endCommandBuffer(cmdBuf);

				//delete src acceleration structures
				for (auto& as : cleanupAs) {
					vkfp::vkDestroyAccelerationStructureKHR(devices->device, as.accel, nullptr);
					devices->memoryAllocator.freeBufferMemory(as.buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
					vkDestroyBuffer(devices->device, as.buffer, nullptr);
				}
			}

			//reset
			batchSize = 0;
			indices.clear();
		}
	}

	//cleanup
	vkDestroyQueryPool(devices->device, queryPool, nullptr);
	devices->memoryAllocator.freeBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, scratchBuffer, nullptr);
}

/*
* record building acceleration structure command to the command buffer
*
* @param cmdBuf - command buffer to record
* @param indices - indices of blas
* @param buildAs - vector of acceleration structure info
* @param scratchAddress - buffer address where ac data is temporarily stored
* @param queryPool - query to find the real amount of memory
*/
void cmdCreateBlas(VulkanDevice* devices,
	VkCommandBuffer cmdBuf,
	std::vector<uint32_t> indices,
	std::vector<BlasCreateInfo>& blasCreateInfos,
	VkDeviceAddress scratchAddress,
	VkQueryPool queryPool,
	std::vector<AccelKHR>& blasHandleOutput) {
	if (queryPool) {
		vkResetQueryPool(devices->device, queryPool, 0, static_cast<uint32_t>(indices.size()));
	}
	uint32_t queryCount = 0;

	for (uint32_t blasIndex : indices) {
		//actual allocation of buffer and acceleration structure
		VkAccelerationStructureCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = blasCreateInfos[blasIndex].buildSizesInfo.accelerationStructureSize;
		blasHandleOutput[blasIndex] = createEmptyAccelerationStructure(devices, createInfo);

		blasCreateInfos[blasIndex].buildInfo.dstAccelerationStructure = blasHandleOutput[blasIndex].accel;
		blasCreateInfos[blasIndex].buildInfo.scratchData.deviceAddress = scratchAddress;

		//build the blas
		vkfp::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &blasCreateInfos[blasIndex].buildInfo, &blasCreateInfos[blasIndex].buildRangeInfo);

		//since scratch buffer is resued across builds, we need a barrier to ensure one build is finished befire starting the next one
		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
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
			vkfp::vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &blasCreateInfos[blasIndex].buildInfo.dstAccelerationStructure,
				VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCount++);
		}
	}
}

/*
* create and replace a new acceleration structure and buffer based on the size retrieved by the query
*
* @param cmdBuf - command buffer to record
* @param indices - indices of blas
* @param buildAs - vector of acceleration structure info
* @param queryPool - query to find the real amount of memory
*
* @return cleanupAs - non-conpacted (old) acceleration structures to delete
*/
std::vector<AccelKHR> cmdCompactBlas(VulkanDevice* devices,
	VkCommandBuffer cmdBuf,
	std::vector<uint32_t> indices,
	std::vector<BlasCreateInfo>& blasCreateInfos,
	VkQueryPool queryPool,
	std::vector<AccelKHR>& blasHandleOutput) {
	uint32_t queryCount = 0;
	std::vector<AccelKHR> cleanupAs;

	//get the compacted size result back
	std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
	vkGetQueryPoolResults(devices->device, queryPool, 0, (uint32_t)compactSizes.size(),
		compactSizes.size() * sizeof(VkDeviceSize), compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);

	for (uint32_t blasIndex : indices) {
		cleanupAs.push_back(blasHandleOutput[blasIndex]);
		blasCreateInfos[blasIndex].buildSizesInfo.accelerationStructureSize = compactSizes[queryCount++]; //new reduced size

		//create a compact version of the AS
		VkAccelerationStructureCreateInfoKHR asCreateInfo{};
		asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		asCreateInfo.size = blasCreateInfos[blasIndex].buildSizesInfo.accelerationStructureSize;
		asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		blasHandleOutput[blasIndex] = createEmptyAccelerationStructure(devices, asCreateInfo);

		//copy the original blas to a compact version
		VkCopyAccelerationStructureInfoKHR copyInfo{};
		copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
		copyInfo.src = cleanupAs.back().accel;
		copyInfo.dst = blasHandleOutput[blasIndex].accel;
		copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
		vkfp::vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
	}

	return cleanupAs;
}

/*
* return the device address of a Blas previously created
*
* @param accelerationStructure
*/
VkDeviceAddress getBlasDeviceAddress(VkDevice device, VkAccelerationStructureKHR accelerationStructure) {
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = accelerationStructure;
	return vkfp::vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);
}

/*
* build top-level acceleration structure
*
* @param instance -
* @param flags - used for acceleration structure build info
*/
void buildTlas(VulkanDevice* devices,
	const std::vector<VkAccelerationStructureInstanceKHR>& instances,
	AccelKHR& tlas,
	VkBuffer& instanceBuffer,
	VkBuildAccelerationStructureFlagsKHR flags,
	bool update) {
	//cannot call buildTlas twice except to update
	if (tlas.accel != VK_NULL_HANDLE && update == false) {
		throw std::runtime_error("buildTlas() called twice");
	}

	//create a buffer holding the actual instance data for use by the AS builder
	VkDeviceSize instanceDescsSizeInBytes = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

	//allocate the instance buffer and copy its contents from host to device memory
	if (update) {
		devices->memoryAllocator.freeBufferMemory(instanceBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices->device, instanceBuffer, nullptr);
	}

	/*
	* staging instance data
	*/
	//create staging buffer
	VkBuffer stagingBuffer;
	VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
		instanceDescsSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VK_CHECK_RESULT(vkCreateBuffer(devices->device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

	//suballocate
	VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices->memoryAllocator.allocateBufferMemory(
		stagingBuffer, properties);

	hostVisibleMemory.mapData(devices->device, instances.data());

	//create instance buffer
	VkBufferCreateInfo bufferCreateInfo = vktools::initializers::bufferCreateInfo(
		instanceDescsSizeInBytes,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
	VK_CHECK_RESULT(vkCreateBuffer(devices->device, &bufferCreateInfo, nullptr, &instanceBuffer));

	//suballocation
	devices->memoryAllocator.allocateBufferMemory(instanceBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkCommandBuffer cmdBuf = devices->beginCommandBuffer();

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

	/*
	* create tlas
	*/
	//create VkAccelerationStructureGeometryInstanceDataKHR
	//This wraps a device pointer to the above uploaded instances
	VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
	instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instancesData.arrayOfPointers = VK_FALSE;
	instancesData.data.deviceAddress = vktools::getBufferDeviceAddress(devices->device, instanceBuffer);

	//put the above into a VkAccelerationStructureGeometryKHR
	//we need to put the instances struct in a union and label it as instance data
	VkAccelerationStructureGeometryKHR topAsGeometry{};
	topAsGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	topAsGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	topAsGeometry.geometry.instances = instancesData;

	//query the needed memory for the tlas and scratch space
	VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
	tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	tlasBuildInfo.flags = flags;
	tlasBuildInfo.geometryCount = 1;
	tlasBuildInfo.pGeometries = &topAsGeometry;
	tlasBuildInfo.mode = update
		? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
		: VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	tlasBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

	uint32_t count = (uint32_t)instances.size();
	VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkfp::vkGetAccelerationStructureBuildSizesKHR(devices->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&tlasBuildInfo, &count, &tlasSizeInfo);

	//create tlas
	if (update == false) {
		VkAccelerationStructureCreateInfoKHR tlasCreateInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
		tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		tlasCreateInfo.size = tlasSizeInfo.accelerationStructureSize;
		tlas = createEmptyAccelerationStructure(devices, tlasCreateInfo);
	}

	//allocate the scratch memory
	VkBuffer scratchBuffer = VK_NULL_HANDLE;
	VkBufferCreateInfo scratchBufferInfo = vktools::initializers::bufferCreateInfo(tlasSizeInfo.buildScratchSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	VK_CHECK_RESULT(vkCreateBuffer(devices->device, &scratchBufferInfo, nullptr, &scratchBuffer));
	devices->memoryAllocator.allocateBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//update build info
	tlasBuildInfo.srcAccelerationStructure = update ? tlas.accel : VK_NULL_HANDLE;
	tlasBuildInfo.dstAccelerationStructure = tlas.accel;
	tlasBuildInfo.scratchData.deviceAddress = vktools::getBufferDeviceAddress(devices->device, scratchBuffer);

	//build offset info : n instances
	VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{ static_cast<uint32_t>(instances.size()), 0, 0, 0 };
	const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

	//build the tlas
	vkfp::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &tlasBuildInfo, &pBuildOffsetInfo);

	devices->endCommandBuffer(cmdBuf);
	devices->memoryAllocator.freeBufferMemory(stagingBuffer, properties);
	vkDestroyBuffer(devices->device, stagingBuffer, nullptr);
	devices->memoryAllocator.freeBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, scratchBuffer, nullptr);
}
