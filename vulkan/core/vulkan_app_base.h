#pragma once
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "GLFW/glfw3.h"

class VulkanAppBase {
public:
	VulkanAppBase(int width, int height, const std::string& appName);
	virtual ~VulkanAppBase();

	void initWindow();
	void initVulkan();
	void run();

protected:
	virtual void update() = 0;

	/** glfw window handle */
	GLFWwindow* window;
	/** window extent */
	int width, height;
	/** application title */
	std::string appName;
	/** list of enalbed (required) instance extensions */
	std::vector<const char*> enabledInstanceExtensions;
	/** list of enalbed (required) device extensions */
	std::vector<const char*> enabledDeviceExtensions;
	/** vulkan instance */
	VkInstance instance;
	/** abstracted handle to the native platform surface */
	VkSurfaceKHR surface;
	/** contains physical & logical device handles, device specific info */
	VulkanDevice devices;
	/** abstracted swapchain object - contains swapchain image views */
	VulkanSwapchain swapchain;

private:
	void createInstance();
};

/*
* entry point macro
*/
#define RUN_APPLICATION_MAIN(Application, WIDTH, HEIGHT, appName)	\
int main(void) {													\
	try {															\
		Application app(WIDTH, HEIGHT, appName);					\
		app.initWindow();											\
		app.initVulkan();											\
		app.run();													\
	}										\
	catch (const std::exception& e) {		\
		std::cerr << e.what() << std::endl;	\
		return EXIT_FAILURE;				\
	}										\
	return EXIT_SUCCESS;					\
}											\
