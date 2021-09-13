#include <include/imgui/imgui.h>
#include "core/vulkan_app_base.h"
#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "core/vulkan_mesh.h"
#include "core/vulkan_imgui.h"
#include "core/vulkan_pipeline.h"

/*
* derived imgui class - define newFrame();
*/
class Imgui : public ImguiBase {
public:
	virtual void newFrame() override {
		ImGui::NewFrame();
		ImGui::Begin("Settings");

		//toggle render mode
		static int renderMode = 0;
		ImGui::RadioButton("raytrace", &renderMode, 0); ImGui::SameLine();
		ImGui::RadioButton("rasterizer", &renderMode, 1);
		if (renderMode != userInput.renderMode) {
			userInput.renderMode = renderMode;
			rerecordCommandBuffer = true;
		}

		//
		ImGui::SliderFloat("Light position X", &userInput.lightPos.x, -3.0f, 3.0f);
		ImGui::SliderFloat("Light position Y", &userInput.lightPos.y, -3.0f, 3.0f);
		ImGui::SliderFloat("Light position Z", &userInput.lightPos.z, -3.0f, 3.0f);
		static glm::vec3 lightPos;
		if (lightPos != userInput.lightPos) {
			lightPos = userInput.lightPos;
			rerecordCommandBuffer = true;
		}

		ImGui::End();
		ImGui::Render();
	}

	/** imgui user input collection */
	struct UserInput {
		int renderMode = 0;
		glm::vec3 lightPos{ 0, 1.5, 0.1 };
	}userInput;
};

class VulkanApp : public VulkanAppBase {
public:
	/*
	* constructor - get window size & title
	*/
	VulkanApp(int width, int height, const std::string& appName)
		: VulkanAppBase(width, height, appName) {
		enabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		imgui = new Imgui;
	}

	/*
	* destructor - destroy vulkan objects created in this level
	*/
	~VulkanApp() {
		//imgui
		imgui->cleanup();
		delete imgui;

		devices.memoryAllocator.freeBufferMemory(rtSBTBuffer,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		vkDestroyBuffer(devices.device, rtSBTBuffer, nullptr);
		vkDestroyPipeline(devices.device, generalPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, generalPipelineLayout, nullptr);
		vkDestroyPipeline(devices.device, rtPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, rtPipelineLayout, nullptr);
		vkDestroyPipeline(devices.device, postPipeline, nullptr);
		vkDestroyPipelineLayout(devices.device, postPipelineLayout, nullptr);
		vkDestroyRenderPass(devices.device, postRenderPass, nullptr);

		//BLAS
		for (auto& as : blas) {
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

		//gltf buffers
		devices.memoryAllocator.freeBufferMemory(vertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, vertexBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(indexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, indexBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(normalBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, normalBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(uvBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, uvBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(primInfoBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, primInfoBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(sceneDescBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, sceneDescBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(materialBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, materialBuffer, nullptr);

		//swapchain framebuffers
		for (auto& framebuffer : framebuffers) {
			vkDestroyFramebuffer(devices.device, framebuffer, nullptr);
		}

		//offscreen framebuffer
		for (auto& framebuffer : offscreenFramebuffers) {
			vkDestroyFramebuffer(devices.device, framebuffer, nullptr);
		}

		//offscreen renderpass
		vkDestroyRenderPass(devices.device, offscreenRenderPass, nullptr);

		//offscreen images
		for (auto& offscreen : offscreens) {
			offscreen.offscreenColorBuffer.cleanup();
			offscreen.offscreenDepthBuffer.cleanup();
		}

		//descriptors
		vkDestroyDescriptorPool(devices.device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, rtDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, rtDescriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(devices.device, postDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(devices.device, postDescriptorSetLayout, nullptr);

		//uniform buffers
		for (auto& uniformBuffer : uniformBuffers) {
			devices.memoryAllocator.freeBufferMemory(uniformBuffer,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			vkDestroyBuffer(devices.device, uniformBuffer, nullptr);
		}
	}

	/*
	* application initialization - also contain base class initApp()
	*/
	virtual void initApp() override {
		VulkanAppBase::initApp();

		//camera
		glm::vec3 camPos = glm::vec3(0, 1, 3.5);
		glm::vec3 center = glm::vec3(0, 1, 0);
		glm::vec3 camUp = glm::vec3(0, 1, 0);
		camera.view = glm::lookAt(camPos, center, camUp);
		camera.viewInverse = glm::inverse(camera.view);
		camera.proj = glm::perspective(glm::radians(45.f),
			swapchain.extent.width / static_cast<float>(swapchain.extent.height), 0.1f, 10.f);
		camera.proj[1][1] *= -1;
		camera.projInverse = glm::inverse(camera.proj);

		//request ray tracing properties
		VkPhysicalDeviceProperties2 properties2{};
		properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties2.pNext = &rtProperties;
		vkGetPhysicalDeviceProperties2(devices.physicalDevice, &properties2);

		//load cornell box gltf
		loadScene();

		//BLAS
		std::vector<Mesh::BlasInput> allBlas;
		allBlas.reserve(cornellBox.m_primMeshes.size());
		for (auto& primMesh : cornellBox.m_primMeshes) {
			allBlas.push_back(Mesh::getVkGeometryKHR(devices.device, primMesh, vertexBuffer, indexBuffer));
		}
		buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

		//TLAS
		std::vector<VkAccelerationStructureInstanceKHR> tlas;
		for (auto& node : cornellBox.m_nodes) {
			VkAccelerationStructureInstanceKHR rayInst;
			rayInst.transform = vktools::toTransformMatrixKHR(node.worldMatrix);
			rayInst.instanceCustomIndex = node.primMesh;
			rayInst.accelerationStructureReference = getBlasDeviceAddress(node.primMesh);
			rayInst.instanceShaderBindingTableRecordOffset = 0; // we will use the same hit group for all object
			rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			rayInst.mask = 0xFF;
			tlas.emplace_back(rayInst);
		}
		buildTlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

		//uniform buffers
		createUniformbuffer();
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updateUniformBuffer(i);
		}

		createOffscreenRender();
		createDescriptorSet();
		createGeneralPipeline();

		/*
		* ray tracing
		*/
		//descriptor set
		createRtDescriptorSet();
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updateRtDescriptorSet(i);
		}
		//pipeline
		createRtPipeline();
		//SBT
		createRtShaderBindingTable();

		/*
		* post
		*/
		//post descriptor set
		createPostDescriptorSet();
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updatePostDescriptorSet(i);
		}
		//render pass
		postRenderPass = vktools::createRenderPass(devices.device,
			{ swapchain.surfaceFormat.format }, depthFormat, 1, true, true);
		//pipeline
		createPostPipeline();

		//imgui
		imgui->init(&devices, swapchain.extent.width, swapchain.extent.height, postRenderPass, MAX_FRAMES_IN_FLIGHT);

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
		size_t commandBufferIndex = currentFrame * framebuffers.size() + imageIndex;
		submitInfo.pCommandBuffers = &commandBuffers[commandBufferIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderCompleteSemaphores[currentFrame];
		VK_CHECK_RESULT(vkQueueSubmit(devices.graphicsQueue, 1, &submitInfo, frameLimitFences[currentFrame]));

		submitFrame(imageIndex);
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
	virtual void recordCommandBuffer() override {
		VkCommandBufferBeginInfo cmdBufBeginInfo{};
		cmdBufBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.1f, 0.1f, 0.1f, 1.f };
		clearValues[1].depthStencil = { 1.f, 0 };

		//for rasterizer render pass
		VkRenderPassBeginInfo offscreenRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		offscreenRenderPassBeginInfo.clearValueCount = 2;
		offscreenRenderPassBeginInfo.pClearValues = clearValues.data();
		offscreenRenderPassBeginInfo.renderPass = offscreenRenderPass;
		offscreenRenderPassBeginInfo.renderArea = { {0, 0},swapchain.extent };

		//for #2 render pass
		VkRenderPassBeginInfo postRenderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		postRenderPassBeginInfo.clearValueCount = 2;
		postRenderPassBeginInfo.pClearValues = clearValues.data();
		postRenderPassBeginInfo.renderPass = postRenderPass;
		postRenderPassBeginInfo.renderArea = { {0, 0},swapchain.extent };

		for (size_t i = 0; i < framebuffers.size() * MAX_FRAMES_IN_FLIGHT; ++i) {
			VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffers[i], &cmdBufBeginInfo));

			//#1 raytracing
			size_t resourceIndex = i / framebuffers.size();
			if (static_cast<Imgui*>(imgui)->userInput.renderMode == RENDER_MODE::RAYRACE) {
				raytrace(commandBuffers[i], resourceIndex);
			}
			else {
				offscreenRenderPassBeginInfo.framebuffer = offscreenFramebuffers[resourceIndex];
				vkCmdBeginRenderPass(commandBuffers[i], &offscreenRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				rasterize(commandBuffers[i], resourceIndex);
				vkCmdEndRenderPass(commandBuffers[i]);
			}

			//#2 full screen quad
			size_t framebufferIndex = i % framebuffers.size();
			postRenderPassBeginInfo.framebuffer = framebuffers[framebufferIndex];
			vkCmdBeginRenderPass(commandBuffers[i], &postRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vktools::setViewportScissorDynamicStates(commandBuffers[i], swapchain.extent);
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postPipeline);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postPipelineLayout, 0, 1,
				&postDescriptorSets[resourceIndex], 0, nullptr);
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); //full screen triangle

			imgui->drawFrame(commandBuffers[i], resourceIndex);

			vkCmdEndRenderPass(commandBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffers[i]));
		}
	}

	/*
	* resize
	*/
	virtual void resizeWindow(bool /*recordCmdBuf*/) override {
		VulkanAppBase::resizeWindow(false);
		createOffscreenRender();
		for (size_t i = 0; i < static_cast<int>(MAX_FRAMES_IN_FLIGHT); ++i) {
			updateRtDescriptorSet(i);
			updatePostDescriptorSet(i);
		}
		recordCommandBuffer();
	}

private:
	/** swapchain framebuffers */
	std::vector<VkFramebuffer> framebuffers;
	/** rt properties */
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

	//for retrieving primitive info in chit
	struct PrimMeshInfo {
		unsigned int indexOffset;
		unsigned int vertexOffset;
		int materialIndex;
	};

	//scene buffer address
	struct SceneDesc {
		uint64_t vertexAddress;
		uint64_t normalAddress;
		uint64_t uvAddress;
		uint64_t indexAddress;
		uint64_t materialAddress;
		uint64_t primInfoAddress;
	};

	struct GltfShadeMaterial {
		glm::vec4 pbtBaseColorFactor;
		glm::vec3 emissiveFactor;
		int pbtBaseColorTexture;
	};

	/** gltf buffers */
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkBuffer normalBuffer = VK_NULL_HANDLE;
	VkBuffer uvBuffer = VK_NULL_HANDLE;
	VkBuffer materialBuffer = VK_NULL_HANDLE;
	VkBuffer primInfoBuffer = VK_NULL_HANDLE;
	VkBuffer sceneDescBuffer = VK_NULL_HANDLE;
	/** cornell box gltf */
	GltfScene cornellBox;

	/*Imgui user input*/
	enum RENDER_MODE {
		RAYRACE,
		RASTERIZER
	};

	/*
	* acceleration structures
	*/
	/** acceleration structure handle & buffer */
	struct AccelKHR {
		/** as handle */
		VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
		/** buffer storing vertex data */
		VkBuffer buffer;
	};
	/** acceleration structure build info */
	struct BuildAccelerationStructure {
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		const VkAccelerationStructureBuildRangeInfoKHR* buildRangeInfo = nullptr;
		VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		AccelKHR as{};
	};
	/** bottom-level acceleration structures */
	std::vector<AccelKHR> blas;
	/** top-level acceleration structure */
	AccelKHR tlas{};
	/** instance buffer for tlas */
	VkBuffer instanceBuffer = VK_NULL_HANDLE;


	/*
	* camera matrices - uniform buffer
	*/
	struct CameraMatrices {
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
	} camera;
	/** uniform buffers for camera matrices */
	std::vector<VkBuffer> uniformBuffers;
	/** uniform buffer memories */
	std::vector<MemoryAllocator::HostVisibleMemory> uniformBufferMemories;


	/*
	* descriptors for rasterizaer & raytracer
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
	* offscreen images - desctination images rt / rasterizer pipelines
	*/
	struct OffscreenImages {
		/** offscreen depth image */
		Texture2D offscreenDepthBuffer;
		/** offscreen color image */
		Texture2D offscreenColorBuffer;
	};
	std::vector<OffscreenImages> offscreens;
	/** offscreen depth format */
	VkFormat offscreenDepthFormat{ VK_FORMAT_X8_D24_UNORM_PACK32 };
	/** offscreen color format */
	VkFormat offscreenColorFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };
	/** offscreen renderpass */
	VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;

	/*
	* used for rasterizer
	*/
	/** offscreen framebuffers */
	std::vector<VkFramebuffer> offscreenFramebuffers;
	/** general pipeline */
	VkPipeline generalPipeline = VK_NULL_HANDLE;
	/** general pipeline layout */
	VkPipelineLayout generalPipelineLayout = VK_NULL_HANDLE;
	struct RasterPushConstant {
		glm::mat4 modelMatrix{ 1.f };
		glm::vec3 lightPos{ 0.f, 1.f, .1f };
		uint32_t objIndex = 0;
		float lightIntensity = 1.f;
		uint32_t lightType = 0;
		uint32_t materialId = 0;
	}rasterPushConstants;

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
		glm::vec4 clearColor;
		glm::vec3 lightPos;
		float lightIntensity = 1.f;
		int lightType = 0;
	} rtPushConstants;


	/*
	* descriptor for full quad pipeline - post pipeline 
	* 1 image sampler
	*/
	/** descriptor set layout bindings */
	DescriptorSetBindings postDescriptorSetBindings;
	/** descriptor set layout */
	VkDescriptorSetLayout postDescriptorSetLayout = VK_NULL_HANDLE;
	/** decriptor pool */
	VkDescriptorPool postDescriptorPool = VK_NULL_HANDLE;
	/** descriptor sets */
	std::vector<VkDescriptorSet> postDescriptorSets;


	/*
	* full quad render resources
	*/
	/** full quad pipeline */
	VkPipeline postPipeline = VK_NULL_HANDLE;
	/** full quad pipeline layout */
	VkPipelineLayout postPipelineLayout = VK_NULL_HANDLE;
	/** normal render pass */
	VkRenderPass postRenderPass = VK_NULL_HANDLE;

	void loadScene() {
		//load cornell box
		cornellBox = Mesh::loadGltf("../../meshes/scene.gltf");

		VkBufferUsageFlags rtFlags = 
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		createBuffer(cornellBox.m_positions.data(), 
			static_cast<VkDeviceSize>(sizeof(cornellBox.m_positions[0]) * cornellBox.m_positions.size()),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rtFlags,
			vertexBuffer);
		createBuffer(cornellBox.m_indices.data(),
			static_cast<VkDeviceSize>(sizeof(cornellBox.m_indices[0]) * cornellBox.m_indices.size()),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rtFlags,
			indexBuffer);
		createBuffer(cornellBox.m_normals.data(),
			static_cast<VkDeviceSize>(sizeof(cornellBox.m_normals[0]) * cornellBox.m_normals.size()),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			normalBuffer);
		createBuffer(cornellBox.m_texcoords0.data(),
			static_cast<VkDeviceSize>(sizeof(cornellBox.m_texcoords0[0]) * cornellBox.m_texcoords0.size()),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			uvBuffer);

		//copy materials we need
		std::vector<GltfShadeMaterial> shadeMaterials;
		for (auto& m : cornellBox.m_materials) {
			shadeMaterials.emplace_back(GltfShadeMaterial{m.baseColorFactor, m.emissiveFactor, m.baseColorTexture});
		}
		createBuffer(shadeMaterials.data(),
			static_cast<VkDeviceSize>(sizeof(shadeMaterials[0]) * shadeMaterials.size()),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			materialBuffer);

		//used to find the primitive mesh info in chit
		std::vector<PrimMeshInfo> primLookup;
		for (auto& primMesh : cornellBox.m_primMeshes) {
			primLookup.push_back({primMesh.firstIndex, primMesh.vertexOffset, primMesh.materialIndex});
		}
		createBuffer(primLookup.data(),
			static_cast<VkDeviceSize>(sizeof(primLookup[0]) * primLookup.size()),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			primInfoBuffer);

		SceneDesc sceneDesc;
		sceneDesc.vertexAddress = vktools::getBufferDeviceAddress(devices.device, vertexBuffer);
		sceneDesc.indexAddress = vktools::getBufferDeviceAddress(devices.device, indexBuffer);
		sceneDesc.normalAddress = vktools::getBufferDeviceAddress(devices.device, normalBuffer);
		sceneDesc.uvAddress = vktools::getBufferDeviceAddress(devices.device, uvBuffer);
		sceneDesc.materialAddress = vktools::getBufferDeviceAddress(devices.device, materialBuffer);
		sceneDesc.primInfoAddress = vktools::getBufferDeviceAddress(devices.device, primInfoBuffer);
		createBuffer(&sceneDesc,
			static_cast<VkDeviceSize>(sizeof(sceneDesc)),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			sceneDescBuffer);

	}

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

		//create vertex & index buffer
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
	* create acceleration structure & buffer
	* 
	* @param info - acceleration structure create info
	* 
	* @return AccelKHR - contain created acceleration structure & buffer
	*/
	AccelKHR createAcceleration(VkAccelerationStructureCreateInfoKHR& info) {
		AccelKHR resultAs;
		devices.createBuffer(resultAs.buffer, info.size,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

		info.buffer = resultAs.buffer;
		vkfp::vkCreateAccelerationStructureKHR(devices.device, &info, nullptr, &resultAs.accel);
		return resultAs;
	}

	/*
	* build bottom-level acceleration structure
	* 
	* @param input - vector blas input built from mesh
	* @param
	*/
	void buildBlas(const std::vector<Mesh::BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags) {
		uint32_t nbBlas = static_cast<uint32_t>(input.size());
		VkDeviceSize asTotalSize{ 0 };
		uint32_t nbCompactions{ 0 };
		VkDeviceSize maxScratchSize{ 0 };

		std::vector<BuildAccelerationStructure> buildAs(input.size());
		
		for (uint32_t blasIndex = 0; blasIndex < nbBlas; ++blasIndex) {
			//fill VkAccelerationStructureBuildGeometryInfoKHR partially for querying the build sizes
			buildAs[blasIndex].buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildAs[blasIndex].buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildAs[blasIndex].buildInfo.flags = flags;
			buildAs[blasIndex].buildInfo.geometryCount = static_cast<uint32_t>(input[blasIndex].asGeometry.size());
			buildAs[blasIndex].buildInfo.pGeometries = input[blasIndex].asGeometry.data();

			//build range info
			buildAs[blasIndex].buildRangeInfo = input[blasIndex].asBuildOffsetInfo.data();

			//finding sizes to create acceleration structures and scratch
			std::vector<uint32_t> maxPrimCount(input[blasIndex].asBuildOffsetInfo.size());
			for (size_t i = 0; i < maxPrimCount.size(); ++i) {
				maxPrimCount[i] = input[blasIndex].asBuildOffsetInfo[i].primitiveCount;
			}
			vkfp::vkGetAccelerationStructureBuildSizesKHR(devices.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&buildAs[blasIndex].buildInfo, maxPrimCount.data(), &buildAs[blasIndex].buildSizesInfo);

			asTotalSize += buildAs[blasIndex].buildSizesInfo.accelerationStructureSize;
			maxScratchSize = std::max(maxScratchSize, buildAs[blasIndex].buildSizesInfo.buildScratchSize);
			nbCompactions += buildAs[blasIndex].buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
		}
		
		//create scratch buffer
		VkBuffer scratchBuffer;
		VkBufferCreateInfo scratchBufferInfo = vktools::initializers::bufferCreateInfo(maxScratchSize,
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &scratchBufferInfo, nullptr, &scratchBuffer));
		devices.memoryAllocator.allocateBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);//memProperty ????

		VkDeviceAddress scratchAddress = vktools::getBufferDeviceAddress(devices.device, scratchBuffer);

		//querying the real size of BLAS
		VkQueryPool queryPool{ VK_NULL_HANDLE };
		if (nbCompactions > 0) {
			assert(nbCompactions == nbBlas); //don't allow mix of on/off compaction
			VkQueryPoolCreateInfo queryPoolCreateInfo{};
			queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			queryPoolCreateInfo.queryCount = nbBlas;
			queryPoolCreateInfo.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
			vkCreateQueryPool(devices.device, &queryPoolCreateInfo, nullptr, &queryPool);
		}

		//split BLAS creation into chunks of ~256MB to avoid pipeline stalling
		std::vector<uint32_t> indices;
		VkDeviceSize batchSize{ 0 };
		VkDeviceSize batchLimit{ 256'000'000 };
		
		for (uint32_t blasIndex = 0; blasIndex < nbBlas; ++blasIndex) {
			indices.push_back(blasIndex);
			batchSize += buildAs[blasIndex].buildSizesInfo.accelerationStructureSize;

			//over the limit or last blas element
			if (batchSize >= batchLimit || blasIndex == nbBlas - 1) {
				VkCommandBuffer cmdBuf = devices.beginOneTimeSubmitCommandBuffer();
				cmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
				devices.endOneTimeSubmitCommandBuffer(cmdBuf);

				if (queryPool) {
					VkCommandBuffer cmdBuf = devices.beginOneTimeSubmitCommandBuffer();
					cmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
					devices.endOneTimeSubmitCommandBuffer(cmdBuf);
				}

				//reset
				batchSize = 0;
				indices.clear();
			}
		}

		//keep all the created acceleration structures
		for (auto& s : buildAs) {
			blas.emplace_back(s.as);
		}

		//cleanup
		vkDestroyQueryPool(devices.device, queryPool, nullptr);
		devices.memoryAllocator.freeBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, scratchBuffer, nullptr);
	}

	/*
	* record building acceleration structure command to the command buffer
	* 
	* @param cmdBuf - command buffer to record
	* @param indices - indices of blas
	* @param buildAs - vector of acceleration structure info
	* @param scratchAddress - buffer address where ac data is temporarily stored
	* @param queryPool - query to find the real amount of memory
	*/
	void cmdCreateBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
		std::vector<BuildAccelerationStructure>& buildAs, VkDeviceAddress scratchAddress, VkQueryPool queryPool) {
		if (queryPool) {
			vkResetQueryPool(devices.device, queryPool, 0, static_cast<uint32_t>(indices.size()));
		}
		uint32_t queryCount = 0;

		for (uint32_t blasIndex : indices) {
			//actual allocation of buffer and acceleration structure
			VkAccelerationStructureCreateInfoKHR createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			createInfo.size = buildAs[blasIndex].buildSizesInfo.accelerationStructureSize;
			buildAs[blasIndex].as = createAcceleration(createInfo);

			buildAs[blasIndex].buildInfo.dstAccelerationStructure = buildAs[blasIndex].as.accel;
			buildAs[blasIndex].buildInfo.scratchData.deviceAddress = scratchAddress;

			//build the blas
			vkfp::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[blasIndex].buildInfo, &buildAs[blasIndex].buildRangeInfo);

			//since scratch buffer is resued across builds, we need a barrier to ensure one build
			VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vkCmdPipelineBarrier(cmdBuf,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				0,
				1, &barrier,
				0, nullptr,
				0, nullptr);

			if (queryPool) {
				//add a query to find the real amount of memory needed, used for compaction
				vkfp::vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &buildAs[blasIndex].buildInfo.dstAccelerationStructure,
					VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCount++);
			}
		}
	}

	/*
	* create and replace a new acceleration structure and buffer based on the size retrieved by the query
	* 
	* @param cmdBuf - command buffer to record
	* @param indices - indices of blas
	* @param buildAs - vector of acceleration structure info
	* @param queryPool - query to find the real amount of memory
	*/
	void cmdCompactBlas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
		std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool) {
		uint32_t queryCount = 0;
		std::vector<AccelKHR> cleanupAs;

		//get the compacted size result back
		std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
		vkGetQueryPoolResults(devices.device, queryPool, 0, (uint32_t)compactSizes.size(),
			compactSizes.size() * sizeof(VkDeviceSize), compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);

		for (uint32_t blasIndex : indices) {
			cleanupAs.push_back(buildAs[blasIndex].as);
			buildAs[blasIndex].buildSizesInfo.accelerationStructureSize = compactSizes[queryCount++]; //new reduced size

			//create a compact version of the AS
			VkAccelerationStructureCreateInfoKHR asCreateInfo{};
			asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
			asCreateInfo.size = buildAs[blasIndex].buildSizesInfo.accelerationStructureSize;
			asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildAs[blasIndex].as = createAcceleration(asCreateInfo);

			//copy the original blas to a compact version
			VkCopyAccelerationStructureInfoKHR copyInfo{};
			copyInfo.sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
			copyInfo.src = buildAs[blasIndex].buildInfo.dstAccelerationStructure;
			copyInfo.dst = buildAs[blasIndex].as.accel;
			copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
			vkfp::vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
		}

		//destroy old acceleration structures
		for (auto& as : cleanupAs) {
			vkfp::vkDestroyAccelerationStructureKHR(devices.device, as.accel, nullptr);
			devices.memoryAllocator.freeBufferMemory(as.buffer);
		}
	}

	/*
	* return the device address of a Blas previously created
	* 
	* @param blasIndex - index of blas to find address
	*/
	VkDeviceAddress getBlasDeviceAddress(uint32_t blasIndex) {
		assert((size_t)blasIndex < blas.size());
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = blas[blasIndex].accel;
		return vkfp::vkGetAccelerationStructureDeviceAddressKHR(devices.device, &addressInfo);
	}

	/*
	* build top-level acceleration structure
	* 
	* @param instance - 
	* @param flags - used for acceleration structure build info
	*/
	void buildTlas(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
		VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
		bool update = false) {
		//cannot call buildTlas twice except to update
		assert(tlas.accel == VK_NULL_HANDLE || update);

		//create a buffer holding the actual instance data for use by the AS builder
		VkDeviceSize instanceDescsSizeInBytes = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

		//allocate the instance buffer and copy its contents from host to device memory
		if (update) {
			devices.memoryAllocator.freeBufferMemory(instanceBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			vkDestroyBuffer(devices.device, instanceBuffer, nullptr);
		}

		//create staging buffer
		VkBuffer stagingBuffer;
		VkBufferCreateInfo stagingBufferCreateInfo = vktools::initializers::bufferCreateInfo(
			instanceDescsSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &stagingBufferCreateInfo, nullptr, &stagingBuffer));

		//suballocate
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		MemoryAllocator::HostVisibleMemory hostVisibleMemory = devices.memoryAllocator.allocateBufferMemory(
			stagingBuffer, properties);

		hostVisibleMemory.mapData(devices.device, instances.data());

		//create vertex & index buffer
		VkBufferCreateInfo bufferCreateInfo = vktools::initializers::bufferCreateInfo(
			instanceDescsSizeInBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &bufferCreateInfo, nullptr, &instanceBuffer));

		//suballocation
		devices.memoryAllocator.allocateBufferMemory(instanceBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkCommandBuffer cmdBuf= devices.beginOneTimeSubmitCommandBuffer();

		//copy buffer
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = instanceDescsSizeInBytes;
		vkCmdCopyBuffer(cmdBuf, stagingBuffer, instanceBuffer, 1, &copyRegion);

		//make sure the copy of the instance buffer are copied before triggering the acceleration structure build
		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);

		//create VkAccelerationStructureGeometryInstanceDataKHR
		//This wraps a device pointer to the above uploaded instances
		VkAccelerationStructureGeometryInstancesDataKHR instancesVk{};
		instancesVk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		instancesVk.arrayOfPointers = VK_FALSE;
		instancesVk.data.deviceAddress = vktools::getBufferDeviceAddress(devices.device, instanceBuffer);

		//put the above into a VkAccelerationStructureGeometryKHR
		//we need to put the instances struct in a union and label it as instance data
		VkAccelerationStructureGeometryKHR topAsGeometry{};
		topAsGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		topAsGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topAsGeometry.geometry.instances = instancesVk;

		//query the needed memory for the tlas and scratch space
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfo.flags = flags;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &topAsGeometry;
		buildInfo.mode = update
			? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
			: VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		uint32_t count = (uint32_t)instances.size();
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		vkfp::vkGetAccelerationStructureBuildSizesKHR(devices.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&buildInfo, &count, &sizeInfo);

		//create tlas
		if (update == false) {
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			createInfo.size = sizeInfo.accelerationStructureSize;
			tlas = createAcceleration(createInfo);
		}
		//allocate the scratch memory
		VkBuffer scratchBuffer = VK_NULL_HANDLE;
		VkBufferCreateInfo scratchBufferInfo = vktools::initializers::bufferCreateInfo(sizeInfo.buildScratchSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | 
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
		VK_CHECK_RESULT(vkCreateBuffer(devices.device, &scratchBufferInfo, nullptr, &scratchBuffer));
		devices.memoryAllocator.allocateBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		//update build info
		buildInfo.srcAccelerationStructure = update ? tlas.accel : VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = tlas.accel;
		buildInfo.scratchData.deviceAddress = vktools::getBufferDeviceAddress(devices.device, scratchBuffer);

		//build offset info : n instances
		VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{ static_cast<uint32_t>(instances.size()), 0, 0, 0 };
		const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

		//build the tlas
		vkfp::vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);

		devices.endOneTimeSubmitCommandBuffer(cmdBuf);
		devices.memoryAllocator.freeBufferMemory(stagingBuffer, properties);
		vkDestroyBuffer(devices.device, stagingBuffer, nullptr);
		devices.memoryAllocator.freeBufferMemory(scratchBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkDestroyBuffer(devices.device, scratchBuffer, nullptr);
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
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR); //primitive mesh info

		//create rt descriptor pool & layout
		uint32_t nbRtDescriptorSet = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		rtDescriptorPool = rtDescriptorSetBindings.createDescriptorPool(devices.device, nbRtDescriptorSet);
		rtDescriptorSetLayout = rtDescriptorSetBindings.createDescriptorSetLayout(devices.device);

		//allocate rt descriptor sets
		std::vector<VkDescriptorSetLayout> layout(nbRtDescriptorSet, rtDescriptorSetLayout);
		rtDescriptorSets.assign(nbRtDescriptorSet, {});
		VkDescriptorSetAllocateInfo descInfo{};
		descInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descInfo.descriptorPool = rtDescriptorPool;
		descInfo.descriptorSetCount = nbRtDescriptorSet;
		descInfo.pSetLayouts = layout.data();
		VK_CHECK_RESULT(vkAllocateDescriptorSets(devices.device, &descInfo, rtDescriptorSets.data()));

		//tlas info
		VkWriteDescriptorSetAccelerationStructureKHR descAsInfo{};
		descAsInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		descAsInfo.accelerationStructureCount = 1;
		descAsInfo.pAccelerationStructures = &tlas.accel;

		//primitive info
		VkDescriptorBufferInfo primitiveInfo{ primInfoBuffer, 0, VK_WHOLE_SIZE };

		//update raytrace descriptor sets
		for (uint32_t i = 0; i < nbRtDescriptorSet; ++i) {
			VkDescriptorImageInfo imageInfo{ {}, offscreens[i].offscreenColorBuffer.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

			std::vector<VkWriteDescriptorSet> writes;
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 0, &descAsInfo));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 1, &imageInfo));
			writes.emplace_back(rtDescriptorSetBindings.makeWrite(rtDescriptorSets[i], 2, &primitiveInfo));
			vkUpdateDescriptorSets(devices.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		}
	}

	/*
	* create offscreen images - color & depth images used as destinations of raytrace pipeline
	*/
	OffscreenImages createOffscreenImages() {
		OffscreenImages images{};

		//create offscreen color buffer
		VkImage offscreenColorImage;
		devices.createImage(offscreenColorImage, { swapchain.extent.width,swapchain.extent.height, 1 },
			offscreenColorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkImageView offscreenColorImageView = vktools::createImageView(devices.device, offscreenColorImage,
			VK_IMAGE_VIEW_TYPE_2D, offscreenColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
		VkSampler sampler;
		VkSamplerCreateInfo samplerInfo = vktools::initializers::samplerCreateInfo(
			devices.availableFeatures, devices.properties);
		VK_CHECK_RESULT(vkCreateSampler(devices.device, &samplerInfo, nullptr, &sampler));

		images.offscreenColorBuffer.init(&devices, offscreenColorImage, offscreenColorImageView, VK_IMAGE_LAYOUT_GENERAL, sampler);

		//create offscreen depth buffer
		VkImage offscreenDepthImage;
		devices.createImage(offscreenDepthImage, { swapchain.extent.width,swapchain.extent.height, 1 },
			offscreenDepthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkImageView offscreenDepthImageView = vktools::createImageView(devices.device, offscreenDepthImage,
			VK_IMAGE_VIEW_TYPE_2D, offscreenDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

		images.offscreenDepthBuffer.init(&devices, offscreenDepthImage, offscreenDepthImageView,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		//setting image layout
		VkCommandBuffer cmdBuf = devices.beginOneTimeSubmitCommandBuffer();
		vktools::setImageLayout(cmdBuf, images.offscreenColorBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		vktools::setImageLayout(cmdBuf, images.offscreenDepthBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
		devices.endOneTimeSubmitCommandBuffer(cmdBuf);

		return images;
	}

	/*
	* create offscreen images & render pass & framebuffers
	*/
	void createOffscreenRender() {
		for (auto& offscreen : offscreens) {
			offscreen.offscreenColorBuffer.cleanup();
			offscreen.offscreenDepthBuffer.cleanup();
		}
		offscreens.clear();

		size_t nbImages = static_cast<size_t>(MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < nbImages; ++i) {
			offscreens.push_back(createOffscreenImages());
		}

		//create renderpass for the offscreen
		if (!offscreenRenderPass) {
			offscreenRenderPass = vktools::createRenderPass(devices.device,
				{ offscreenColorFormat },
				offscreenDepthFormat,
				1,
				true,
				true,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_GENERAL);
		}

		//create framebuffers for the offscreen
		if (offscreenFramebuffers.empty()) {
			offscreenFramebuffers.resize(nbImages);
		}
		else {
			for (auto& framebuffer : offscreenFramebuffers) {
				vkDestroyFramebuffer(devices.device, framebuffer, nullptr);
			}
		}
		
		for (size_t i = 0; i < nbImages; ++i) {
			std::array<VkImageView, 2> attachments{ 
				offscreens[i].offscreenColorBuffer.descriptor.imageView,
				offscreens[i].offscreenDepthBuffer.descriptor.imageView 
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass		= offscreenRenderPass;
			framebufferInfo.attachmentCount = 2;
			framebufferInfo.pAttachments	= attachments.data();
			framebufferInfo.width			= swapchain.extent.width;
			framebufferInfo.height			= swapchain.extent.height;
			framebufferInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(devices.device, &framebufferInfo, nullptr, &offscreenFramebuffers[i]));
		}
	}

	/*
	* create general (rasterizer) pipeline
	*/
	void createGeneralPipeline() {
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
		gen.addPushConstantRange({ {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RasterPushConstant)} });
		gen.addShader(vktools::createShaderModule(devices.device, vktools::readFile("shaders/rasterizer_vert.spv")),
			VK_SHADER_STAGE_VERTEX_BIT);
		gen.addShader(vktools::createShaderModule(devices.device, vktools::readFile("shaders/rasterizer_frag.spv")),
			VK_SHADER_STAGE_FRAGMENT_BIT);

		gen.generate(offscreenRenderPass, generalPipeline, generalPipelineLayout);
	}

	/*
	* create (normal) descriptor set - set camera matrix uniform buffer & scene description buffer
	*/
	void createDescriptorSet() {
		uint32_t nbTexture = 0;

		//camera matrix
		descriptorSetBindings.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
		//scene description
		descriptorSetBindings.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
			VK_SHADER_STAGE_VERTEX_BIT |
			VK_SHADER_STAGE_FRAGMENT_BIT | 
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
			VK_SHADER_STAGE_ANY_HIT_BIT_KHR);
		//textures
		/*descriptorSetBindings.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nbTexture,
			VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);*/

		uint32_t nbDescriptorSet = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		descriptorSetLayout = descriptorSetBindings.createDescriptorSetLayout(devices.device);
		descriptorPool = descriptorSetBindings.createDescriptorPool(devices.device, nbDescriptorSet);
		
		VkDescriptorSetAllocateInfo descInfo{};
		std::vector<VkDescriptorSetLayout> layout(nbDescriptorSet, descriptorSetLayout);
		descInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descInfo.descriptorPool = descriptorPool;
		descInfo.descriptorSetCount = nbDescriptorSet;
		descInfo.pSetLayouts = layout.data();
		descriptorSets.resize(nbDescriptorSet);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(devices.device, &descInfo, descriptorSets.data()));

		for (size_t i = 0; i < static_cast<size_t>(MAX_FRAMES_IN_FLIGHT); ++i) {
			VkDescriptorBufferInfo camMatricesInfo{ uniformBuffers[i], 0, sizeof(CameraMatrices) };
			VkDescriptorBufferInfo sceneBufferInfo{ sceneDescBuffer, 0, sizeof(SceneDesc) };

			std::vector<VkWriteDescriptorSet> writes;
			writes.emplace_back(descriptorSetBindings.makeWrite(descriptorSets[i], 0, &camMatricesInfo));
			writes.emplace_back(descriptorSetBindings.makeWrite(descriptorSets[i], 1, &sceneBufferInfo));
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
		VkDescriptorImageInfo imageInfo{ {}, offscreens[currentFrame].offscreenColorBuffer.descriptor.imageView,
			VK_IMAGE_LAYOUT_GENERAL };
		VkWriteDescriptorSet wds = rtDescriptorSetBindings.makeWrite(rtDescriptorSets[currentFrame], 1, &imageInfo);
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
		stage.module = vktools::createShaderModule(devices.device, vktools::readFile("shaders/raytrace_rgen.spv"));
		stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		stages[STAGE_RAYGEN] = stage;
		//miss
		stage.module = vktools::createShaderModule(devices.device, vktools::readFile("shaders/raytrace_rmiss.spv"));
		stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		stages[STAGE_MISS] = stage;
		//shadow miss
		stage.module = vktools::createShaderModule(devices.device, vktools::readFile("shaders/raytrace_shadow_rmiss.spv"));
		stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
		stages[STAGE_SHADOW_MISS] = stage;
		//closest hit
		stage.module = vktools::createShaderModule(devices.device, vktools::readFile("shaders/raytrace_rchit.spv"));
		stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		stages[STAGE_CLOSEST_HIT] = stage;

		//shader groups
		VkRayTracingShaderGroupCreateInfoKHR group{};
		group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		group.anyHitShader			= VK_SHADER_UNUSED_KHR;
		group.closestHitShader		= VK_SHADER_UNUSED_KHR;
		group.generalShader			= VK_SHADER_UNUSED_KHR;
		group.intersectionShader	= VK_SHADER_UNUSED_KHR;

		//raygen
		group.type					= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		group.generalShader			= STAGE_RAYGEN;
		rtShaderGroups.push_back(group);

		//miss
		group.type					= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		group.generalShader			= STAGE_MISS;
		rtShaderGroups.push_back(group);

		//miss
		group.type					= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		group.generalShader			= STAGE_SHADOW_MISS;
		rtShaderGroups.push_back(group);

		//closest hit
		group.type					= VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		group.generalShader			= VK_SHADER_UNUSED_KHR;
		group.closestHitShader		= STAGE_CLOSEST_HIT;
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

		VK_CHECK_RESULT(vkfp::vkCreateRayTracingPipelinesKHR(devices.device, {}, {}, 1, & rayPipelineInfo, nullptr, & rtPipeline));

		for (auto& s : stages) {
			vkDestroyShaderModule(devices.device, s.module, nullptr);
		}
	}

	//TODO: implement sbtwrapper
	/*
	* shader binding table (SBT)
	* 
	* gets all shader handles and write them in a SBT buffer
	*/
	void createRtShaderBindingTable() {
		uint32_t groupCount = static_cast<uint32_t>(rtShaderGroups.size()); // 3 shaders: raygen, miss, chit
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
		gen.generate(postRenderPass, postPipeline, postPipelineLayout);
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
		postDescriptorSets = vktools::allocateDescriptorSets(devices.device, postDescriptorSetLayout,
			postDescriptorPool, nbDescriptorSet);
	}

	/*
	* update post descriptor set - sampler references offscreen color buffer
	*/
	void updatePostDescriptorSet(size_t currentFrame) {
		VkWriteDescriptorSet wd = postDescriptorSetBindings.makeWrite(postDescriptorSets[currentFrame], 0,
			&offscreens[currentFrame].offscreenColorBuffer.descriptor);
		vkUpdateDescriptorSets(devices.device, 1, &wd, 0, nullptr);
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
		uniformBufferMemories[currentFrame].mapData(devices.device, &camera);
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

		rtPushConstants.clearColor = { 0.95f, 0.95f, 0.95f, 1.f };
		rtPushConstants.lightPos = static_cast<Imgui*>(imgui)->userInput.lightPos;

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
	*/
	void rasterize(VkCommandBuffer cmdBuf, size_t resourceIndex) {
		vktools::setViewportScissorDynamicStates(cmdBuf, swapchain.extent);
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, generalPipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, generalPipelineLayout,
			0, 1, &descriptorSets[resourceIndex], 0, nullptr);

		std::vector<VkDeviceSize> offsets = { 0, 0, 0 };
		std::vector<VkBuffer> vertexBuffers = { vertexBuffer , normalBuffer, uvBuffer };
		vkCmdBindVertexBuffers(cmdBuf, 0, static_cast<uint32_t>(vertexBuffers.size()),
			vertexBuffers.data(), offsets.data());
		vkCmdBindIndexBuffer(cmdBuf, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		for (GltfNode& node : cornellBox.m_nodes) {
			GltfPrimMesh& primitive = cornellBox.m_primMeshes[node.primMesh];
			rasterPushConstants.modelMatrix = node.worldMatrix;
			rasterPushConstants.objIndex = node.primMesh;
			rasterPushConstants.materialId = primitive.materialIndex;
			rasterPushConstants.lightPos = static_cast<Imgui*>(imgui)->userInput.lightPos;
			vkCmdPushConstants(cmdBuf, generalPipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(RasterPushConstant), &rasterPushConstants);
			vkCmdDrawIndexed(cmdBuf, primitive.indexCount, 1, primitive.firstIndex, primitive.vertexOffset, 0);
		}
	}
};

//entry point
RUN_APPLICATION_MAIN(VulkanApp, 800, 600, "ray tracing");
