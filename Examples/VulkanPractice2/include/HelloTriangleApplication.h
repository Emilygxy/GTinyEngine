#pragma once
#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>

class HelloTriangleApp
{
public:
	HelloTriangleApp();
	~HelloTriangleApp();

	void run();

private:
	void initVulkan();
	void mainLoop();
	void cleanUp();
};

