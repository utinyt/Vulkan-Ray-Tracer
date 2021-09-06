#include <array>
#include <imgui/imgui.h>
#include "vulkan_imgui.h"

/*
* init context & style & resources
* 
* @param devices - abstracted vulkan device (physical / logical) pointer
*/
void Imgui::init(VulkanDevice* devices, int width, int height,
	VkRenderPass renderPass, uint32_t MAX_FRAMES_IN_FLIGHT) {
	this->devices = devices;
	ImGui::CreateContext();

	//color scheme
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_TitleBg]			= ImVec4(1.f, 0.f, 0.f, 0.6f);
	style.Colors[ImGuiCol_TitleBgActive]	= ImVec4(1.f, 0.f, 0.f, 0.8f);
	style.Colors[ImGuiCol_MenuBarBg]		= ImVec4(1.f, 0.f, 0.f, 0.4f);
	style.Colors[ImGuiCol_Header]			= ImVec4(1.f, 0.f, 0.f, 0.4f);
	style.Colors[ImGuiCol_CheckMark]		= ImVec4(0.f, 1.f, 0.f, 1.0f);

	//dimensions
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
	io.DisplayFramebufferScale = ImVec2(1.f, 1.f);

	//load font texture
	unsigned char* fontData;
	int texWidth, texHeight;
	io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
	fontImage.load(devices,
		fontData,
		texWidth,
		texHeight,
		texWidth * texHeight * 4 * sizeof(char),
		VK_FORMAT_R8G8B8A8_UNORM);
	
	//descriptor pool & layout & allocate
	bindings.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	descriptorPool = bindings.createDescriptorPool(devices->device, MAX_FRAMES_IN_FLIGHT);
	descriptorSetLayout = bindings.createDescriptorSetLayout(devices->device);
	descriptorSets = vktools::allocateDescriptorSets(devices->device, descriptorSetLayout, descriptorPool, MAX_FRAMES_IN_FLIGHT);

	//update descriptor sets
	std::vector<VkWriteDescriptorSet> writes;
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		writes.emplace_back(bindings.makeWrite(descriptorSets[i], 0, &fontImage.descriptor));
	}
	vkUpdateDescriptorSets(devices->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	//pipeline 
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = 
		vktools::initializers::pipelineLayoutCreateInfo(1, &descriptorSetLayout);
	
	VK_CHECK_RESULT(vkCreatePipelineLayout(devices->device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo =
		vktools::initializers::pipelineInputAssemblyStateCreateInfo();

	VkPipelineRasterizationStateCreateInfo rasterizationInfo =
		vktools::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
		vktools::initializers::pipelineColorBlendAttachment(VK_TRUE);

	VkPipelineColorBlendStateCreateInfo colorBlendInfo =
		vktools::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo =
		vktools::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewportStateInfo =
		vktools::initializers::pipelineViewportStateCreateInfo();

	VkPipelineMultisampleStateCreateInfo multisampleStateInfo =
		vktools::initializers::pipelineMultisampleStateCreateInfo();

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateInfo =
		vktools::initializers::pipelineDynamicStateCreateInfo(dynamicStates, 2);

	std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		vktools::initializers::vertexInputBindingDescription(0, sizeof(ImDrawVert))
	};

	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		vktools::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),
		vktools::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),
		vktools::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col))
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo =
		vktools::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
	VkShaderModule vertexShader = vktools::createShaderModule(devices->device,
		vktools::readFile("shaders/imgui_vert.spv"));
	VkShaderModule fragmentShader = vktools::createShaderModule(devices->device,
		vktools::readFile("shaders/imgui_frag.spv"));
	shaderStages[0] = vktools::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader);
	shaderStages[1] = vktools::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, fragmentShader);

	VkGraphicsPipelineCreateInfo pipelineInfo 
		= vktools::initializers::graphicsPipelineCreateInfo(pipelineLayout, renderPass);
	pipelineInfo.pVertexInputState		= &vertexInputStateInfo;
	pipelineInfo.pInputAssemblyState	= &inputAssemblyInfo;
	pipelineInfo.pRasterizationState	= &rasterizationInfo;
	pipelineInfo.pColorBlendState		= &colorBlendInfo;
	pipelineInfo.pMultisampleState		= &multisampleStateInfo;
	pipelineInfo.pViewportState			= &viewportStateInfo;
	pipelineInfo.pDepthStencilState		= &depthStencilStateInfo;
	pipelineInfo.pDynamicState			= &dynamicStateInfo;
	pipelineInfo.stageCount				= static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages				= shaderStages.data();
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(devices->device, {}, 1, &pipelineInfo, nullptr, &pipeline));

	vkDestroyShaderModule(devices->device, vertexShader, nullptr);
	vkDestroyShaderModule(devices->device, fragmentShader, nullptr);
}

/*
* destroy all resources
*/
void Imgui::cleanup() {
	ImGui::DestroyContext();
	//vertex & index buffer
	devices->memoryAllocator.freeBufferMemory(vertexIndexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vkDestroyBuffer(devices->device, vertexIndexBuffer, nullptr);
	//image
	fontImage.cleanup();
	//pipeline
	vkDestroyPipeline(devices->device, pipeline, nullptr);
	vkDestroyPipelineLayout(devices->device, pipelineLayout, nullptr);
	//descriptor
	vkDestroyDescriptorPool(devices->device, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(devices->device, descriptorSetLayout, nullptr);
}
