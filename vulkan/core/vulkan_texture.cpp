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
	devices->memoryAllocator.freeImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyImage(devices->device, image, nullptr);
}

/*
* load 2d texture from a file
* 
* @param devices - abstracted vulkan device handle
* @param path - texture file path
*/
void Texture2D::load(VulkanDevice* devices, const std::string& path) {
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
	VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
		imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VK_CHECK_RESULT(vkCreateBuffer(devices->device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

	//suballocation
	VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices->memoryAllocator.allocateBufferMemory(
		stagingBuffer, properties);
	hostVisibleMemory.mapData(devices->device, pixels);
	stbi_image_free(pixels);

	//image creation
	VkImageCreateInfo imageCreateInfo = vktools::initializers::imageCreateInfo(
		{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
	VK_CHECK_RESULT(vkCreateImage(devices->device, &imageCreateInfo, nullptr, &image));

	//image memory (sub)llocation
	devices->memoryAllocator.allocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//layout trasition & data transfer
	transitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	devices->copyBufferToImage(stagingBuffer, image, { 0, 0, 0 },
		{ static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 });
	transitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	devices->memoryAllocator.freeBufferMemory(stagingBuffer, properties);
	vkDestroyBuffer(devices->device, stagingBuffer, nullptr);

	//image view creation
	imageView = vktools::createImageView(devices->device, image,
		VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo samplerInfo = vktools::initializers::samplerCreateInfo(
		devices->availableFeatures, devices->properties);
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
	vktools::setImageLayout(commandBuffer, image, oldLayout, newLayout);
	devices->endOneTimeSubmitCommandBuffer(commandBuffer);
}
