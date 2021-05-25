#pragma once
#include "utils.h"
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
	void cleanup();

	//application info
	GLFWwindow* window;
	int width, height;
	std::string appName;

	//extensions
	std::vector<const char*> enabledInstanceExtensions;
	std::vector<const char*> enabledDeviceExtensions;

protected:
	VkInstance instance;
private:
	void createInstance();
};

//entry point
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
