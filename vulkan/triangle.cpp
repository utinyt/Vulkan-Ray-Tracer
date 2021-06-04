#include "core/vulkan_app_base.h"

class VulkanApp : public VulkanAppBase {
public:
	VulkanApp(int width, int height, const std::string& appName)
		: VulkanAppBase(width, height, appName) {}

	~VulkanApp() {}

	virtual void initApp() override {
		VulkanAppBase::initApp();
		createRenderPass();
		createPipeline();
		createFramebuffers();
		recordCommandBuffer();
	}

private:
	virtual void draw() override {
		//std::cout << "Hello\n"; 
		
	}

	void createRenderPass() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapchain.surfaceFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				//op before rendering - clear
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;				//op after rendering - preserve content
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef; //directly referenced by fragment shader (location  = #)

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // match pWaitDstStageMask in VkSubmitInfo
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VK_CHECK_RESULT(vkCreateRenderPass(devices.device, &renderPassInfo, nullptr, &renderPass));
		LOG("created:\trender pass");
	}

	void createFramebuffers() {

	}

	void createPipeline() {

	}

	void recordCommandBuffer() {

	}
};

//entry point
RUN_APPLICATION_MAIN(VulkanApp, 800, 600, "vkApp");
