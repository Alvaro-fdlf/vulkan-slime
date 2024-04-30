#include "vulkanSetup.h"
#include <stdlib.h>
#include <stdio.h>

void vkSetup () {
	VkInstance inst;
	VkApplicationInfo appInfo;
	VkInstanceCreateInfo instInfo;

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = "Slime simulation";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = NULL;
	appInfo.engineVersion = 0;
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pNext = NULL;
	instInfo.flags = 0;
	instInfo.pApplicationInfo = &appInfo;
	instInfo.enabledLayerCount = 0;
	instInfo.ppEnabledLayerNames = NULL;
	instInfo.enabledExtensionCount = 0;
	instInfo.ppEnabledExtensionNames = NULL;

	VkResult result = vkCreateInstance(&instInfo, NULL, &inst);
	if (result != VK_SUCCESS) {
		fprintf(stderr, "Failed to create vulkan instance");
		abort();
	}

	return;
}
