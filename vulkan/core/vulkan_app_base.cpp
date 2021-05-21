#include "vulkan_app_base.h"

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

}

void VulkanAppBase::run() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		update();
	}
}

void VulkanAppBase::cleanup() {
	glfwDestroyWindow(window);
	glfwTerminate();
}
