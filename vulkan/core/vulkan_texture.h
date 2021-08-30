#pragma once
#include "vulkan_device.h"

class TextureBase {
public:
	/** @brief destroy image & imageView & sampler */
	void cleanup();
	virtual void load(VulkanDevice* devices, const std::string& path) = 0;
	/** @brief just get the handle from the user and init texture */
	void init(VulkanDevice* devices, VkImage image, VkImageView imageView, VkSampler sampler = VK_NULL_HANDLE) {
		this->devices = devices;
		this->image = image;
		this->imageView = imageView;
		this->sampler = sampler;
	}

	/** abstracted vulkan device handle */
	VulkanDevice* devices = nullptr;
	/** texture buffer handle */
	VkImage image = VK_NULL_HANDLE;
	/** image view handle */
	VkImageView imageView = VK_NULL_HANDLE;
	/** image sampler handle */
	VkSampler sampler = VK_NULL_HANDLE;

protected:
	void transitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
};

class Texture2D : public TextureBase {
public:
	/** @brief load texture from a file and create image & imageView & sampler */
	virtual void load(VulkanDevice* devices, const std::string& path) override;
};