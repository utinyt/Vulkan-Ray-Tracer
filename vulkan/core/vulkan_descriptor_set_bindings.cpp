#include "vulkan_descriptor_set_bindings.h"

/*
* create descriptor pool based on added bindings 
* 
* @param device - logical device handle
* @param maxSets - maximum number of descriptor sets allocated from the pool
* @param flags
*/
VkDescriptorPool DescriptorSetBindings::createDescriptorPool(VkDevice device, uint32_t maxSets,
	VkDescriptorPoolCreateFlags flags) const {
	std::vector<VkDescriptorPoolSize> poolSizes = getRequiredPoolSizes(maxSets);

	VkDescriptorPool descriptorPool;
	VkDescriptorPoolCreateInfo descriptorPoolInfo{};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.maxSets = maxSets;
	descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolInfo.pPoolSizes = poolSizes.data();
	descriptorPoolInfo.flags = flags;

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

	return descriptorPool;
}

/*
* create descriptor set layout 
* 
* @param device - logical device handle
* 
* @return VkDescriptorSetLayout - created layout
*/
VkDescriptorSetLayout DescriptorSetBindings::createDescriptorSetLayout(VkDevice device) const {
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkDescriptorSetLayout layout;
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout));

	return layout;
}

/*
* return descriptor pool sizes built from added bindings 
* 
* @param maximum number of descriptor SETs
* 
* @return std::vector<VkDescriptorPoolSize>
*/
std::vector<VkDescriptorPoolSize> DescriptorSetBindings::getRequiredPoolSizes(uint32_t numSets) const {
	std::vector<VkDescriptorPoolSize> poolSizes;

	for (auto bindingIt = bindings.cbegin(); bindingIt != bindings.cend(); ++bindingIt) {
		//check if same type of VkDescriptorPoolSize struct is already built
		bool typeFound = false;
		for (auto poolSizeIt = poolSizes.begin(); poolSizeIt != poolSizes.end(); ++poolSizeIt) {
			if (poolSizeIt->type == bindingIt->descriptorCount) {
				poolSizeIt->descriptorCount += bindingIt->descriptorCount * numSets;
				typeFound = true;
				break;
			}
		}

		//if there is none, build the struct
		if (!typeFound) {
			VkDescriptorPoolSize poolSize;
			poolSize.type = bindingIt->descriptorType;
			poolSize.descriptorCount = bindingIt->descriptorCount * numSets;
			poolSizes.push_back(poolSize);
		}
	}

	return poolSizes;
}
