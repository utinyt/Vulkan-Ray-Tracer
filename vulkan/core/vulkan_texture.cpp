#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "vulkan_texture.h"

/*
* clean image & image memory
* must be called if texture is not null
*/
void TextureBase::cleanup() {
	vkDestroySampler(devices->device, sampler, nullptr);
	vkDestroyImageView(devices->device, imageView, nullptr);
	vkDestroyImage(devices->device, texture, nullptr);
	vkFreeMemory(devices->device, textureMemory, nullptr);
}

/*
* load 2d texture from a file
* 
* @param devices - abstracted vulkan device handle
* @param path - texture file path
*/
void Texture2D::load(const VulkanDevice* devices, const std::string& path) {
	this->devices = devices;
	
	//image load
	int width, height, channels;
	stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	VkDeviceSize imageSize = width * height * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture: " + path);
	}

	//staging
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	devices->createBuffer(imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	void* data;
	vkMapMemory(devices->device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(devices->device, stagingBufferMemory);
	stbi_image_free(pixels);

	//image creation
	devices->createImage({ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		texture,
		textureMemory);

	//layout trasition & data transfer
	transitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	devices->copyBufferToImage(stagingBuffer, texture, { 0, 0, 0 },
		{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 });
	transitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	vkDestroyBuffer(devices->device, stagingBuffer, nullptr);
	vkFreeMemory(devices->device, stagingBufferMemory, nullptr);

	//image view creation
	imageView = vktools::createImageView(devices->device, texture,
		VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	if (devices->availableFeatures.samplerAnisotropy == VK_TRUE) {
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = devices->properties.limits.maxSamplerAnisotropy;
	}
	else {
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.f;
	}
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.f;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = 0.f;
	VK_CHECK_RESULT(vkCreateSampler(devices->device, &samplerInfo, nullptr, &sampler));
	
}

/*
* helper function for load()
* 
* @param format
* @param oldLayout
* @param newLayout
*/
void TextureBase::transitionImageLayout(VkFormat format,
	VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer commandBuffer = devices->beginOneTimeSubmitCommandBuffer();
	vktools::setImageLayout(commandBuffer, texture, format, oldLayout, newLayout);
	devices->endOneTimeSubmitCommandBuffer(commandBuffer);
}
