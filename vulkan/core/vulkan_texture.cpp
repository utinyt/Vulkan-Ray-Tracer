#include <stb_image.h>
#include "vulkan_texture.h"


/*
* clean image & image memory
* must be called if texture is not null
*/
void TextureBase::cleanup() {
	if (devices == nullptr) {
		return;
	}
	vkDestroySampler(devices->device, descriptor.sampler, nullptr);
	descriptor.sampler = VK_NULL_HANDLE;
	vkDestroyImageView(devices->device, descriptor.imageView, nullptr);
	descriptor.imageView = VK_NULL_HANDLE;
	devices->memoryAllocator.freeImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyImage(devices->device, image, nullptr);
	image = VK_NULL_HANDLE;
}

/*
* load 2d texture from a file
* 
* @param devices - abstracted vulkan device handle
* @param path - texture file path
*/
void Texture2D::load(VulkanDevice* devices, const std::string& path, VkFilter filter, VkSamplerAddressMode mode) {
	this->devices = devices;

	//image load
	int width, height, channels;
	stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	VkDeviceSize imageSize = width * height * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture: " + path);
	}

	load(devices, pixels, width, height, imageSize, VK_FORMAT_R8G8B8A8_SRGB, filter, mode);
	stbi_image_free(pixels);
}

/*
* load 2d texture from a file
*
* @param devices - abstracted vulkan device handle
* @param data - buffer storing image
* @param texWidth
* @param texHeight
* @param imageSize - image size in bytes
* @param format - image format
*/
void Texture2D::load(VulkanDevice* devices, unsigned char* data,
	uint32_t texWidth, uint32_t texHeight, VkDeviceSize imageSize, VkFormat format,
	VkFilter filter, VkSamplerAddressMode mode) {
	//image creation
	this->devices = devices;
	cleanup();

	//mip level
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1; // +1 for original image

	/*
	* create image (empty)
	*/
	devices->createImage(image, { texWidth, texHeight, 1 },
		format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		mipLevels,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	/*
	* create image view
	*/
	descriptor.imageView = vktools::createImageView(devices->device, image,
		VK_IMAGE_VIEW_TYPE_2D, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

	/*
	* staging - create starging buffer
	*/
	VkBuffer stagingBuffer;
	VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
		imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VK_CHECK_RESULT(vkCreateBuffer(devices->device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

	VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices->memoryAllocator.allocateBufferMemory(
		stagingBuffer, properties);
	hostVisibleMemory.mapData(devices->device, data);

	/*
	* undefined -> transfer dst optimal
	*/
	VkCommandBuffer cmdBuf = devices->beginCommandBuffer();
	vktools::setImageLayout(cmdBuf,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		mipLevels);

	/*
	* image copy from staging buffer to the iamge
	*/
	VkBufferImageCopy copy = vktools::initializers::bufferCopyRegion({ texWidth, texHeight, 1 });
	vkCmdCopyBufferToImage(cmdBuf,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&copy
	);

	/*
	* generate mipmaps
	*/
	vktools::generateMipmaps(cmdBuf, devices->physicalDevice, image, format, texWidth, texHeight, mipLevels, filter);
	devices->endCommandBuffer(cmdBuf);

	/*
	* cleanup
	*/
	devices->memoryAllocator.freeBufferMemory(stagingBuffer, properties);
	vkDestroyBuffer(devices->device, stagingBuffer, nullptr);

	/*
	* create sampler
	*/
	VkSamplerCreateInfo samplerInfo = vktools::initializers::samplerCreateInfo(
		devices->availableFeatures, devices->properties, filter, mode, mipLevels);
	VK_CHECK_RESULT(vkCreateSampler(devices->device, &samplerInfo, nullptr, &descriptor.sampler));
	descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

/*
* create empty texture (mostly used as destination image) 
*/
void Texture2D::createEmptyTexture(VulkanDevice* devices,
	VkExtent2D extent,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VkImageLayout layout,
	VkMemoryPropertyFlags memProperties) {
	this->devices = devices;
	cleanup();

	//mip level
	//mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1; // +1 for original image

	//create image
	devices->createImage(image, { extent.width, extent.height, 1 }, format, tiling, usage, mipLevels, memProperties);

	//check image aspect
	VkImageAspectFlags imageAspect = 0;
	if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
		imageAspect = VK_IMAGE_ASPECT_COLOR_BIT;
	}
	else if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		if (vktools::hasDepthComponent(format)) {
			imageAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (vktools::hasStencilComponent(format)) {
			imageAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}

	//create image view
	descriptor.imageView = vktools::createImageView(devices->device, image, VK_IMAGE_VIEW_TYPE_2D, format, imageAspect, mipLevels);
	descriptor.imageLayout = layout;
	
	//image layout trasition
	VkCommandBuffer cmdBuf = devices->beginCommandBuffer();
	vktools::setImageLayout(cmdBuf, image, VK_IMAGE_LAYOUT_UNDEFINED, layout, mipLevels);
	devices->endCommandBuffer(cmdBuf);

	//sampler
	VkSamplerCreateInfo samplerInfo = vktools::initializers::samplerCreateInfo(devices->availableFeatures, devices->properties);
	VK_CHECK_RESULT(vkCreateSampler(devices->device, &samplerInfo, nullptr, &descriptor.sampler));
}

/*
* load cube map textures
*
* @param devices - abstracted vulkan device handle
* @param path - path to the folder containing 6 textures
*/
void TextureCube::load(VulkanDevice* devices, const std::string& path, VkFilter filter, VkSamplerAddressMode mode){
	this->devices = devices;
	cleanup();
	//6 texture paths
	std::vector<std::string> paths = {
		path + "/posx.jpg",
		path + "/negx.jpg",
		path + "/posy.jpg",
		path + "/negy.jpg",
		path + "/posz.jpg",
		path + "/negz.jpg"
	};

	//load textures
	int width, height, channels;
	stbi_uc* pixelData[6];
	for (int i = 0; i < 6; ++i) {
		stbi_uc* data = stbi_load(paths[i].c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (data == nullptr) {
			throw std::runtime_error("failed to load texture: " + paths[i]);
		}
		pixelData[i] = data;
	}

	//mip level
	//mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1; // +1 for original image

	VkDeviceSize imageSize = width * height * 4 * 6;
	VkDeviceSize layerSize = imageSize / 6;

	//image create info
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = { static_cast<VkDeviceSize>(width), static_cast<VkDeviceSize>(height), 1 };
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 6; //cube faces
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; //cube map
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	VK_CHECK_RESULT(vkCreateImage(devices->device, &imageInfo, nullptr, &image));
	devices->memoryAllocator.allocateImageMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//staging
	VkBuffer stagingBuffer;
	VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
		imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VK_CHECK_RESULT(vkCreateBuffer(devices->device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

	VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices->memoryAllocator.allocateBufferMemory(
		stagingBuffer, properties);

	//map memory
	unsigned char* data = reinterpret_cast<unsigned char*>(hostVisibleMemory.getHandle(devices->device));
	for (int i = 0; i < 6; ++i) {
		memcpy(data, pixelData[i], layerSize);
		stbi_image_free(pixelData[i]);
		data += layerSize;
	}
	hostVisibleMemory.unmap(devices->device);


	//set image layout & data copy
	VkCommandBuffer cmdBuf = devices->beginCommandBuffer();
	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipLevels;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 6;
	
	vktools::setImageLayout(cmdBuf,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	VkBufferImageCopy copy = vktools::initializers::bufferCopyRegion(
		{ static_cast<VkDeviceSize>(width), static_cast<VkDeviceSize>(height), 1 });
	copy.imageSubresource.layerCount = 6;

	vkCmdCopyBufferToImage(cmdBuf,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&copy);

	descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vktools::setImageLayout(cmdBuf,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		descriptor.imageLayout,
		subresourceRange);

	devices->endCommandBuffer(cmdBuf);

	//create image view
	VkImageViewCreateInfo imageViewInfo = vktools::initializers::imageViewCreateInfo(
		image, VK_IMAGE_VIEW_TYPE_CUBE, VK_FORMAT_R8G8B8A8_UNORM, subresourceRange);
	vkCreateImageView(devices->device, &imageViewInfo, nullptr, &descriptor.imageView);

	//create sampler
	VkSamplerCreateInfo samplerInfo = 
		vktools::initializers::samplerCreateInfo(devices->availableFeatures, devices->properties, filter, mode);
	samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
	VK_CHECK_RESULT(vkCreateSampler(devices->device, &samplerInfo, nullptr, &descriptor.sampler));

	devices->memoryAllocator.freeBufferMemory(stagingBuffer, properties);
	vkDestroyBuffer(devices->device, stagingBuffer, nullptr);
}
