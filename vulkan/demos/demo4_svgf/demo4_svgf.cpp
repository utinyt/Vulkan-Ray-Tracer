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
#include "core/vulkan_debug.h"

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

		static float radius = 6, lightIntensity = 100;
		static int depth = 5, rayPerPixel = 1;

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
		createDefaultDepthBuffer = false;
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
		vkDestroyPipeline(devices.device, gbufferPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, gbufferPipelineLayout, nullptr);
		vkDestroyPipeline(devices.device, rtPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, rtPipelineLayout, nullptr);
		vkDestroyPipeline(devices.device, postPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, postPipelineLayout, nullptr);
		vkDestroyRenderPass(devices.device, postRenderPass, nullptr);
		vkDestroyPipeline(devices.device, reprojectionComputePipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, reprojectionComputePipelineLayout, nullptr);
		vkDestroyPipeline(devices.device, updateHistoryComputePipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, updateHistoryComputePipelineLayout, nullptr);

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

		//raytrace destination image
		devices.memoryAllocator.freeImageMemory(rtDestinationImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, rtDestinationImage, nullptr);
		vkDestroyImageView(devices.device, rtDestinationImageView, nullptr);

		//history images & integrated images
		devices.memoryAllocator.freeImageMemory(historyLengthImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, historyLengthImage, nullptr);
		vkDestroyImageView(devices.device, historyLengthImageView, nullptr);
		devices.memoryAllocator.freeImageMemory(updatedHistoryLengthImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, updatedHistoryLengthImage, nullptr);
		vkDestroyImageView(devices.device, updatedHistoryLengthImageView, nullptr);
		devices.memoryAllocator.freeImageMemory(momentHistoryImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, momentHistoryImage, nullptr);
		vkDestroyImageView(devices.device, momentHistoryImageView, nullptr);
		devices.memoryAllocator.freeImageMemory(colorHistoryImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, colorHistoryImage, nullptr);
		vkDestroyImageView(devices.device, colorHistoryImageView, nullptr);
		devices.memoryAllocator.freeImageMemory(integratedColorImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, integratedColorImage, nullptr);
		vkDestroyImageView(devices.device, integratedColorImageView, nullptr);
		devices.memoryAllocator.freeImageMemory(integratedMomentsImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, integratedMomentsImage, nullptr);
		vkDestroyImageView(devices.device, integratedMomentsImageView, nullptr);
		devices.memoryAllocator.freeImageMemory(varianceImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, varianceImage, nullptr);
		vkDestroyImageView(devices.device, varianceImageView, nullptr);

		//swapchain framebuffers
		for (auto& framebuffer : framebuffers) {
			vkDestroyFramebuffer(devices.device, framebuffer, nullptr);
		}

		//gbuffer
		for (auto& gbuffer : gbuffers) {
			gbuffer.cleanup();
		}

		//sampler
		vkDestroySampler(devices.device, imageSampler, nullptr);

		//offscreen renderpass
		vkDestroyRenderPass(devices.device, gbufferRenderPass, nullptr);

		//descriptors
		vkDestroyDescriptorPool(devices.device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, rtDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, rtDescriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, postDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, postDescriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, reprojectionDescPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, reprojectionDescLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, updateHistoryDescPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, updateHistoryDescLayout, nullptr);

		//uniform buffers
		devices.memoryAllocator.freeBufferMemory(matricesUniformBuffer,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkDestroyBuffer(devices.device, matricesUniformBuffer, nullptr);

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
		updateUniformBuffer();

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
		* rasterizer - gbuffer
		*/
		createGBufferResources();
		createDescriptorSet();
		createGBufferPipeline();

		/*
		* history data & integrated images
		*/
		createStorageImage(historyLengthImage, historyLengthImageView, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT); //length
		createStorageImage(updatedHistoryLengthImage, updatedHistoryLengthImageView, VK_FORMAT_R32_SFLOAT); //length
		initHistoryLengthImage();
		createStorageImage(colorHistoryImage, colorHistoryImageView, VK_FORMAT_R32G32B32A32_SFLOAT); //color
		createStorageImage(momentHistoryImage, momentHistoryImageView, VK_FORMAT_R32G32_SFLOAT); //vec2(moment1, moment2)
		createStorageImage(integratedColorImage, integratedColorImageView, VK_FORMAT_R32G32B32A32_SFLOAT); //color
		createStorageImage(integratedMomentsImage, integratedMomentsImageView, VK_FORMAT_R32G32_SFLOAT); //vec2(moment1, moment2)
		createStorageImage(varianceImage, varianceImageView, VK_FORMAT_R32_SFLOAT); //vec2(moment1, moment2)
		imageLayoutTransition();

		/*
		* ray tracing
		*/
		createRaytraceDestinationImage();
		createRtDescriptorSet();
		updateRtDescriptorSet();
		createRtPipeline();
		createRtShaderBindingTable();

		/*
		* compute (reprojection / history update)
		*/
		createComputeDescSet();
		createComputePipeline();
		updateComputeDescSet();

		/*
		* full-quad
		*/
		//post descriptor set
		createPostDescriptorSet();
		updatePostDescriptorSet();

		//render pass
		postRenderPass = vktools::createRenderPass(devices.device,
			{ swapchain.surfaceFormat.format },
			VK_FORMAT_UNDEFINED,
			VK_SAMPLE_COUNT_1_BIT, 1, true, true,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		);
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

		updateUniformBuffer();

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

		currentGBufferIndex = (currentGBufferIndex + 1) % 2;
		oldViewMatrix = cameraMatrices.view;
	}

	virtual void update() override {
		VulkanAppBase::update();

		/*Imgui* imgui = static_cast<Imgui*>(imguiBase);
		if (oldViewMatrix != cameraMatrices.view || imgui->frameReset) {
			rtPushConstants.frame = -1;
			oldViewMatrix = cameraMatrices.view;
			if (imgui->frameReset)
				imgui->frameReset = false;
		}
		rtPushConstants.frame++;*/

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
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = postRenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = &swapchain.imageViews[i];
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

		const int clearValueCount = 5;
		std::array<VkClearValue, clearValueCount> clearValues{};
		clearValues[0].color = { 0.00f, 0.00f, 0.00f, 1.f }; //position
		clearValues[1].color = { 0.00f, 0.00f, 0.00f, -1.f }; //normal
		clearValues[2].color = { 0.00f, 0.00f, 0.00f, 1.f }; //albedo
		clearValues[3].depthStencil = { 1.f, 0 }; //depth

		Imgui* imgui = static_cast<Imgui*>(imguiBase);
		rtPushConstants.lightPos = imgui->userInput.lightPos;
		rtPushConstants.maxDepht = imgui->userInput.maxRayDepth;
		rtPushConstants.lightRadius = imgui->userInput.radius;
		rtPushConstants.lightIntensity = imgui->userInput.lightInternsity;
		rtPushConstants.rayPerPixel = imgui->userInput.rayPerPixel;
		rtPushConstants.shadow = imgui->userInput.shadow;

		//for rasterizer render pass
		VkRenderPassBeginInfo gbufferRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		gbufferRenderPassBeginInfo.clearValueCount = clearValueCount;
		gbufferRenderPassBeginInfo.pClearValues = clearValues.data();
		gbufferRenderPassBeginInfo.renderPass = gbufferRenderPass;
		gbufferRenderPassBeginInfo.renderArea = { {0, 0},swapchain.extent };

		//for #2 render pass
		VkRenderPassBeginInfo postRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		postRenderPassBeginInfo.clearValueCount = 1;
		postRenderPassBeginInfo.pClearValues = clearValues.data();
		postRenderPassBeginInfo.renderPass = postRenderPass;
		postRenderPassBeginInfo.renderArea = { {0, 0},swapchain.extent };

		cam.oldView = oldViewMatrix;
		cam.proj = cameraMatrices.proj;

		for (size_t i = 0; i < framebuffers.size(); ++i) {
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffers[i], &cmdBufBeginInfo));

			/*
			* #1 gbuffer pass
			*/
			vkdebug::marker::beginLabel(commandBuffers[i], "gbuffer pass");
			gbufferRenderPassBeginInfo.framebuffer = gbuffers[currentGBufferIndex].framebuffer;
			vkCmdBeginRenderPass(commandBuffers[i], &gbufferRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			rasterize(commandBuffers[i]);
			vkCmdEndRenderPass(commandBuffers[i]);
			vkdebug::marker::endLabel(commandBuffers[i]);

			/*
			* #2 raytracing
			*/
			vkdebug::marker::beginLabel(commandBuffers[i], "raytrace");
			raytrace(commandBuffers[i]);
			vkdebug::marker::endLabel(commandBuffers[i]);

			/*
			* #3 reprojection
			*/
			vkdebug::marker::beginLabel(commandBuffers[i], "reproject");
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, reprojectionComputePipeline);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, reprojectionComputePipelineLayout, 0, 1, &reprojectionDescSet, 0, nullptr);
			vkCmdPushConstants(commandBuffers[i], reprojectionComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::mat4) * 2, &cam);
			vkCmdDispatch(commandBuffers[i], swapchain.extent.width / 32, swapchain.extent.height / 32, 1);

			std::array<VkImageMemoryBarrier, 4> barriers{};
			barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[0].pNext = nullptr;
			barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[0].image = integratedColorImage;
			barriers[1] = barriers[0];
			barriers[1].image = integratedMomentsImage;
			barriers[2] = barriers[0];
			barriers[2].image = updatedHistoryLengthImage;
			barriers[3] = barriers[0];
			barriers[3].image = varianceImage;

			vkCmdPipelineBarrier(commandBuffers[i],
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				static_cast<uint32_t>(barriers.size()), barriers.data()
			);
			vkdebug::marker::endLabel(commandBuffers[i]);

			/*
			* #4 update history
			*/
			vkdebug::marker::beginLabel(commandBuffers[i], "update history");
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, updateHistoryComputePipeline);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, updateHistoryComputePipelineLayout, 0, 1, &updateHistoryDescSet, 0, nullptr);
			vkCmdDispatch(commandBuffers[i], swapchain.extent.width / 32, swapchain.extent.height / 32, 1);

			VkImageMemoryBarrier barrier = barriers[0];
			vkCmdPipelineBarrier(commandBuffers[i],
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
			vkdebug::marker::endLabel(commandBuffers[i]);

			/*
			* #5 full screen quad
			*/
			vkdebug::marker::beginLabel(commandBuffers[i], "full screen quad");
			size_t framebufferIndex = i % framebuffers.size();
			postRenderPassBeginInfo.framebuffer = framebuffers[framebufferIndex];
			vkCmdBeginRenderPass(commandBuffers[i], &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vktools::setViewportScissorDynamicStates(commandBuffers[i], swapchain.extent);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postPipeline);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postPipelineLayout, 0, 1,
				&postDescriptorSet, 0, nullptr);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); //full screen triangle

			imguiBase->drawFrame(commandBuffers[i], currentFrame);

			vkCmdEndRenderPass(commandBuffers[i]);
			vkdebug::marker::endLabel(commandBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffers[i]));
		}
	}

	/*
	* resize
	*/
	virtual void resizeWindow(bool /*recordCmdBuf*/) override {
		VulkanAppBase::resizeWindow(false);
		createGBufferResources(true);

		createStorageImage(historyLengthImage, historyLengthImageView, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT); //length
		createStorageImage(updatedHistoryLengthImage, updatedHistoryLengthImageView, VK_FORMAT_R32_SFLOAT); //length
		initHistoryLengthImage();
		createStorageImage(colorHistoryImage, colorHistoryImageView, VK_FORMAT_R32G32B32A32_SFLOAT); //color
		createStorageImage(momentHistoryImage, momentHistoryImageView, VK_FORMAT_R32G32_SFLOAT); //vec2(moment1, moment2)
		createStorageImage(integratedColorImage, integratedColorImageView, VK_FORMAT_R32G32B32A32_SFLOAT); //color
		createStorageImage(integratedMomentsImage, integratedMomentsImageView, VK_FORMAT_R32G32_SFLOAT); //vec2(moment1, moment2)
		createStorageImage(varianceImage, varianceImageView, VK_FORMAT_R32_SFLOAT); //vec2(moment1, moment2)
		imageLayoutTransition();

		createRaytraceDestinationImage();
		updateRtDescriptorSet();
		updateComputeDescSet();
		updatePostDescriptorSet();
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
	VkBuffer matricesUniformBuffer;
	/** uniform buffer memories */
	MemoryAllocator::HostVisibleMemory matricesUniformBufferMemory;
	/** gltf model */
	VulkanGLTF gltfDioramaModel;
	/** view matrix of previous frame */
	glm::mat4 oldViewMatrix = glm::mat4(1.f);

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
	VkDescriptorSet descriptorSet;


	/*
	* gbuffer creation resources
	*/
	std::array<Framebuffer, 2> gbuffers; //current & previous
	/** current gbuffer index */
	int currentGBufferIndex = 0;
	/** offscreen renderpass */
	VkRenderPass gbufferRenderPass = VK_NULL_HANDLE;
	/** image sampler */
	VkSampler imageSampler = VK_NULL_HANDLE;
	/** general pipeline */
	VkPipeline gbufferPipeline = VK_NULL_HANDLE;
	/** general pipeline layout */
	VkPipelineLayout gbufferPipelineLayout = VK_NULL_HANDLE;
	/* push constancts for rasterizer pipeline*/
	struct GBufferPushConstant {
		glm::mat4 modelMatrix{ 1.f };
		uint32_t materialId = 0;
		uint32_t primitiveId = 0;
	} gbufferPushConstants;
	
	/*
	* history data
	*/
	/** length -> integer  */
	VkImage historyLengthImage = VK_NULL_HANDLE;
	VkImageView historyLengthImageView = VK_NULL_HANDLE;
	VkImage updatedHistoryLengthImage = VK_NULL_HANDLE;
	VkImageView updatedHistoryLengthImageView = VK_NULL_HANDLE;
	/** color history */
	VkImage colorHistoryImage = VK_NULL_HANDLE;
	VkImageView colorHistoryImageView = VK_NULL_HANDLE;
	/** moment history */
	VkImage momentHistoryImage = VK_NULL_HANDLE;
	VkImageView momentHistoryImageView = VK_NULL_HANDLE;

	struct Cam {
		glm::mat4 oldView;
		glm::mat4 proj;
	} cam;

	/*
	* integrated data
	*/
	/** integrated color */
	VkImage integratedColorImage = VK_NULL_HANDLE;
	VkImageView integratedColorImageView = VK_NULL_HANDLE;
	/** integrated moments */
	VkImage integratedMomentsImage = VK_NULL_HANDLE;
	VkImageView integratedMomentsImageView = VK_NULL_HANDLE;
	/** variance */
	VkImage varianceImage = VK_NULL_HANDLE;
	VkImageView varianceImageView = VK_NULL_HANDLE;

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
	/** raytracing destination image */
	VkImage rtDestinationImage = VK_NULL_HANDLE;
	/** raytracing destination image view */
	VkImageView rtDestinationImageView = VK_NULL_HANDLE;

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
	VkDescriptorSet rtDescriptorSet;

	/*
	* reprojection (compute) resources
	*/
	/** compute pipeline */
	VkPipeline reprojectionComputePipeline = VK_NULL_HANDLE;
	VkPipelineLayout reprojectionComputePipelineLayout = VK_NULL_HANDLE;
	/** descriptor set */
	DescriptorSetBindings reprojectionDescBindings;
	VkDescriptorPool reprojectionDescPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout reprojectionDescLayout = VK_NULL_HANDLE;
	VkDescriptorSet reprojectionDescSet = VK_NULL_HANDLE;

	/*
	* update history (compute) resources
	*/
	/** compute pipeline */
	VkPipeline updateHistoryComputePipeline = VK_NULL_HANDLE;
	VkPipelineLayout updateHistoryComputePipelineLayout = VK_NULL_HANDLE;
	/** descriptor set */
	DescriptorSetBindings updateHistoryDescBindings;
	VkDescriptorPool updateHistoryDescPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout updateHistoryDescLayout = VK_NULL_HANDLE;
	VkDescriptorSet updateHistoryDescSet = VK_NULL_HANDLE;

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
	VkDescriptorSet postDescriptorSet;
	/** full quad pipeline */
	VkPipeline postPipeline = VK_NULL_HANDLE;
	/** full quad pipeline layout */
	VkPipelineLayout postPipelineLayout = VK_NULL_HANDLE;
	/** normal render pass */
	VkRenderPass postRenderPass = VK_NULL_HANDLE;


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
	* create offscreen images & render pass & framebuffers
	*/
	void createGBufferResources(bool createFramebufferOnly = false) {
		for (auto& gbuffer : gbuffers) {
			gbuffer.init(&devices);
			gbuffer.cleanup();

			VkImageCreateInfo imageCreateInfo =
				vktools::initializers::imageCreateInfo({ swapchain.extent.width, swapchain.extent.height, 1 },
					VK_FORMAT_R32G32B32A32_SFLOAT,
					VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
					1
				);

			//position.xyz
			gbuffer.addAttachment(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				VK_IMAGE_LAYOUT_GENERAL, true);

			//normal.xyz & prim id
			gbuffer.addAttachment(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				VK_IMAGE_LAYOUT_GENERAL, true);

			//albedo
			gbuffer.addAttachment(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				VK_IMAGE_LAYOUT_GENERAL, true);

			//depth attachment
			imageCreateInfo.format = depthFormat;
			imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			gbuffer.addAttachment(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}

		if (createFramebufferOnly == false) {
			//renderpass
			std::vector<VkSubpassDependency> dependencies{};
			dependencies.resize(2);
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT; //primary ray

			gbufferRenderPass = gbuffers[0].createRenderPass(dependencies);

			//image sampler
			VkSamplerCreateInfo samplerInfo =
				vktools::initializers::samplerCreateInfo(devices.availableFeatures, devices.properties);
			VK_CHECK_RESULT(vkCreateSampler(devices.device, &samplerInfo, nullptr, &imageSampler));
		}

		//create framebuffer
		for (auto& gbuffer : gbuffers) {
			gbuffer.createFramebuffer(swapchain.extent, gbufferRenderPass);
		}
	}

	/*
	* history image creation (history length / color / moment)
	*/
	void createStorageImage(VkImage& historyImage, VkImageView& historyImageView, VkFormat format, VkFlags usage = 0) {
		/** destroy */
		devices.memoryAllocator.freeImageMemory(historyImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, historyImage, nullptr);
		vkDestroyImageView(devices.device, historyImageView, nullptr);

		/** image */
		devices.createImage(historyImage,
			{ swapchain.extent.width, swapchain.extent.height, 1 },
			format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | usage,
			1,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		/** image view */
		historyImageView = vktools::createImageView(devices.device,
			historyImage,
			VK_IMAGE_VIEW_TYPE_2D,
			format,
			VK_IMAGE_ASPECT_COLOR_BIT,
			1
		);
	}



	/*
	* init history length image
	*/
	void initHistoryLengthImage() {
		/*
		* staging - create starging buffer
		*/
		const int imageSize = swapchain.extent.width * swapchain.extent.height * sizeof(float);
		VkBuffer stagingBuffer;
		VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
			imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices.memoryAllocator.allocateBufferMemory(
			stagingBuffer, properties);

		std::vector<uint32_t> data(imageSize);
		memset(data.data(), 0, imageSize);
		hostVisibleMemory.mapData(devices.device, data.data());

		/*
		* undefined -> transfer dst optimal
		*/
		VkCommandBuffer cmdBuf = devices.beginCommandBuffer();
		vktools::setImageLayout(cmdBuf,
			historyLengthImage,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1
		);

		/*
		* image copy from staging buffer to the iamge
		*/
		VkBufferImageCopy copy = vktools::initializers::bufferCopyRegion({ swapchain.extent.width , swapchain.extent.height, 1 });
		vkCmdCopyBufferToImage(cmdBuf,
			stagingBuffer,
			historyLengthImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copy
		);

		/*
		* transfer dst optimal -> general
		*/
		vktools::setImageLayout(cmdBuf,
			historyLengthImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,
			1
		);

		vktools::setImageLayout(cmdBuf,
			updatedHistoryLengthImage,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			1
		);

		devices.endCommandBuffer(cmdBuf);

		/*
		* cleanup
		*/
		devices.memoryAllocator.freeBufferMemory(stagingBuffer, properties);
		vkDestroyBuffer(devices.device, stagingBuffer, nullptr);
	}

	/*
	* image data layout transition (undefined -> general)
	*/
	void imageLayoutTransition() {
		VkCommandBuffer cmdBuf = devices.beginCommandBuffer();
		vktools::setImageLayout(cmdBuf, colorHistoryImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
		vktools::setImageLayout(cmdBuf, momentHistoryImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
		vktools::setImageLayout(cmdBuf, integratedColorImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
		vktools::setImageLayout(cmdBuf, integratedMomentsImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
		vktools::setImageLayout(cmdBuf, varianceImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 1);
		devices.endCommandBuffer(cmdBuf);
	}

	/*
	* create reprojection compute pipeline
	*/
	void createComputePipeline() {
		/*
		* reprojection pipeline
		*/
		std::vector<VkDescriptorSetLayout> layouts{ reprojectionDescLayout};
		std::vector<VkPushConstantRange> ranges{ {VK_SHADER_STAGE_COMPUTE_BIT, 0, static_cast<uint32_t>(sizeof(glm::mat4) * 2)} };
		VkPipelineLayoutCreateInfo reprojectionComputePipelineLayoutCreateInfo = vktools::initializers::pipelineLayoutCreateInfo(layouts, ranges);
		VK_CHECK_RESULT(vkCreatePipelineLayout(devices.device, &reprojectionComputePipelineLayoutCreateInfo, nullptr, &reprojectionComputePipelineLayout));

		VkComputePipelineCreateInfo reprojectionComputePipelineCreateInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		reprojectionComputePipelineCreateInfo.layout = reprojectionComputePipelineLayout;
		VkShaderModule reprojectionShaderModule = vktools::createShaderModule(devices.device, vktools::readFile("shaders/reprojection_comp.spv"));
		reprojectionComputePipelineCreateInfo.stage = vktools::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, reprojectionShaderModule);
		VK_CHECK_RESULT(vkCreateComputePipelines(devices.device, VK_NULL_HANDLE, 1, &reprojectionComputePipelineCreateInfo, nullptr, &reprojectionComputePipeline));

		vkDestroyShaderModule(devices.device, reprojectionShaderModule, nullptr);

		/*
		* history update pipeline
		*/
		VkPipelineLayoutCreateInfo  updateHistoryComputePipelineLayoutCreateInfo = vktools::initializers::pipelineLayoutCreateInfo(&updateHistoryDescLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(devices.device, &updateHistoryComputePipelineLayoutCreateInfo, nullptr, &updateHistoryComputePipelineLayout));

		VkComputePipelineCreateInfo updateHistoryComputePipelineCreateInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		updateHistoryComputePipelineCreateInfo.layout = updateHistoryComputePipelineLayout;
		VkShaderModule updateHistoryShaderModule = vktools::createShaderModule(devices.device, vktools::readFile("shaders/update_history_comp.spv"));
		updateHistoryComputePipelineCreateInfo.stage = vktools::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, updateHistoryShaderModule);
		VK_CHECK_RESULT(vkCreateComputePipelines(devices.device, VK_NULL_HANDLE, 1, &updateHistoryComputePipelineCreateInfo, nullptr, &updateHistoryComputePipeline));

		vkDestroyShaderModule(devices.device, updateHistoryShaderModule, nullptr);
	}

	/*
	* create reprojection descriptor set
	*/
	void createComputeDescSet() {
		/*
		* current frame data 
		*/
		/** raytraced image */
		reprojectionDescBindings.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** gbuffer - position */
		reprojectionDescBindings.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** gbuffer - normal & prim id */
		reprojectionDescBindings.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);

		/*
		* history data
		*/
		/** gbuffer - position */
		reprojectionDescBindings.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** gbuffer - normal & prim id*/
		reprojectionDescBindings.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** history length */
		reprojectionDescBindings.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** color history */
		reprojectionDescBindings.addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** moment history */
		reprojectionDescBindings.addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);

		/*
		* integrated images (output)
		*/
		/** integrated color */
		reprojectionDescBindings.addBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** integrated moments */
		reprojectionDescBindings.addBinding(9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** udpated history length destination */
		reprojectionDescBindings.addBinding(10, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** variance destination */
		reprojectionDescBindings.addBinding(11, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);

		/** create resources */
		reprojectionDescPool = reprojectionDescBindings.createDescriptorPool(devices.device);
		reprojectionDescLayout = reprojectionDescBindings.createDescriptorSetLayout(devices.device);
		reprojectionDescSet = vktools::allocateDescriptorSets(devices.device, reprojectionDescLayout, reprojectionDescPool, 1).front();

		/*
		* source
		*/
		/** integrated moments */
		updateHistoryDescBindings.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** updated history length */
		updateHistoryDescBindings.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);

		/*
		* destination
		*/
		/** moment history */
		updateHistoryDescBindings.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		/** history length */
		updateHistoryDescBindings.addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);

		updateHistoryDescBindings.addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
		updateHistoryDescBindings.addBinding(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);

		/** create resources */
		updateHistoryDescPool = updateHistoryDescBindings.createDescriptorPool(devices.device);
		updateHistoryDescLayout = updateHistoryDescBindings.createDescriptorSetLayout(devices.device);
		updateHistoryDescSet = vktools::allocateDescriptorSets(devices.device, updateHistoryDescLayout, updateHistoryDescPool, 1).front();
	}

	/*
	* update reprojection descriptor set
	*/
	void updateComputeDescSet() {
		std::vector<VkWriteDescriptorSet> writes;

		VkDescriptorImageInfo rtImageInfo{imageSampler, rtDestinationImageView, VK_IMAGE_LAYOUT_GENERAL};
		VkDescriptorImageInfo currentGBufferPositionInfo{imageSampler, gbuffers[currentGBufferIndex].attachments[0].imageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo currentGBufferNormalPrimIDInfo{imageSampler, gbuffers[currentGBufferIndex].attachments[1].imageView, VK_IMAGE_LAYOUT_GENERAL };

		uint32_t previousGBufferIndex = (currentGBufferIndex + 1) % 2;
		VkDescriptorImageInfo previousGBufferPositionInfo{ imageSampler, gbuffers[previousGBufferIndex].attachments[0].imageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo previousGBufferNormalPrimIDInfo{ imageSampler, gbuffers[previousGBufferIndex].attachments[1].imageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo historyLengthInfo{ imageSampler, historyLengthImageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo colorHistoryInfo{ imageSampler, colorHistoryImageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo momentHistoryInfo{ imageSampler, momentHistoryImageView, VK_IMAGE_LAYOUT_GENERAL };

		VkDescriptorImageInfo integratedColorInfo{ imageSampler, integratedColorImageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo integratedMomentInfo{ imageSampler, integratedMomentsImageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo updatedHistoryLengthInfo{ imageSampler, updatedHistoryLengthImageView, VK_IMAGE_LAYOUT_GENERAL };
		VkDescriptorImageInfo varianceInfo{ imageSampler, varianceImageView, VK_IMAGE_LAYOUT_GENERAL };

		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 0, &rtImageInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 1, &currentGBufferPositionInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 2, &currentGBufferNormalPrimIDInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 3, &previousGBufferPositionInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 4, &previousGBufferNormalPrimIDInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 5, &historyLengthInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 6, &colorHistoryInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 7, &momentHistoryInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 8, &integratedColorInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 9, &integratedMomentInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 10, &updatedHistoryLengthInfo));
		writes.push_back(reprojectionDescBindings.makeWrite(reprojectionDescSet, 11, &varianceInfo));

		writes.push_back(updateHistoryDescBindings.makeWrite(updateHistoryDescSet, 0, &integratedMomentInfo));
		writes.push_back(updateHistoryDescBindings.makeWrite(updateHistoryDescSet, 1, &updatedHistoryLengthInfo));
		writes.push_back(updateHistoryDescBindings.makeWrite(updateHistoryDescSet, 2, &momentHistoryInfo));
		writes.push_back(updateHistoryDescBindings.makeWrite(updateHistoryDescSet, 3, &historyLengthInfo));
		writes.push_back(updateHistoryDescBindings.makeWrite(updateHistoryDescSet, 4, &integratedColorInfo));
		writes.push_back(updateHistoryDescBindings.makeWrite(updateHistoryDescSet, 5, &colorHistoryInfo));

		vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	/*
	* create general (rasterizer) pipeline
	*/
	void createGBufferPipeline() {
		PipelineGenerator gen(devices.device);
		gen.addVertexInputBindingDescription({
			{0, sizeof(glm::vec3)},
			{1, sizeof(glm::vec3)},
			{2, sizeof(glm::vec2)}
		});
		gen.addVertexInputAttributeDescription({
			{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, //pos
			{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, //normal
			{2, 2, VK_FORMAT_R32G32_SFLOAT, 0}, //texcoord0
		});
		gen.addDescriptorSetLayout({ descriptorSetLayout });
		gen.addPushConstantRange({ {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(gbufferPushConstants)} });
		gen.addShader(vktools::createShaderModule(devices.device, vktools::readFile("shaders/gbuffer_vert.spv")),
			VK_SHADER_STAGE_VERTEX_BIT);
		gen.addShader(vktools::createShaderModule(devices.device, vktools::readFile("shaders/gbuffer_frag.spv")),
			VK_SHADER_STAGE_FRAGMENT_BIT);
		gen.setColorBlendInfo(VK_FALSE, 3);
		gen.generate(gbufferRenderPass, &gbufferPipeline, &gbufferPipelineLayout);
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
		descriptorSet = vktools::allocateDescriptorSets(devices.device, descriptorSetLayout, descriptorPool, nbDescriptorSet).back();

		std::vector<VkDescriptorImageInfo> imageInfos{};
		for (auto& image : gltfDioramaModel.images) {
			imageInfos.emplace_back(image.descriptor);
		}

		VkDescriptorBufferInfo camMatricesInfo{ matricesUniformBuffer, 0, sizeof(CameraMatrices) };
		VkDescriptorBufferInfo sceneBufferInfo{ sceneBuffer, 0, objInstances.size() * sizeof(ObjInstance) };

		std::vector<VkWriteDescriptorSet> writes;
		writes.emplace_back(descriptorSetBindings.makeWrite(descriptorSet, 0, &camMatricesInfo));
		writes.emplace_back(descriptorSetBindings.makeWrite(descriptorSet, 1, &sceneBufferInfo));
		writes.emplace_back(descriptorSetBindings.makeWriteArray(descriptorSet, 2, imageInfos.data()));
		vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}

	/*
	* create destination image for raytracing pass
	*/
	void createRaytraceDestinationImage() {
		//delete resources
		devices.memoryAllocator.freeImageMemory(rtDestinationImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyImage(devices.device, rtDestinationImage, nullptr);
		vkDestroyImageView(devices.device, rtDestinationImageView, nullptr);

		//create image
		devices.createImage(rtDestinationImage,
			{ swapchain.extent.width, swapchain.extent.height, 1 },
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			1,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		//create image view
		rtDestinationImageView = vktools::createImageView(devices.device,
			rtDestinationImage,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_ASPECT_COLOR_BIT,
			1
		);

		//explicit layout transition
		VkCommandBuffer cmdBuf = devices.beginCommandBuffer();
		vktools::setImageLayout(cmdBuf,
			rtDestinationImage,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0,1 }
		);
		devices.endCommandBuffer(cmdBuf);
	}

	/*
	* create raytrace descriptor set - acceleration structure & raytracing pipeline destination image
	*/
	void createRtDescriptorSet() {
		rtDescriptorSetBindings.addBinding(0,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR); //tlas
		rtDescriptorSetBindings.addBinding(1,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			1,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR); //output image
		rtDescriptorSetBindings.addBinding(2,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			1,
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

		//create rt descriptor pool & layout
		uint32_t nbRtDescriptorSet = 1;
		rtDescriptorPool = rtDescriptorSetBindings.createDescriptorPool(devices.device, nbRtDescriptorSet);
		rtDescriptorSetLayout = rtDescriptorSetBindings.createDescriptorSetLayout(devices.device);
		rtDescriptorSet = vktools::allocateDescriptorSets(devices.device, rtDescriptorSetLayout, rtDescriptorPool, nbRtDescriptorSet).front();

		VkDescriptorBufferInfo primitiveInfo{ gltfDioramaModel.primitiveBuffer, 0, VK_WHOLE_SIZE };

		//update raytrace descriptor sets
		for (uint32_t i = 0; i < nbRtDescriptorSet; ++i) {
			VkWriteDescriptorSetAccelerationStructureKHR descAsInfo{};
			descAsInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			descAsInfo.accelerationStructureCount = 1;
			descAsInfo.pAccelerationStructures = &tlas.accel;
			VkDescriptorImageInfo imageInfo{ {}, rtDestinationImageView, VK_IMAGE_LAYOUT_GENERAL };

			std::vector<VkWriteDescriptorSet> writes;
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSet, 0, &descAsInfo));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSet, 1, &imageInfo));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSet, 2, &primitiveInfo));
			vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		}
	}

	/*
	* update raytrace descriptor set - set image where raytrac pipeline output is stored
	* this should be called in windowResized()
	*
	* @param currentFrame - index of raytrace descriptor set (0 <= currentFrame < MAX_FRAMES_IN_FLIGHT)
	*/
	void updateRtDescriptorSet() {
		VkDescriptorImageInfo imageInfo{ {},
			rtDestinationImageView,
			VK_IMAGE_LAYOUT_GENERAL
		};
		VkWriteDescriptorSet wds = rtDescriptorSetBindings.makeWrite(rtDescriptorSet, 1, &imageInfo);
		vkUpdateDescriptorSets(devices.device, 1, &wds, 0, nullptr);
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
		gen.setColorBlendInfo(VK_TRUE, 1);
		gen.generate(postRenderPass, &postPipeline, &postPipelineLayout);
	}

	/*
	* create post descriptor set - one image sampler for ray tracing pipeline output
	*/
	void createPostDescriptorSet() {
		postDescriptorSetBindings.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1, VK_SHADER_STAGE_FRAGMENT_BIT);

		uint32_t nbDescriptorSet = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		postDescriptorSetLayout = postDescriptorSetBindings.createDescriptorSetLayout(devices.device);
		postDescriptorPool = postDescriptorSetBindings.createDescriptorPool(devices.device, nbDescriptorSet);
		postDescriptorSet = vktools::allocateDescriptorSets(devices.device, postDescriptorSetLayout,
			postDescriptorPool, nbDescriptorSet).front();
	}

	/*
	* update post descriptor set - sampler references offscreen color buffer
	*/
	void updatePostDescriptorSet() {
		VkDescriptorImageInfo colorInfo{
			imageSampler,
			integratedColorImageView,
			VK_IMAGE_LAYOUT_GENERAL
		};
		VkWriteDescriptorSet wd = postDescriptorSetBindings.makeWrite(
			postDescriptorSet,
			0,
			&colorInfo);
		vkUpdateDescriptorSets(devices.device, 1, &wd, 0, nullptr);
	}

	/*
	* create uniform buffers - camera matrices
	*/
	void createUniformbuffer() {
		VkBufferCreateInfo info = vktools::initializers::bufferCreateInfo(sizeof(CameraMatrices),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &info, nullptr, &matricesUniformBuffer));
		matricesUniformBufferMemory = devices.memoryAllocator.allocateBufferMemory(matricesUniformBuffer,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	/*
	* update uniform buffer - camera matrices
	*
	* @param currentFrame - index of uniform buffer (0 <= currentFrame < MAX_FRAMES_IN_FLIGHT)
	*/
	void updateUniformBuffer() {
		matricesUniformBufferMemory.mapData(devices.device, &cameraMatrices);
	}

	/*
	* record raytrace commands
	*
	* @param cmdBuf
	* @param descriptorSetIndex
	*/
	void raytrace(VkCommandBuffer cmdBuf) {
		/*if (rtPushConstants.frame > 2000) {
			return;
		}*/

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);

		std::vector<VkDescriptorSet> descSets{ rtDescriptorSet, descriptorSet };
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

	/*
	* rasterize
	*
	* @param cmdBuf
	* @param resourceIndex
	*/
	void rasterize(VkCommandBuffer cmdBuf) {
		vktools::setViewportScissorDynamicStates(cmdBuf, swapchain.extent);
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipelineLayout,
			0, 1, &descriptorSet, 0, nullptr);

		//bind vertex / index buffers
		VkDeviceSize offsets[3] = { 0, 0, 0 };
		VkBuffer vertexBuffers[3] = {
			gltfDioramaModel.vertexBuffer,
			gltfDioramaModel.normalBuffer,
			gltfDioramaModel.uvBuffer
		};
		vkCmdBindVertexBuffers(cmdBuf, 0, 3, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuf, gltfDioramaModel.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		//draw
		for (VulkanGLTF::Node& node : gltfDioramaModel.nodes) {
			VulkanGLTF::Primitive& primitive = gltfDioramaModel.primitives[node.primitiveIndex];
			gbufferPushConstants.modelMatrix = node.matrix;
			gbufferPushConstants.materialId = primitive.materialIndex;
			gbufferPushConstants.primitiveId = node.primitiveIndex;
			vkCmdPushConstants(cmdBuf, gbufferPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(gbufferPushConstants), &gbufferPushConstants);
			vkCmdDrawIndexed(cmdBuf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
		}
	}
};

//entry point
RUN_APPLICATION_MAIN(VulkanApp, 1024, 768, "svgf");
