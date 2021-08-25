#include "core/vulkan_app_base.h"
#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

class VulkanApp : public VulkanAppBase {
public:
	/*
	* constructor - get window size & title
	*/
	VulkanApp(int width, int height, const std::string& appName)
		: VulkanAppBase(width, height, appName) {}

	/*
	* destructor - destroy vulkan objects created in this level
	*/
	~VulkanApp() {

	}

	/*
	* application initialization - also contain base class initApp()
	*/
	virtual void initApp() override {

	}

	/*
	* draw
	*/
	virtual void draw() override {

	}

	/*
	* create framebuffers
	*/
	virtual void createFramebuffers() override {

	}

	/*
	* record command buffer
	*/
	virtual void recordCommandBuffer() override {

	}

private:
};

//entry point
RUN_APPLICATION_MAIN(VulkanApp, 800, 600, "app_name");
