#pragma once
#include "vulkan_device.h"

class VulkanTextureBase {
public:
	void cleanup();
	virtual void load(const VulkanDevice* devices, const std::string& path) = 0;

	/** abstracted vulkan device handle */
	const VulkanDevice* devices = nullptr;
	/** texture buffer handle */
	VkImage texture;
	/** texture buffer memory handle */
	VkDeviceMemory textureMemory;
	/** image view handle */
	VkImageView imageView;
	/** image sampler handle */
	VkSampler sampler;

protected:
	void transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
};

class VulkanTexture2D : public VulkanTextureBase {
public:
	virtual void load(const VulkanDevice* devices, const std::string& path) override;
};