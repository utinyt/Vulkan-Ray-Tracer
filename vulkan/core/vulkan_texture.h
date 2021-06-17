#pragma once
#include "vulkan_device.h"

class TextureBase {
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

class Texture2D : public TextureBase {
public:
	virtual void load(const VulkanDevice* devices, const std::string& path) override;
};