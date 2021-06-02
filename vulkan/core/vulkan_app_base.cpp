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
	swapchain.cleanup();
	devices.cleanup();
	vkDestroySurfaceKHR(instance, surface, nullptr);
	destroyDebugUtilsMessengerEXT(instance, nullptr);
	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
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
* called every frame - may contain update & draw functions
*/
void VulkanAppBase::run() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		update();
	}
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

