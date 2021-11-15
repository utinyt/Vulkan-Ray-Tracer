#pragma once
#include "vulkan_utils.h"
#include "vulkan_mesh.h"
#include "vulkan_gltf.h"

struct VulkanDevice;

/*
* Bottom level acceleration structure
*/

/* single geometry info for bottom-level acceleration structure */
struct AsGeometry {
	VkAccelerationStructureGeometryKHR asGeometry;
	VkAccelerationStructureBuildRangeInfoKHR asBuildRangeInfo;
};

/* one blas can have multiple geometries */
struct BlasGeometries {
	std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildRangeInfos;

	void push_back(const AsGeometry& asGeometry) {
		asGeometries.push_back(asGeometry.asGeometry);
		asBuildRangeInfos.push_back(asGeometry.asBuildRangeInfo);
	}
};

/* blas build info */
struct BlasCreateInfo {
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ 
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR 
	};
	const VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfo = nullptr;
	VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{ 
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR 
	};
};

/** acceleration structure handle & buffer */
struct AccelKHR {
	/** acceleration structure handle */
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	/** buffer storing vertex data */
	VkBuffer buffer;
};

/** @brief convert mesh to ray tracing geometry used to build the BLAS */
AsGeometry getVkGeometryKHR(VkDevice device,
	const Mesh& mesh,
	VkBuffer vertexBuffer,
	VkBuffer indexBuffer
);

/** @brief convert mesh to ray tracing geometry used to build the BLAS */
BlasGeometries getBlasGeometriesKHR(VkDevice device, VulkanGLTF& gltfModel);

/** @brief convert gltf primitive to rt geometry used for BLAS */
/*static BlasInput getVkGeometryKHR(VkDevice device, const GltfPrimMesh& primMesh,
	VkBuffer vertexBuffer, VkBuffer indexBuffer);*/

/* @brief create acceleration structure & buffer */
AccelKHR createEmptyAccelerationStructure(VulkanDevice* devices,
	VkAccelerationStructureCreateInfoKHR& info
);

/* @brief build bottom-level acceleration structure */
void buildBlas(VulkanDevice* devices,
	const std::vector<BlasGeometries>& input,
	VkBuildAccelerationStructureFlagsKHR flags,
	std::vector<AccelKHR>& blasHandleOutput
);

/* @brief helper function for buildBlas -> create & build acceleration structure */
void cmdCreateBlas(VulkanDevice* devices,
	VkCommandBuffer cmdBuf,
	std::vector<uint32_t> indices,
	std::vector<BlasCreateInfo>& blasCreateInfos,
	VkDeviceAddress scratchAddress,
	VkQueryPool queryPool,
	std::vector<AccelKHR>& blasHandleOutput
);

/* 
* @brief helper function for buildBlas -> create and replace a new acceleration structure 
*		and buffer based on the size retrieved from the query 
*/
std::vector<AccelKHR> cmdCompactBlas(VulkanDevice* devices,
	VkCommandBuffer cmdBuf,
	std::vector<uint32_t> indices,
	std::vector<BlasCreateInfo>& blasCreateInfos,
	VkQueryPool queryPool,
	std::vector<AccelKHR>& blasHandleOutput
);

/* @brief return the device address of a Blas previously created */
VkDeviceAddress getBlasDeviceAddress(VkDevice device, VkAccelerationStructureKHR accelerationStructure);

/* @brief build top-level acceleration structure */
void buildTlas(VulkanDevice* devices,
	const std::vector<VkAccelerationStructureInstanceKHR>& instances,
	AccelKHR& tlas,
	VkBuffer& instanceBuffer,
	VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
	bool update = false
);
