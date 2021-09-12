#include "vulkan_pipeline.h"

/*
* ctor - init all create info
*/
PipelineGenerator::PipelineGenerator(VkDevice device) {
	this->device = device;
	reset();
}

/*
* set all create info to default settings
*/
void PipelineGenerator::reset() {
	if (device == VK_NULL_HANDLE) {
		throw std::runtime_error("PipelineGenerator::destroyShaderModule(): device handle is null");
	}

	vertexInputBindingDescs.clear();
	vertexInputAttributeDescs.clear();
	colorBlendAttachmentStates.clear();
	pushConstantRanges.clear();
	descriptorSetLayouts.clear();
	destroyShaderModule();

	inputAssemblyStateCreateInfo = 
		vktools::initializers::pipelineInputAssemblyStateCreateInfo();

	viewportStateCreateInfo =
		vktools::initializers::pipelineViewportStateCreateInfo();

	setRasterizerInfo();

	multisampleStateCreateInfo =
		vktools::initializers::pipelineMultisampleStateCreateInfo();

	setDepthStencilInfo();

	setColorBlendInfo(VK_FALSE);

	dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	dynamicStates.shrink_to_fit();

	dynamicStateCreateInfo =
		vktools::initializers::pipelineDynamicStateCreateInfo(
			dynamicStates.data(),
			static_cast<uint32_t>(dynamicStates.size())
		);
}

/*
* add vertex input binding description 
* 
* @param bindings - binding collection
*/
void PipelineGenerator::addVertexInputBindingDescription(
	const std::vector<VkVertexInputBindingDescription>& bindings) {
	vertexInputBindingDescs.insert(vertexInputBindingDescs.end(), bindings.begin(), bindings.end());
}

/*
* add vertex input attribute description
*
* @param attributes - attribute collection
*/
void PipelineGenerator::addVertexInputAttributeDescription(
	const std::vector<VkVertexInputAttributeDescription>& attributes) {
	vertexInputAttributeDescs.insert(vertexInputAttributeDescs.end(), attributes.begin(), attributes.end());
}

/*
* add shader stage create info
* 
* @param shaderModule
* @param stage - shader stage
*/
void PipelineGenerator::addShader(VkShaderModule module, VkShaderStageFlagBits stage) {
	shaderStages.push_back(vktools::initializers::pipelineShaderStageCreateInfo(stage, module));
}

/*
* add push constant range
*
* @param ranges
*/
void PipelineGenerator::addPushConstantRange(const std::vector<VkPushConstantRange>& ranges) {
	pushConstantRanges.insert(pushConstantRanges.begin(), ranges.begin(), ranges.end());
}

/*
* add descriptor set layout 
* 
* @param layouts - descriptor set layouts
*/
void PipelineGenerator::addDescriptorSetLayout(const std::vector<VkDescriptorSetLayout>& layouts) {
	descriptorSetLayouts.insert(descriptorSetLayouts.begin(), layouts.begin(), layouts.end());
}

/*
* (re)set rasterizer info 
* 
* @param polygonMode
* @param cullMode
* @param frontFace
*/
void PipelineGenerator::setRasterizerInfo(VkPolygonMode polygonMode,
	VkCullModeFlags cullMode, VkFrontFace frontFace) {
	rasterizationStateCreateInfo =
		vktools::initializers::pipelineRasterizationStateCreateInfo(polygonMode, cullMode, frontFace);
}

/*
* (re)set color blend info
* 
* @param blendEnable
*/
void PipelineGenerator::setColorBlendInfo(VkBool32 blendEnable) {
	colorBlendAttachmentStates = { vktools::initializers::pipelineColorBlendAttachment(blendEnable) };
	colorBlendAttachmentStates.shrink_to_fit();

	colorBlendStateCreateInfo =
		vktools::initializers::pipelineColorBlendStateCreateInfo(
			static_cast<uint32_t>(colorBlendAttachmentStates.size()),
			colorBlendAttachmentStates.data()
		);
}

/*
* (re)set depth stencil state info 
* 
* @param depthTest
* @param depthWrite
* @param depthCompareOp
*/
void PipelineGenerator::setDepthStencilInfo(VkBool32 depthTest, VkBool32 depthWrite,
	VkCompareOp depthCompareOp) {
	depthStencilStateCreateInfo =
		vktools::initializers::pipelineDepthStencilStateCreateInfo(depthTest, depthWrite, depthCompareOp);
}

/*
* generate pipeline & pipeline layour
* 
* @param renderPass
* @param descriptorSetLayout
* @param outPipeline
* @param outPipelineLayout
*/
void PipelineGenerator::generate(VkRenderPass renderPass,
	VkPipeline& outPipeline,
	VkPipelineLayout& outPipelineLayout) {
	//check minimal info were provided
	if (device == VK_NULL_HANDLE) {
		throw std::runtime_error("PipelineGenerator::destroyShaderModule(): device handle is null");
	}
	if (shaderStages.empty()) {
		throw std::runtime_error("PipelineGenerator::generate(): missing shader stages");
	}
	if (descriptorSetLayouts.empty()) {
		throw std::runtime_error("PipelineGenerator::generate(): missing descriptor set layouts");
	}

	//shrink
	shaderStages.shrink_to_fit();

	//vertex input state info
	vertexInputStateInfo =
		vktools::initializers::pipelineVertexInputStateCreateInfo(
			vertexInputBindingDescs,
			vertexInputAttributeDescs
		);

	//pipeline layout create info
	pipelineLayoutCreateInfo =
		vktools::initializers::pipelineLayoutCreateInfo(
			descriptorSetLayouts,
			pushConstantRanges
		);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &outPipelineLayout));

	//create pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputStateInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineInfo.pViewportState = &viewportStateCreateInfo;
	pipelineInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	pipelineInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineInfo.layout = outPipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, {}, 1, &pipelineInfo, nullptr, &outPipeline));

	//reset all setting
	reset();
}

/*
* destroy all shader module 
*/
void PipelineGenerator::destroyShaderModule() {
	for (auto& shader : shaderStages) {
		vkDestroyShaderModule(device, shader.module, nullptr);
	}
	shaderStages.clear();
}