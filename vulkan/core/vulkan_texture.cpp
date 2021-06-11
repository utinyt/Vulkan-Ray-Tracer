#define STB_IMAGE_IMPLEMENTAION
#include <stb_image.h>
#include "vulkan_texture.h"

/*
* load 2d texture from a file
* 
* @param devices - abstracted vulkan device handle
* @param path - texture file path
*/
void VulkanTexture2D::load(const VulkanDevice* devices, const std::string& path) {
	this->devices = devices;
	
	int width, height, channels;
	stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	VkDeviceSize imageSize = width * height * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture: " + path);
	}

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

	stbi_image_free(pixels);
}
