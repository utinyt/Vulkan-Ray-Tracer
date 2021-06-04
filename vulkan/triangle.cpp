#include "core/vulkan_app_base.h"

class VulkanApp : public VulkanAppBase {
public:
	VulkanApp(int width, int height, const std::string& appName)
		: VulkanAppBase(width, height, appName) {}

private:
	virtual void update() override {
		//std::cout << "Hello\n"; 
	}
};

//entry point
RUN_APPLICATION_MAIN(VulkanApp, 800, 600, "vkApp");
