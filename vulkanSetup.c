#include "vulkanSetup.h"
#include <stdlib.h>
#include <stdio.h>

static VkResult result;
#define vkFail(msg) \
	if (result != VK_SUCCESS) {\
		fprintf(stderr, (msg)); \
		abort(); \
	}

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
	vkFail("Failed to create vulkan instance\n");

	// Enumerate physical devices
	uint32_t devCount;
	result = vkEnumeratePhysicalDevices(inst, &devCount, NULL);
	vkFail("Failed to get amount of physical devices\n");

	printf("This computer has %d Vulkan compatible devices\n", devCount);
	if (devCount == 0)
		abort();

	VkPhysicalDevice devs[devCount];
	result = vkEnumeratePhysicalDevices(inst, &devCount, devs);
	vkFail("Failed to enumerate physical devices\n");
	printf("Enumerationg devices available\n");
	for (int i=0; i<devCount; i++) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devs[i], &props);
		printf("Name: %s, API version: %u\n", props.deviceName, props.apiVersion);
	}

	return;
}
