#pragma once
#include "vulkan_device.h"

class TextureBase {
public:
	/** @brief destroy image & imageView & sampler */
	void cleanup();
	/** @brief load texture from a file */
	virtual void load(VulkanDevice* devices, const std::string& path, VkFilter filter, VkSamplerAddressMode mode) = 0;
	/** @brief just get the handle from the user and init texture */
	void init(VulkanDevice* devices, VkImage image, VkImageView imageView,
		VkImageLayout imageLayout, VkSampler sampler = VK_NULL_HANDLE) {
		this->devices = devices;
		this->image = image;
		descriptor.imageView = imageView;
		descriptor.sampler = sampler;
		descriptor.imageLayout = imageLayout;
	}

	/** abstracted vulkan device handle */
	VulkanDevice* devices = nullptr;
	/** texture buffer handle */
	VkImage image = VK_NULL_HANDLE;
	/** image descriptor */
	VkDescriptorImageInfo descriptor{VK_NULL_HANDLE, VK_NULL_HANDLE };
	/** mip levels */
	uint32_t mipLevels = 1;
};

class Texture2D : public TextureBase {
public:
	/** @brief load texture from a file and create image & imageView & sampler */
	virtual void load(VulkanDevice* devices, const std::string& path, VkFilter filter, VkSamplerAddressMode mode) override;
	/** @brief load texture from a buffer */
	void load(VulkanDevice* devices, unsigned char* data,
		uint32_t texWidth, uint32_t texHeight, VkDeviceSize imageSize, VkFormat format,
		VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	/** @brief create empty texture (mostly used as destination image) */
	void createEmptyTexture(VulkanDevice* devices, VkExtent2D extent,
		VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage, VkImageLayout layout, VkMemoryPropertyFlags memProperties);
};

class TextureCube : public TextureBase {
public:
	/** @brief load texture from a file and create image & imageView & sampler */
	virtual void load(VulkanDevice* devices, const std::string& path, VkFilter filter, VkSamplerAddressMode mode) override;
};
