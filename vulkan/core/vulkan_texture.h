#pragma once
#include "vulkan_device.h"

class TextureBase {
public:
	void cleanup();
	virtual void load(VulkanDevice* devices, const std::string& path) = 0;

	/** abstracted vulkan device handle */
	VulkanDevice* devices = nullptr;
	/** texture buffer handle */
	VkImage texture;
	/** image view handle */
	VkImageView imageView;
	/** image sampler handle */
	VkSampler sampler;

protected:
	void transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
};

class Texture2D : public TextureBase {
public:
	virtual void load(VulkanDevice* devices, const std::string& path) override;
};