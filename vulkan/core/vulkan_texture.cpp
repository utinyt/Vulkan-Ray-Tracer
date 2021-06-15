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
	
}

/*
* set pipeline barrier for image layout transition
* record command to the command buffer
* 
* @param commandBuffer - command buffer to record
* @param image - image to transit layout
* @param format
* @param oldLayout
* @param newLayout
*/
void VulkanTextureBase::setImageLayout(VkCommandBuffer commandBuffer, VkImage image,
	VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkImageMemoryBarrier imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarrier.oldLayout = oldLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.layerCount = 1;
	imageBarrier.srcAccessMask = 0;
	imageBarrier.dstAccessMask = 0;

	vkCmdPipelineBarrier(commandBuffer,
		0, 0,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageBarrier);
}

/*
* helper function for load()
* 
* @param format
* @param oldLayout
* @param newLayout
*/
void VulkanTextureBase::transitionImageLayout(VkFormat format,
	VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer commandBuffer = devices->beginOneTimeSubmitCommandBuffer();
	setImageLayout(commandBuffer, texture, format, oldLayout, newLayout);
	devices->endOneTimeSubmitCommandBuffer(commandBuffer);
}
