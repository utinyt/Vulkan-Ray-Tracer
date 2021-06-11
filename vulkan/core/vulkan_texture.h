#pragma once
#include "vulkan_device.h"

class VulkanTextureBase {
public:
	void cleanup();
	virtual void load(const VulkanDevice* devices, const std::string& path) = 0;

	/** abstracted vulkan device handle */
	const VulkanDevice* devices = nullptr;

};

class VulkanTexture2D : public VulkanTextureBase {
public:
	virtual void load(const VulkanDevice* devices, const std::string& path) override;
};