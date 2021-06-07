#include <fstream>
#include "vulkan_app_base.h"
#include "vulkan_debug.h"

#ifdef NDEBUG
const bool enableValidationLayer = false;
#else
const bool enableValidationLayer = true;
#endif

/*
* app constructor
* 
* @param width - window width
* @param height - window height
* @param appName - application title
*/
VulkanAppBase::VulkanAppBase(int width, int height, const std::string& appName)
	: width(width), height(height), appName(appName) {
	enabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

/*
* app destructor
*/
VulkanAppBase::~VulkanAppBase() {
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroySemaphore(devices.device, presentCompleteSemaphores[i], nullptr);
		vkDestroySemaphore(devices.device, renderCompleteSemaphores[i], nullptr);
		vkDestroyFence(devices.device, frameLimitFences[i], nullptr);
	}

	swapchain.cleanup();

	vkDestroyPipelineCache(devices.device, pipelineCache, nullptr);
	destroyCommandBuffers();
	vkDestroyCommandPool(devices.device, commandPool, nullptr);

	devices.cleanup();
	vkDestroySurfaceKHR(instance, surface, nullptr);
	destroyDebugUtilsMessengerEXT(instance, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

/*
* init program - window & vulkan & application
*/
void VulkanAppBase::init() {
	initWindow();
	initVulkan();
	initApp();
}

/*
* called every frame - may contain update & draw functions
*/
void VulkanAppBase::run() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		draw();
	}
	vkDeviceWaitIdle(devices.device);
}

/*
* glfw window initialization
*/
void VulkanAppBase::initWindow() {
	if (glfwInit() == GLFW_FALSE) {
		throw std::runtime_error("failed to init GLFW");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(width, height, appName.c_str(), nullptr, nullptr);
	LOG("initialized:\tglfw");
}

/*
* vulkan setup
*/
void VulkanAppBase::initVulkan() {
	//instance
	createInstance();

	//debug messenger
	if (enableValidationLayer) {
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		setupDebugMessengerCreateInfo(debugCreateInfo);
		VK_CHECK_RESULT(createDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr));
		LOG("created:\tdebug utils messenger");
	}

	//surface
	VK_CHECK_RESULT(glfwCreateWindowSurface(instance, window, nullptr, &surface));
	LOG("created:\tsurface");

	//physical & logical device
	devices.pickPhysicalDevice(instance, surface, enabledDeviceExtensions);
	devices.createLogicalDevice();

	swapchain.init(&devices, window);
	swapchain.create();
}

/*
* basic application setup
*/
void VulkanAppBase::initApp() {
	createCommandPool();
	createCommandBuffers();
	createSyncObjects();
	createPipelineCache();

	/*createRenderPass();
	createFramebuffers();*/
}

/*
* helper function - creates vulkan instance
*/
void VulkanAppBase::createInstance() {
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName.c_str();
	appInfo.pEngineName = appName.c_str();
	appInfo.apiVersion = VK_API_VERSION_1_2;

	VkInstanceCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledLayerCount = 0;
	instanceInfo.ppEnabledLayerNames = nullptr;

	//vailidation layer settings
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (enableValidationLayer) {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		const char* requiredValidationLayer = "VK_LAYER_KHRONOS_validation";

		// -- support check --
		auto layerIt = std::find_if(availableLayers.begin(), availableLayers.end(),
			[&requiredValidationLayer](const VkLayerProperties& properties) {
				return strcmp(properties.layerName, requiredValidationLayer) == 0;
			});

		if (layerIt == availableLayers.end()) {
			throw std::runtime_error("valiation layer is not supported");
		}

		instanceInfo.enabledLayerCount = 1;
		instanceInfo.ppEnabledLayerNames = &requiredValidationLayer;
		setupDebugMessengerCreateInfo(debugCreateInfo);
		instanceInfo.pNext = &debugCreateInfo;
	}

	//instance extension settings
	// -- GLFW extensions --
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> requiredInstanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	// -- required extensions specified by user --
	requiredInstanceExtensions.insert(requiredInstanceExtensions.end(),
		enabledInstanceExtensions.begin(), enabledInstanceExtensions.end());
	if (enableValidationLayer) {
		requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	uint32_t availableInstanceExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, nullptr);
	std::vector<VkExtensionProperties> availableInstanceExtensions(availableInstanceExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionCount, availableInstanceExtensions.data());

	// -- support check --
	for (auto& requiredEXT : requiredInstanceExtensions) {
		auto extensionIt = std::find_if(availableInstanceExtensions.begin(), availableInstanceExtensions.end(),
			[&requiredEXT](const VkExtensionProperties& properties) {
				return strcmp(requiredEXT, properties.extensionName) == 0;
			});

		if (extensionIt == availableInstanceExtensions.end()) {
			throw std::runtime_error(std::string(requiredEXT)+" instance extension is not supported");
		}
	}

	instanceInfo.enabledExtensionCount = static_cast<uint32_t>(requiredInstanceExtensions.size());
	instanceInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();

	VK_CHECK_RESULT(vkCreateInstance(&instanceInfo, nullptr, &instance));
	LOG("created:\tvulkan instance");
}

/*
* create command pool - use no flags
*/
void VulkanAppBase::createCommandPool() {
	const VulkanDevice::QueueFamilyIndices indices = devices.indices;

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = indices.graphicsFamily.value();
	poolInfo.flags = 0;

	VK_CHECK_RESULT(vkCreateCommandPool(devices.device, &poolInfo, nullptr, &commandPool));
	LOG("created:\tcommand pool");
}

/*
* allocate empty command buffers
*/
void VulkanAppBase::createCommandBuffers() {
	commandBuffers.resize(swapchain.imageCount);

	VkCommandBufferAllocateInfo commandBufferInfo{};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.commandPool = commandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	VK_CHECK_RESULT(vkAllocateCommandBuffers(devices.device, &commandBufferInfo, commandBuffers.data()));
	LOG("created:\tcommand buffers");
}

/*
* helper function - free command buffers
*/
void VulkanAppBase::destroyCommandBuffers() {
	vkFreeCommandBuffers(devices.device, commandPool,
		static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
}

/*
* create semaphore & fence
*/
void VulkanAppBase::createSyncObjects() {
	presentCompleteSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderCompleteSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frameLimitFences.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightImageFences.resize(swapchain.imageCount, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VK_CHECK_RESULT(vkCreateSemaphore(devices.device, &semaphoreInfo, nullptr, &presentCompleteSemaphores[i]));
		VK_CHECK_RESULT(vkCreateSemaphore(devices.device, &semaphoreInfo, nullptr, &renderCompleteSemaphores[i]));
		VK_CHECK_RESULT(vkCreateFence(devices.device, &fenceInfo, nullptr, &frameLimitFences[i]));
	}
	LOG("created:\tsync objects");
}

/*
* create pipeline cache to optimize subsequent pipeline creation
*/
void VulkanAppBase::createPipelineCache() {
	VkPipelineCacheCreateInfo pipelineCacheInfo{};
	pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(devices.device, &pipelineCacheInfo, nullptr, &pipelineCache));
	LOG("created:\tpipeline cache");
}

/*
* create shader module
* 
* @param code - compiled shader code (raw binary data, .spv)
*/
VkShaderModule VulkanAppBase::createShaderModule(const std::vector<char>& code) {
	VkShaderModuleCreateInfo shaderModuleInfo{};
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.codeSize = code.size();
	shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	VK_CHECK_RESULT(vkCreateShaderModule(devices.device, &shaderModuleInfo, nullptr, &shaderModule));
	return shaderModule;
}
