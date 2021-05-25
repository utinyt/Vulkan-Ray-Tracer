#include "vulkan_app_base.h"
#include "vulkan_debug.h"

#ifdef NDEBUG
const bool enableValidationLayer = false;
#else
const bool enableValidationLayer = true;
#endif

VulkanAppBase::VulkanAppBase(int w, int h, const std::string& name)
	: width(w), height(h), appName(name) {
}

VulkanAppBase::~VulkanAppBase() {
	cleanup();
}

void VulkanAppBase::initWindow() {
	if (glfwInit() == GLFW_FALSE) {
		throw std::runtime_error("failed to init GLFW");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(width, height, appName.c_str(), nullptr, nullptr);
}

void VulkanAppBase::initVulkan() {
	createInstance();

	if (enableValidationLayer) {
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		setupDebugMessengerCreateInfo(debugCreateInfo);
		VK_CHECK_RESULT(createDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr));
	}

}

void VulkanAppBase::run() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		update();
	}
}

void VulkanAppBase::cleanup() {
	destroyDebugUtilsMessengerEXT(instance, nullptr);
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();
}

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
	for (size_t i = 0; i < requiredInstanceExtensions.size(); ++i) {
		auto extensionIt = std::find_if(availableInstanceExtensions.begin(), availableInstanceExtensions.end(),
			[&i, &requiredInstanceExtensions](const VkExtensionProperties& properties) {
				return strcmp(requiredInstanceExtensions[i], properties.extensionName) == 0;
			});

		if (extensionIt == availableInstanceExtensions.end()) {
			throw std::runtime_error(std::string(requiredInstanceExtensions[i])+" instance extension is not supported");
		}
	}

	instanceInfo.enabledExtensionCount = static_cast<uint32_t>(requiredInstanceExtensions.size());
	instanceInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();

	VK_CHECK_RESULT(vkCreateInstance(&instanceInfo, nullptr, &instance));
}
