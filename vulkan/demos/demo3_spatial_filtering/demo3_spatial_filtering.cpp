#include <include/imgui/imgui.h>
#include "core/vulkan_app_base.h"
#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "core/vulkan_ray_tracing_helper.h"
#include "core/vulkan_imgui.h"
#include "core/vulkan_pipeline.h"
#include "core/vulkan_framebuffer.h"
#include "core/vulkan_gltf.h"

/*
* derived imgui class - define newFrame();
*/
class Imgui : public ImguiBase {
public:
	virtual void newFrame() override {
		ImGui::NewFrame();
		ImGui::Begin("Settings", 0, flags);

		ImGui::Text("Shadow");
		static int shadow = 0;
		ImGui::RadioButton("Soft shadow", &shadow, 0); ImGui::SameLine();
		ImGui::RadioButton("Hard shadow", &shadow, 1);
		if (shadow != userInput.shadow) {
			userInput.shadow = shadow;
			frameReset = true;
		}
		ImGui::NewLine();

		static glm::vec3 lightPos = { 24.382f, 30.f, 0.1f };
		ImGui::Text("Light sphere position");
		ImGui::SliderFloat("X [-30, 30]", &lightPos.x, -30.0f, 30.0f);
		ImGui::SliderFloat("Y [-30, 30]", &lightPos.y, -30.0f, 30.0f);
		ImGui::SliderFloat("Z [-30, 30]", &lightPos.z, -30.0f, 30.0f);
		if (lightPos != userInput.lightPos) {
			userInput.lightPos = lightPos;
			frameReset = true;
		}
		ImGui::NewLine();

		static float radius = 3, lightIntensity = 70;
		static int depth = 3, rayPerPixel = 2;
		
		ImGui::Text("Light sphere radius");
		ImGui::SliderFloat("[1, 10]", &radius, 1, 10);
		ImGui::Text("Light intensity");
		ImGui::SliderFloat("[1, 100]", &lightIntensity, 1, 100);
		ImGui::Text("Maximum ray depth");
		ImGui::SliderInt("[1, 30]", &depth, 1, 30);
		ImGui::Text("Ray per pixel (!Warning: drastic frame drop)");
		ImGui::SliderInt("[1, 32]", &rayPerPixel, 1, 32);
		if (radius != userInput.radius || 
			depth != userInput.maxRayDepth || 
			lightIntensity != userInput.lightInternsity || 
			rayPerPixel != userInput.rayPerPixel) {
			userInput.radius = radius;
			userInput.maxRayDepth = depth;
			userInput.lightInternsity = lightIntensity;
			userInput.rayPerPixel = rayPerPixel;
			frameReset = true;
		}
		ImGui::NewLine();

		static glm::vec3 scale = { 129.398f, 1.f, 0.01f };
		ImGui::Text("Scale");
		ImGui::SliderFloat("X [0.01, 200]", &scale.x, 0.01f, 200.0f);
		ImGui::SliderFloat("Y [0.01, 1]", &scale.y, 0.01f, 1.0f);
		ImGui::SliderFloat("Z [0.01, 1]", &scale.z, 0.01f, 1.0f);
		if (scale != userInput.scale) {
			userInput.scale = scale;
			frameReset = true;
		}

		ImGui::End();
		ImGui::Render();
	}

	/** imgui user input collection */
	struct UserInput {
		int maxRayDepth = 5;
		int rayPerPixel = 1;
		float radius = 6;
		float lightInternsity = 100;
		int shadow = 0;
		glm::vec3 lightPos{ 24.382f, 30.f, 0.1f };
		glm::vec3 scale{ 38.398f, 0.610f, 0.315f };
	}userInput;

	bool frameReset = false;
};

class VulkanApp : public VulkanAppBase {
public:
	/*
	* constructor - get window size & title
	*/
	VulkanApp(int width, int height, const std::string& appName)
		: VulkanAppBase(width, height, appName) {
		//extensions
		enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);

		//imgui
		imguiBase = new Imgui;

		//command buffer settings
		commandPoolFlags =
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | //short lived
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; //automatic reset via vkBeginCommandBuffer
		buildCommandBuffersEveryFrame = true;

		MAX_FRAMES_IN_FLIGHT = 1;
	}

	/*
	* destructor - destroy vulkan objects created in this level
	*/
	~VulkanApp() {
		//imgui
		imguiBase->cleanup();
		delete imguiBase;

		devices.memoryAllocator.freeBufferMemory(rtSBTBuffer,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkDestroyBuffer(devices.device, rtSBTBuffer, nullptr);
		vkDestroyPipeline(devices.device, rtPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, rtPipelineLayout, nullptr);
		vkDestroyPipeline(devices.device, postPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, postPipelineLayout, nullptr);
		vkDestroyRenderPass(devices.device, postRenderPass, nullptr);
		vkDestroyPipeline(devices.device, imageFilteringPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, imageFilteringPipelineLayout, nullptr);
		vkDestroyRenderPass(devices.device, imageFilteringRenderPass, nullptr);

		devices.memoryAllocator.freeBufferMemory(sceneBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, sceneBuffer, nullptr);

		//BLAS
		for (auto& as : blasHandles) {
			vkfp::vkDestroyAccelerationStructureKHR(devices.device, as.accel, nullptr);
			devices.memoryAllocator.freeBufferMemory(as.buffer);
			vkDestroyBuffer(devices.device, as.buffer, nullptr);
		}

		//instance buffer
		devices.memoryAllocator.freeBufferMemory(instanceBuffer);
		vkDestroyBuffer(devices.device, instanceBuffer, nullptr);

		//TLAS
		vkfp::vkDestroyAccelerationStructureKHR(devices.device, tlas.accel, nullptr);
		devices.memoryAllocator.freeBufferMemory(tlas.buffer);
		vkDestroyBuffer(devices.device, tlas.buffer, nullptr);

		//swapchain framebuffers
		for (auto& framebuffer : framebuffers) {
			vkDestroyFramebuffer(devices.device, framebuffer, nullptr);
		}

		//pingpong framebuffer
		for (auto& framebuffer : pingpongFramebuffer) {
			framebuffer.cleanup();
		}

		//rt output images
		rtOutputColor.cleanup();
		rtOutputPos.cleanup();
		rtOutputNormal.cleanup();
		rtOutputAlbedo.cleanup();

		//descriptors
		vkDestroyDescriptorPool(devices.device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, rtDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, rtDescriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, postDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, postDescriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, imageFilteringDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, imageFilteringDescriptorSetLayout, nullptr);

		//uniform buffers
		for (auto& uniformBuffer : uniformBuffers) {
			devices.memoryAllocator.freeBufferMemory(uniformBuffer,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			vkDestroyBuffer(devices.device, uniformBuffer, nullptr);
		}

		//models
		gltfDioramaModel.cleanup();
	}

	/*
	* application initialization - also contain base class initApp()
	*/
	virtual void initApp() override {
		VulkanAppBase::initApp();

		//camera
		camera.camPos = glm::vec3(-10, 15, 40);
		camera.camFront = glm::vec3(10, -10, -40);
		camera.camUp = glm::vec3(0, 1, 0);

		//request ray tracing properties
		VkPhysicalDeviceProperties2 properties2{};
		properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties2.pNext = &rtProperties;
		rtProperties.pNext = &asProperties;
		vkGetPhysicalDeviceProperties2(devices.physicalDevice, &properties2);

		//create & build bottom-level acceleration structure
		createBottomLevelAccelerationStructure();
		//create & build top-level acceleration structure
		createTopLevelAccelerationStructure();

		//uniform buffers
		createUniformbuffer();
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updateUniformBuffer(i);
		}

		//push bunny obj instance
		objInstances.push_back({ glm::mat4(1.f), glm::mat4(1.f),
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.vertexBuffer),
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.indexBuffer),
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.normalBuffer),
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.uvBuffer),
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.colorBuffer),
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.tangentBuffer),
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.materialIndicesBuffer),
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.materialBuffer), 
			vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.primitiveBuffer) 
			}
		);
		//scene description buffer
		createBuffer(objInstances.data(), static_cast<VkDeviceSize>(objInstances.size() * sizeof(ObjInstance)),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sceneBuffer);

		/*
		* ray tracing
		*/
		createRaytraceOutputImages();
		createDescriptorSet(); //shared descriptor
		createRtDescriptorSet();
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updateRtDescriptorSet(i);
		}
		createRtPipeline();
		createRtShaderBindingTable();

		/*
		* image filtering
		*/
		createImageFilteringResources();
		createImageFilteringDescriptorSet();
		createImageFilteringPipeline();
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			updateImageFilteringDescriptorSet(i);
		}

		/*
		* full-quad
		*/
		//post descriptor set
		createPostDescriptorSet();
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updatePostDescriptorSet(i);
		}
		//render pass
		postRenderPass = vktools::createRenderPass(devices.device,
			{ swapchain.surfaceFormat.format },
			depthFormat,
			VK_SAMPLE_COUNT_1_BIT, 1, true, true,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
		//pipeline
		createPostPipeline();

		//imgui
		imguiBase->init(&devices, swapchain.extent.width, swapchain.extent.height, postRenderPass, MAX_FRAMES_IN_FLIGHT, VK_SAMPLE_COUNT_1_BIT);
		//framebuffer
		createFramebuffers();
		//command buffer
		recordCommandBuffer();
	}

	/*
	* draw
	*/
	virtual void draw() override {
		uint32_t imageIndex = prepareFrame();

		updateUniformBuffer(currentFrame);

		//render
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT };
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &presentCompleteSemaphores[currentFrame];
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		size_t commandBufferIndex = imageIndex;
		submitInfo.pCommandBuffers = &commandBuffers[commandBufferIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderCompleteSemaphores[currentFrame];
		VK_CHECK_RESULT(vkQueueSubmit(devices.graphicsQueue, 1, &submitInfo, frameLimitFences[currentFrame]));

		submitFrame(imageIndex);
	}

	virtual void update() override {
		VulkanAppBase::update();
		Imgui* imgui = static_cast<Imgui*>(imguiBase);
		if (oldViewMatrix != cameraMatrices.view || imgui->frameReset) {
			rtPushConstants.frame = -1;
			oldViewMatrix = cameraMatrices.view;
			if (imgui->frameReset)
				imgui->frameReset = false;
		}
		rtPushConstants.frame++;

		buildCommandBuffer();
	}

	/*
	* create framebuffers
	*/
	virtual void createFramebuffers() override {
		for (auto& framebuffer : framebuffers) {
			vkDestroyFramebuffer(devices.device, framebuffer, nullptr);
		}
		framebuffers.resize(swapchain.imageCount);

		for (size_t i = 0; i < swapchain.imageCount; ++i) {
			std::array<VkImageView, 2> attachments = {
				swapchain.imageViews[i],
				depthImageView
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = postRenderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapchain.extent.width;
			framebufferInfo.height = swapchain.extent.height;
			framebufferInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(devices.device, &framebufferInfo, nullptr, &framebuffers[i]));
		}
	}

	/*
	* record command buffer
	*/
	virtual void recordCommandBuffer() override { }

	/*
	* record command buffer every frame
	*/
	void buildCommandBuffer() {
		VkCommandBufferBeginInfo cmdBufBeginInfo{};
		cmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.05f, 0.05f, 0.05f, 1.f };
		clearValues[1].depthStencil = { 1.f, 0 };

		Imgui* imgui = static_cast<Imgui*>(imguiBase);
		rtPushConstants.lightPos = imgui->userInput.lightPos;
		rtPushConstants.maxDepht = imgui->userInput.maxRayDepth;
		rtPushConstants.lightRadius = imgui->userInput.radius;
		rtPushConstants.lightIntensity = imgui->userInput.lightInternsity;
		rtPushConstants.rayPerPixel = imgui->userInput.rayPerPixel;
		rtPushConstants.shadow = imgui->userInput.shadow;

		//for #2 render pass
		VkRenderPassBeginInfo iamgeFileringRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		iamgeFileringRenderPassBeginInfo.clearValueCount = 1;
		iamgeFileringRenderPassBeginInfo.pClearValues = clearValues.data();
		iamgeFileringRenderPassBeginInfo.renderPass = imageFilteringRenderPass;
		iamgeFileringRenderPassBeginInfo.renderArea = { {0, 0},swapchain.extent };
		glm::vec4 imageFilteringPushConstant{ 1, 0.125, 0.125, 0.125 };

		//for #3 render pass
		VkRenderPassBeginInfo postRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		postRenderPassBeginInfo.clearValueCount = 2;
		postRenderPassBeginInfo.pClearValues = clearValues.data();
		postRenderPassBeginInfo.renderPass = postRenderPass;
		postRenderPassBeginInfo.renderArea = { {0, 0},swapchain.extent };

		const int imageFilteringIteration = 5;
		for (size_t i = 0; i < framebuffers.size(); ++i) {
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffers[i], &cmdBufBeginInfo));

			//#1 raytracing
			raytrace(commandBuffers[i], currentFrame);

			//#2 image filtering
			for (int currenrtFilter = 1; currenrtFilter <= imageFilteringIteration; ++currenrtFilter) {
				size_t pingpongFramebufferIndex = currentFrame * MAX_FRAMES_IN_FLIGHT + (currenrtFilter + 1) % 2;
				iamgeFileringRenderPassBeginInfo.framebuffer = pingpongFramebuffer[pingpongFramebufferIndex].framebuffer;
				vkCmdBeginRenderPass(commandBuffers[i], &iamgeFileringRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				vktools::setViewportScissorDynamicStates(commandBuffers[i], swapchain.extent);

				imageFilteringPushConstant.x = static_cast<float>(currenrtFilter - 1);
				imageFilteringPushConstant.y = imgui->userInput.scale.x;
				imageFilteringPushConstant.z = imgui->userInput.scale.y;
				imageFilteringPushConstant.w = imgui->userInput.scale.z;

				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, imageFilteringPipeline);
				vkCmdPushConstants(commandBuffers[i], imageFilteringPipelineLayout,
					VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &imageFilteringPushConstant);

				size_t descriptorIndex = currentFrame * MAX_FRAMES_IN_FLIGHT;
				if (currenrtFilter != 1) {//2, 3, 4, 5
					if (currenrtFilter % 2 == 0) {//2, 4
						descriptorIndex = currentFrame * MAX_FRAMES_IN_FLIGHT + 1;
					}
					else {//3, 5
						descriptorIndex = currentFrame * MAX_FRAMES_IN_FLIGHT + 2;
					}
				}

				vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, imageFilteringPipelineLayout, 0, 1,
					&imageFilteringDescriptorSets[descriptorIndex], 0, nullptr);
				vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); //full screen triangle
				vkCmdEndRenderPass(commandBuffers[i]);
			}

			//#3 full screen quad
			size_t framebufferIndex = i % framebuffers.size();
			postRenderPassBeginInfo.framebuffer = framebuffers[framebufferIndex];
			vkCmdBeginRenderPass(commandBuffers[i], &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vktools::setViewportScissorDynamicStates(commandBuffers[i], swapchain.extent);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postPipeline);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postPipelineLayout, 0, 1,
				&postDescriptorSets[currentFrame], 0, nullptr);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); //full screen triangle

			imguiBase->drawFrame(commandBuffers[i], currentFrame);

			vkCmdEndRenderPass(commandBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffers[i]));
		}
	}

	/*
	* resize
	*/
	virtual void resizeWindow(bool /*recordCmdBuf*/) override {
		VulkanAppBase::resizeWindow(false);
		createRaytraceOutputImages();
		createImageFilteringResources(true);
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updateRtDescriptorSet(i);
			updatePostDescriptorSet(i);
			updateImageFilteringDescriptorSet(i);
		}
		recordCommandBuffer();
	}

private:
	/** swapchain framebuffers */
	std::vector<VkFramebuffer> framebuffers;
	/** rt properties */
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
	};
	/** acceleration properties */
	VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
	};
	/** bottom-level acceleration structures */
	std::vector<AccelKHR> blasHandles;
	/** top-level acceleration structure */
	AccelKHR tlas{};
	/** instance buffer for tlas */
	VkBuffer instanceBuffer = VK_NULL_HANDLE;
	/** uniform buffers for camera matrices */
	std::vector<VkBuffer> uniformBuffers;
	/** uniform buffer memories */
	std::vector<MemoryAllocator::HostVisibleMemory> uniformBufferMemories;
	/** gltf model */
	VulkanGLTF gltfDioramaModel;
	/** view matrix of previous frame */
	glm::mat4 oldViewMatrix;

	/*
	* object instances - scene descriptor
	*/
	struct ObjInstance {
		glm::mat4 transform;
		glm::mat4 transformIT;
		uint64_t vertexAddress;
		uint64_t IndexAddress;
		uint64_t normalAddress;
		uint64_t uvAddress;
		uint64_t colorAddress;
		uint64_t tangentAddress;
		uint64_t materialIndicesAddress;
		uint64_t materialAddress;
		uint64_t primitiveAddress;
		uint64_t padding;
	};
	/** vector of obj instances */
	std::vector<ObjInstance> objInstances;
	/** obj instances buffer */
	VkBuffer sceneBuffer = VK_NULL_HANDLE;


	/*
	* shared descriptors for rasterizaer & raytracer
	* 1 uniform buffer for camera matrices
	* 1 storage buffer for scene descriptions (obj info)
	*/
	/** descriptor set layout bindings */
	DescriptorSetBindings descriptorSetBindings;
	/** descriptor set layout */
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	/** decriptor pool */
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	/** descriptor sets */
	std::vector<VkDescriptorSet> descriptorSets;
	

	/*
	* ray trace pipeline
	*/
	/** ray trace shader groups */
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> rtShaderGroups;
	/** ray trace pipeline layout */
	VkPipelineLayout rtPipelineLayout = VK_NULL_HANDLE;
	/** ray trace pipeline */
	VkPipeline rtPipeline = VK_NULL_HANDLE;
	/** shader binding table buffer */
	VkBuffer rtSBTBuffer = VK_NULL_HANDLE;
	/* push constancts for rt pipeline*/
	struct RtPushConstant {
		glm::vec4 clearColor = { 0.05f, 0.05f, 0.05f, 1.f };
		glm::vec3 lightPos;
		float lightRadius = 1.f;
		float lightIntensity = 1;
		int64_t frame = -1;
		int maxDepht = 10;
		int rayPerPixel = 1;
		int shadow = true;
	} rtPushConstants;
	/** raytracing output image - rt color */
	Texture2D rtOutputColor;
	/** raytracing output image - position */
	Texture2D rtOutputPos;
	/** raytracing output image - normal */
	Texture2D rtOutputNormal;
	/** raytracing output image - albedo */
	Texture2D rtOutputAlbedo;
	/*
	* descriptors for raytracer
	* 1 acceleration structure - tlas
	* 1 storage image - raytracing output image
	*/
	/** descriptor set layout bindings */
	DescriptorSetBindings rtDescriptorSetBindings;
	/** descriptor set layout */
	VkDescriptorSetLayout rtDescriptorSetLayout = VK_NULL_HANDLE;
	/** decriptor pool */
	VkDescriptorPool rtDescriptorPool = VK_NULL_HANDLE;
	/** descriptor sets */
	std::vector<VkDescriptorSet> rtDescriptorSets;


	/*
	* a-trous wavelet transform filtering
	*/
	/** 5 iteration - 2 sets framebuffer */
	std::vector<Framebuffer> pingpongFramebuffer;
	/** full quad pipeline */
	VkPipeline imageFilteringPipeline = VK_NULL_HANDLE;
	/** push constant + image descriptors */
	VkPipelineLayout imageFilteringPipelineLayout = VK_NULL_HANDLE;
	/** 1 color attachment */
	VkRenderPass imageFilteringRenderPass = VK_NULL_HANDLE;
	/** descriptor set layout bindings */
	DescriptorSetBindings imageFilteringDescriptorSetBindings;
	/** descriptor set layout */
	VkDescriptorSetLayout imageFilteringDescriptorSetLayout = VK_NULL_HANDLE;
	/** decriptor pool */
	VkDescriptorPool imageFilteringDescriptorPool = VK_NULL_HANDLE;
	/** descriptor sets */
	std::vector<VkDescriptorSet> imageFilteringDescriptorSets;

	/*
	* full-quad pipeline resources
	*/
	/** descriptor set layout bindings */
	DescriptorSetBindings postDescriptorSetBindings; //1 image sampler from (raytracing / rasterizer)
	/** descriptor set layout */
	VkDescriptorSetLayout postDescriptorSetLayout = VK_NULL_HANDLE;
	/** decriptor pool */
	VkDescriptorPool postDescriptorPool = VK_NULL_HANDLE;
	/** descriptor sets */
	std::vector<VkDescriptorSet> postDescriptorSets;
	/** full quad pipeline */
	VkPipeline postPipeline = VK_NULL_HANDLE;
	/** full quad pipeline layout */
	VkPipelineLayout postPipelineLayout = VK_NULL_HANDLE;
	/** normal render pass */
	VkRenderPass postRenderPass = VK_NULL_HANDLE;


	/** @brief build buffer - used to create vertex / index buffer */
	/*
	* build buffer - used to create vertex / index buffer
	*
	* @param bufferData - data to transfer
	* @param bufferSize
	* @param usage - buffer usage (vertex / index bit)
	* @param buffer - buffer handle
	* @param bufferMemory - buffer memory handle
	*/
	void createBuffer(const void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags usage,
		VkBuffer& buffer) {
		//create staging buffer
		VkBuffer stagingBuffer;
		VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
			bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

		//suballocate
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices.memoryAllocator.allocateBufferMemory(
			stagingBuffer, properties);

		hostVisibleMemory.mapData(devices.device, bufferData);

		//create vertex or index buffer
		VkBufferCreateInfo bufferCreateInfo = vktools::initializers::bufferCreateInfo(
			bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &bufferCreateInfo, nullptr, &buffer));

		//suballocation
		devices.memoryAllocator.allocateBufferMemory(buffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		//host visible -> device local
		devices.copyBuffer(devices.commandPool, stagingBuffer, buffer, bufferSize);

		devices.memoryAllocator.freeBufferMemory(stagingBuffer, properties);
		vkDestroyBuffer(devices.device, stagingBuffer, nullptr);
	}

	/*
	* create bottom-level acceleration
	*/
	void createBottomLevelAccelerationStructure() {
		//load bunny mesh
		VkBufferUsageFlags rtFlags = 
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
		gltfDioramaModel.loadScene(&devices, "../../meshes/pica_pica_mini_diorama/scene.gltf", rtFlags);

		std::vector<BlasGeometries> allBlas{}; //array of blas
		allBlas.reserve(gltfDioramaModel.primitives.size());
		for (VulkanGLTF::Primitive& primitive : gltfDioramaModel.primitives) {
			BlasGeometries blas;
			blas.push_back(getVkGeometryKHR(primitive,
				vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.vertexBuffer),
				vktools::getBufferDeviceAddress(devices.device, gltfDioramaModel.indexBuffer),
				gltfDioramaModel.vertexSize)
			);
			allBlas.push_back(blas);
		}
		
		buildBlas(&devices, allBlas,
			VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR, // allow compaction
			blasHandles);
	}

	/*
	* create top-level acceleration structure
	*/
	void createTopLevelAccelerationStructure() {
		std::vector<VkAccelerationStructureInstanceKHR> instances{};
		for(VulkanGLTF::Node& node : gltfDioramaModel.nodes){
			VkAccelerationStructureInstanceKHR instance;
			instance.transform = vktools::toTransformMatrixKHR(node.matrix);
			instance.instanceCustomIndex = node.primitiveIndex;
			instance.accelerationStructureReference = getBlasDeviceAddress(devices.device, blasHandles[node.primitiveIndex].accel);
			instance.instanceShaderBindingTableRecordOffset = 0; // we will use the same hit group for all object
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.mask = 0xFF;
			instances.push_back(instance);
		}
		buildTlas(&devices, instances, tlas, instanceBuffer, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
	}

	/*
	* create raytrace output images (rt 1 pass, pos, normal)
	*/
	void createRaytraceOutputImages() {
		rtOutputColor.cleanup();
		rtOutputPos.cleanup();
		rtOutputNormal.cleanup();
		rtOutputAlbedo.cleanup();

		//output #1 - rt color image
		rtOutputColor.createEmptyTexture(&devices,
			swapchain.extent,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);
		//output #2 - position (gbuffer)
		rtOutputPos.createEmptyTexture(&devices,
			swapchain.extent,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);
		//output #3 - normal (gbuffer)
		rtOutputNormal.createEmptyTexture(&devices,
			swapchain.extent,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);
		//output #4 - albedo (gbuffer)
		rtOutputAlbedo.createEmptyTexture(&devices,
			swapchain.extent,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);
	}

	/*
	* create image filtering resources
	*/
	void createImageFilteringResources(bool createFramebufferOnly = false) {
		for (auto& framebuffer : pingpongFramebuffer) {
			framebuffer.cleanup();
		}
		pingpongFramebuffer.resize(MAX_FRAMES_IN_FLIGHT * 2);

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT * 2; ++i) {
			pingpongFramebuffer[i].init(&devices);

			//1 color output
			VkImageCreateInfo imageCreateInfo =
				vktools::initializers::imageCreateInfo({ swapchain.extent.width, swapchain.extent.height, 1 },
					VK_FORMAT_R32G32B32A32_SFLOAT,
					VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
					1
			);
			pingpongFramebuffer[i].addAttachment(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				VK_IMAGE_LAYOUT_GENERAL, true);
		}

		if (createFramebufferOnly == false) {
			//renderpass
			std::vector<VkSubpassDependency> dependencies{};
			dependencies.resize(2);
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			imageFilteringRenderPass = pingpongFramebuffer[0].createRenderPass(dependencies);
		}

		//create framebuffer
		for (auto& framebuffer : pingpongFramebuffer) {
			framebuffer.createFramebuffer(swapchain.extent, imageFilteringRenderPass);
		}
	}

	/*
	* create image filtering pipeline
	*/
	void createImageFilteringPipeline() {
		PipelineGenerator gen(devices.device);
		gen.setRasterizerInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT);
		gen.addDescriptorSetLayout({ imageFilteringDescriptorSetLayout });
		gen.addPushConstantRange({ VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4)} });
		gen.addShader(vktools::createShaderModule(devices.device, vktools::readFile("shaders/full_quad_vert.spv")),
			VK_SHADER_STAGE_VERTEX_BIT);
		gen.addShader(vktools::createShaderModule(devices.device, vktools::readFile("shaders/a_trous_filter_frag.spv")),
			VK_SHADER_STAGE_FRAGMENT_BIT);
		gen.generate(imageFilteringRenderPass, &imageFilteringPipeline, &imageFilteringPipelineLayout);
	}

	/*
	* create image filtering descriptor set 
	*/
	void createImageFilteringDescriptorSet() {
		imageFilteringDescriptorSetBindings.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT); //rt color
		imageFilteringDescriptorSetBindings.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT); //position
		imageFilteringDescriptorSetBindings.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT); //normal
		imageFilteringDescriptorSetLayout = imageFilteringDescriptorSetBindings.createDescriptorSetLayout(devices.device);
		uint32_t nbDescriptorSets = MAX_FRAMES_IN_FLIGHT * 3;
		imageFilteringDescriptorPool = imageFilteringDescriptorSetBindings.createDescriptorPool(devices.device, nbDescriptorSets);
		imageFilteringDescriptorSets = vktools::allocateDescriptorSets(devices.device, imageFilteringDescriptorSetLayout, imageFilteringDescriptorPool,
			nbDescriptorSets);
	}

	/*
	* update image filtering descriptor set
	*/
	void updateImageFilteringDescriptorSet(size_t currentFrame) {
		std::vector<VkWriteDescriptorSet> writes;
		size_t descriptorIndex = currentFrame * MAX_FRAMES_IN_FLIGHT;
		//raytrace -> first pingpong framebuffer
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex], 0, &rtOutputColor.descriptor));
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex], 1, &rtOutputPos.descriptor));
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex], 2, &rtOutputNormal.descriptor));
		//first pingpong framebuffer -> second pingpong framebuffer
		VkDescriptorImageInfo firstPingpongFramebufferImageInfo{
			rtOutputColor.descriptor.sampler,
			pingpongFramebuffer[MAX_FRAMES_IN_FLIGHT * currentFrame].attachments[0].imageView,
			VK_IMAGE_LAYOUT_GENERAL
		};
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex +1], 0, &firstPingpongFramebufferImageInfo));
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex +1], 1, &rtOutputPos.descriptor));
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex +1], 2, &rtOutputNormal.descriptor));
		//second pingpong framebuffer -> first pingpong framebuffer
		VkDescriptorImageInfo secondPingpongFramebufferImageInfo{
			rtOutputColor.descriptor.sampler,
			pingpongFramebuffer[MAX_FRAMES_IN_FLIGHT * currentFrame + 1].attachments[0].imageView,
			VK_IMAGE_LAYOUT_GENERAL
		};
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex +2], 0, &secondPingpongFramebufferImageInfo));
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex +2], 1, &rtOutputPos.descriptor));
		writes.emplace_back(imageFilteringDescriptorSetBindings.makeWrite(imageFilteringDescriptorSets[descriptorIndex +2], 2, &rtOutputNormal.descriptor));
		vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	/*
	* create (normal) descriptor set - set camera matrix uniform buffer & scene description buffer
	*/
	void createDescriptorSet() {
		//camera matrix
		descriptorSetBindings.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		//scene description
		descriptorSetBindings.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		descriptorSetBindings.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(gltfDioramaModel.images.size()),
			 VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

		uint32_t nbDescriptorSet = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		descriptorSetLayout = descriptorSetBindings.createDescriptorSetLayout(devices.device);
		descriptorPool = descriptorSetBindings.createDescriptorPool(devices.device, nbDescriptorSet);
		descriptorSets = vktools::allocateDescriptorSets(devices.device, descriptorSetLayout, descriptorPool, nbDescriptorSet);

		std::vector<VkDescriptorImageInfo> imageInfos{};
		for (auto& image : gltfDioramaModel.images) {
			imageInfos.emplace_back(image.descriptor);
		}

		for (size_t i = 0; i < static_cast<size_t>(MAX_FRAMES_IN_FLIGHT); ++i) {
			VkDescriptorBufferInfo camMatricesInfo{ uniformBuffers[i], 0, sizeof(CameraMatrices) };
			VkDescriptorBufferInfo sceneBufferInfo{ sceneBuffer, 0, objInstances.size() * sizeof(ObjInstance) };

			std::vector<VkWriteDescriptorSet> writes;
			writes.emplace_back(descriptorSetBindings.makeWrite(descriptorSets[i], 0, &camMatricesInfo));
			writes.emplace_back(descriptorSetBindings.makeWrite(descriptorSets[i], 1, &sceneBufferInfo));
			writes.emplace_back(descriptorSetBindings.makeWriteArray(descriptorSets[i], 2, imageInfos.data()));
			vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		}
	}

	/*
	* create raytrace descriptor set - acceleration structure & raytracing pipeline destination image
	*/
	void createRtDescriptorSet() {
		//top level acceleration structure
		rtDescriptorSetBindings.addBinding(0,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
		//rt output color image
		rtDescriptorSetBindings.addBinding(1,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		//rt output position 
		rtDescriptorSetBindings.addBinding(2,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		//rt output normal
		rtDescriptorSetBindings.addBinding(3,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		//rt output albedo
		rtDescriptorSetBindings.addBinding(4,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		//primitive info
		rtDescriptorSetBindings.addBinding(5,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			1,
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

		//create rt descriptor pool & layout
		uint32_t nbRtDescriptorSet = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		rtDescriptorPool = rtDescriptorSetBindings.createDescriptorPool(devices.device, nbRtDescriptorSet);
		rtDescriptorSetLayout = rtDescriptorSetBindings.createDescriptorSetLayout(devices.device);
		rtDescriptorSets = vktools::allocateDescriptorSets(devices.device, rtDescriptorSetLayout, rtDescriptorPool, nbRtDescriptorSet);

		VkDescriptorBufferInfo primitiveInfo{ gltfDioramaModel.primitiveBuffer, 0, VK_WHOLE_SIZE };

		//update raytrace descriptor sets
		for (uint32_t i = 0; i < nbRtDescriptorSet; ++i) {
			VkWriteDescriptorSetAccelerationStructureKHR descAsInfo{};
			descAsInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			descAsInfo.accelerationStructureCount = 1;
			descAsInfo.pAccelerationStructures = &tlas.accel;

			std::vector<VkWriteDescriptorSet> writes;
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 0, &descAsInfo));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 1, &rtOutputColor.descriptor));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 2, &rtOutputPos.descriptor));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 3, &rtOutputNormal.descriptor));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 4, &rtOutputAlbedo.descriptor));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 5, &primitiveInfo));
			vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		}
	}

	/*
	* update raytrace descriptor set - set image where raytrac pipeline output is stored
	* this should be called in windowResized()
	*
	* @param currentFrame - index of raytrace descriptor set (0 <= currentFrame < MAX_FRAMES_IN_FLIGHT)
	*/
	void updateRtDescriptorSet(size_t currentFrame) {
		std::vector<VkWriteDescriptorSet> writes;
		writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[currentFrame], 1, &rtOutputColor.descriptor));
		writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[currentFrame], 2, &rtOutputPos.descriptor));
		writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[currentFrame], 3, &rtOutputNormal.descriptor));
		writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[currentFrame], 4, &rtOutputAlbedo.descriptor));
		vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	/*
	* create raytrace pipeline
	*/
	void createRtPipeline() {
		enum StageIndices {
			STAGE_RAYGEN,
			STAGE_MISS,
			STAGE_SHADOW_MISS,
			STAGE_CLOSEST_HIT,
			SHADER_GROUP_COUNT
		};

		std::array<VkPipelineShaderStageCreateInfo, SHADER_GROUP_COUNT> stages{};
		VkPipelineShaderStageCreateInfo stage{};
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.pName = "main";
		//raygen
		stage.module = vktools::createShaderModule(devices.device, vktools::readFile("shaders/pathtrace_rgen.spv"));
		stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		stages[STAGE_RAYGEN] = stage;
		//miss
		stage.module = vktools::createShaderModule(devices.device, vktools::readFile("shaders/pathtrace_rmiss.spv"));
		stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		stages[STAGE_MISS] = stage;
		//shadow miss
		stage.module = vktools::createShaderModule(devices.device, vktools::readFile("shaders/pathtrace_shadow_rmiss.spv"));
		stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		stages[STAGE_SHADOW_MISS] = stage;
		//closest hit
		stage.module = vktools::createShaderModule(devices.device, vktools::readFile("shaders/pathtrace_rchit.spv"));
		stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		stages[STAGE_CLOSEST_HIT] = stage;

		//shader groups
		VkRayTracingShaderGroupCreateInfoKHR group{};
		group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		group.anyHitShader = VK_SHADER_UNUSED_KHR;
		group.closestHitShader = VK_SHADER_UNUSED_KHR;
		group.generalShader = VK_SHADER_UNUSED_KHR;
		group.intersectionShader = VK_SHADER_UNUSED_KHR;

		//raygen
		group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		group.generalShader = STAGE_RAYGEN;
		rtShaderGroups.push_back(group);

		//miss
		group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		group.generalShader = STAGE_MISS;
		rtShaderGroups.push_back(group);

		//shadow miss
		group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		group.generalShader = STAGE_SHADOW_MISS;
		rtShaderGroups.push_back(group);

		//closest hit
		group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		group.generalShader = VK_SHADER_UNUSED_KHR;
		group.closestHitShader = STAGE_CLOSEST_HIT;
		rtShaderGroups.push_back(group);

		//push constant
		VkPushConstantRange pushConstant{};
		pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
			VK_SHADER_STAGE_MISS_BIT_KHR;
		pushConstant.offset = 0;
		pushConstant.size = sizeof(RtPushConstant);

		//pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

		//descriptor sets: one specific to ray tracing, and one shadred with the rasterization pipeline
		std::vector<VkDescriptorSetLayout> rtDescSetLayouts = { rtDescriptorSetLayout, descriptorSetLayout };
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayouts.size());
		pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayouts.data();

		VK_CHECK_RESULT(vkCreatePipelineLayout(devices.device, &pipelineLayoutCreateInfo, nullptr, &rtPipelineLayout));

		//assemble the shader stages and recursion depth info into the ray tracing pipeline
		VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{};
		rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size()); //shaders
		rayPipelineInfo.pStages = stages.data();
		// rtShaderGroups.size() == 3, 1 raygen group, 1 miss shader group, 1 hit group
		rayPipelineInfo.groupCount = static_cast<uint32_t>(rtShaderGroups.size());
		rayPipelineInfo.pGroups = rtShaderGroups.data();

		rayPipelineInfo.maxPipelineRayRecursionDepth = 2; //ray depth
		rayPipelineInfo.layout = rtPipelineLayout;

		VK_CHECK_RESULT(vkfp::vkCreateRayTracingPipelinesKHR(devices.device, {}, {}, 1, &rayPipelineInfo, nullptr, &rtPipeline));

		for (auto& s : stages) {
			vkDestroyShaderModule(devices.device, s.module, nullptr);
		}
	}

	/*
	* shader binding table (SBT)
	*
	* gets all shader handles and write them in a SBT buffer
	*/
	void createRtShaderBindingTable() {
		uint32_t groupCount = static_cast<uint32_t>(rtShaderGroups.size()); // 4 shaders: raygen, miss, shadow_miss, chit
		uint32_t groupHandleSize = rtProperties.shaderGroupHandleSize;
		//compute the actual size needed per SBT entry (round up to alignment needed)
		uint32_t groupSizeAligned = alignUp(groupHandleSize, static_cast<size_t>(rtProperties.shaderGroupBaseAlignment));
		//bytes needed for the SBT
		uint32_t sbtSize = groupCount * groupSizeAligned;

		//fetch all the shader handles used in the pipeline
		std::vector<uint8_t> shaderHandleStorage(sbtSize);
		VK_CHECK_RESULT(vkfp::vkGetRayTracingShaderGroupHandlesKHR(devices.device, rtPipeline, 0, groupCount,
			sbtSize, shaderHandleStorage.data()));

		//allocate a buffer for storing the SBT
		VkBufferCreateInfo bufferInfo = vktools::initializers::bufferCreateInfo(sbtSize,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &bufferInfo, nullptr, &rtSBTBuffer));

		//allocate memory
		MemoryAllocator::HostVisibleMemory memory = devices.memoryAllocator.allocateBufferMemory(rtSBTBuffer,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		//map data
		uint8_t* pData = reinterpret_cast<uint8_t*>(memory.getHandle(devices.device));
		for (uint32_t g = 0; g < groupCount; ++g) {
			memcpy(pData, shaderHandleStorage.data() + g * groupHandleSize, groupHandleSize);
			pData += groupSizeAligned;
		}
		memory.unmap(devices.device);
	}

	/*
	* full quad pipeline - used for #2 render pass
	*/
	void createPostPipeline() {
		//fixed functions
		PipelineGenerator gen(devices.device);
		gen.setRasterizerInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT);
		gen.addDescriptorSetLayout({ postDescriptorSetLayout });
		gen.addShader(vktools::createShaderModule(devices.device, vktools::readFile("shaders/full_quad_vert.spv")),
			VK_SHADER_STAGE_VERTEX_BIT);
		gen.addShader(vktools::createShaderModule(devices.device, vktools::readFile("shaders/full_quad_frag.spv")),
			VK_SHADER_STAGE_FRAGMENT_BIT);
		gen.generate(postRenderPass, &postPipeline, &postPipelineLayout);
	}

	/*
	* create post descriptor set - one image sampler for ray tracing pipeline output
	*/
	void createPostDescriptorSet() {
		postDescriptorSetBindings.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1, VK_SHADER_STAGE_FRAGMENT_BIT);
		postDescriptorSetBindings.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1, VK_SHADER_STAGE_FRAGMENT_BIT);

		uint32_t nbDescriptorSet = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		postDescriptorSetLayout = postDescriptorSetBindings.createDescriptorSetLayout(devices.device);
		postDescriptorPool = postDescriptorSetBindings.createDescriptorPool(devices.device, nbDescriptorSet);
		postDescriptorSets = vktools::allocateDescriptorSets(devices.device, postDescriptorSetLayout,
			postDescriptorPool, nbDescriptorSet);
	}

	/*
	* update post descriptor set - sampler references offscreen color buffer
	*/
	void updatePostDescriptorSet(size_t currentFrame) {
		VkDescriptorImageInfo imageInfo{ 
			rtOutputColor.descriptor.sampler,
			pingpongFramebuffer[currentFrame * MAX_FRAMES_IN_FLIGHT].attachments[0].imageView,
			VK_IMAGE_LAYOUT_GENERAL
		};
		std::vector<VkWriteDescriptorSet> writes{};
		writes.emplace_back(postDescriptorSetBindings.makeWrite(postDescriptorSets[currentFrame], 0, &imageInfo));
		writes.emplace_back(postDescriptorSetBindings.makeWrite(postDescriptorSets[currentFrame], 1, &rtOutputAlbedo.descriptor));
		vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	/*
	* create uniform buffers - camera matrices
	*/
	void createUniformbuffer() {
		VkDeviceSize size = sizeof(CameraMatrices);
		uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		uniformBufferMemories.resize(MAX_FRAMES_IN_FLIGHT);

		VkBufferCreateInfo info = vktools::initializers::bufferCreateInfo(size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			VK_CHECK_RESULT(vkCreateBuffer(devices.device, &info, nullptr, &uniformBuffers[i]));
			uniformBufferMemories[i] = devices.memoryAllocator.allocateBufferMemory(uniformBuffers[i],
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
	}

	/*
	* update uniform buffer - camera matrices
	*
	* @param currentFrame - index of uniform buffer (0 <= currentFrame < MAX_FRAMES_IN_FLIGHT)
	*/
	void updateUniformBuffer(size_t currentFrame) {
		uniformBufferMemories[currentFrame].mapData(devices.device, &cameraMatrices);
	}

	/*
	* record raytrace commands
	*
	* @param cmdBuf
	* @param descriptorSetIndex
	*/
	void raytrace(VkCommandBuffer cmdBuf, size_t descriptorSetIndex) {
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);

		std::vector<VkDescriptorSet> descSets{ rtDescriptorSets[descriptorSetIndex], descriptorSets[descriptorSetIndex] };
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout,
			0, static_cast<uint32_t>(descSets.size()), descSets.data(), 0, nullptr);
		vkCmdPushConstants(cmdBuf, rtPipelineLayout,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
			0, sizeof(RtPushConstant), &rtPushConstants);

		//size of a program identifier
		uint32_t groupSize = alignUp(rtProperties.shaderGroupHandleSize, rtProperties.shaderGroupBaseAlignment);
		uint32_t groupStride = groupSize;

		VkDeviceAddress sbtAddress = vktools::getBufferDeviceAddress(devices.device, rtSBTBuffer);

		using Stride = VkStridedDeviceAddressRegionKHR;
		std::array<Stride, 4> strideAddress{
			Stride{sbtAddress + 0u * groupSize, groupStride, groupSize * 1},
			Stride{sbtAddress + 1u * groupSize, groupStride, groupSize * 2},
			Stride{sbtAddress + 3u * groupSize, groupStride, groupSize * 1},
			Stride{0u, 0u, 0u}
		};

		vkfp::vkCmdTraceRaysKHR(cmdBuf, &strideAddress[0], &strideAddress[1],
			&strideAddress[2], &strideAddress[3], swapchain.extent.width, swapchain.extent.height, 1);
	}
};

//entry point
RUN_APPLICATION_MAIN(VulkanApp, 1920, 1080, "ray tracing");
