#include "vulkanSetup.h"
#include <stdlib.h>
#include <stdio.h>

static VkInstance inst;
static VkDevice dev;

static VkResult result;
#define vkFail(msg) \
	if (result != VK_SUCCESS) {\
		fprintf(stderr, (msg)); \
		abort(); \
	}

static void printPhysicalDevicesInfo(uint32_t devCount, VkPhysicalDevice *devs) {
	printf("Enumerationg devices available\n");
	for (int i=0; i<devCount; i++) {
		VkPhysicalDeviceProperties props;
		VkPhysicalDeviceMemoryProperties memProps;

		vkGetPhysicalDeviceProperties(devs[i], &props);
		printf("--------------\n");
		printf("Name: %s, API version: %u\n", props.deviceName, props.apiVersion);

		vkGetPhysicalDeviceMemoryProperties(devs[i], &memProps);
		printf("Memory type count: %u, memory heap count: %u\n", memProps.memoryTypeCount, memProps.memoryHeapCount);

		for (int j=0; j<memProps.memoryTypeCount; j++) {
			VkMemoryType *type = memProps.memoryTypes + j;
			VkMemoryHeap *heap = memProps.memoryHeaps + type->heapIndex;

			printf("Heap for type %d: Index %d, Size %ldMB\n", j, type->heapIndex, heap->size / 1000000);

			printf("Memory type %d properties: ", j);
			if (type->propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				printf("DEVICE_LOCAL ");
			if (type->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
				printf("HOST_VISIBLE ");
			if (type->propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
				printf("HOST_COHERENT ");
			if (type->propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
				printf("HOST_CACHED ");
			if (type->propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
				printf("LAZILY_ALLOCATED ");
			printf("\n\n");
		}

		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &queueFamilyCount, NULL);
		VkQueueFamilyProperties queueFamilies[queueFamilyCount];
		vkGetPhysicalDeviceQueueFamilyProperties(devs[i], &queueFamilyCount, queueFamilies);
		printf("Queue family count: %u\n", queueFamilyCount);

		for (int j=0; j<queueFamilyCount; j++) {
			VkQueueFamilyProperties *qFam = queueFamilies + j;
			printf("Queue family %d\n", j);
			printf("Queue count: %d\n", qFam->queueCount);
			printf("Queue properties: ");
			if (qFam->queueFlags & VK_QUEUE_GRAPHICS_BIT)
				printf("GRAPHICS ");
			if (qFam->queueFlags & VK_QUEUE_COMPUTE_BIT)
				printf("COMPUTE ");
			if (qFam->queueFlags & VK_QUEUE_TRANSFER_BIT)
				printf("TRANSFER ");
			if (qFam->queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
				printf("SPARSE_BINDING ");
			printf("\n\n");
		}
	}
	printf("--------------\n");
}

void vkSetup() {
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

	// Enumerate layers and enable all of them in the instance
	uint32_t layerCount;
	result = vkEnumerateInstanceLayerProperties(&layerCount, NULL);
	vkFail("Failed to get amount of available layers\n");
	VkLayerProperties layers[layerCount];
	result = vkEnumerateInstanceLayerProperties(&layerCount, layers);

	printf("There are %d layers to enable:\n", layerCount);
	instInfo.enabledLayerCount = layerCount;
	char **names = (char**) malloc(layerCount * sizeof(char *));
	for (int i=0; i<layerCount; i++) {
		names[i] = layers[i].layerName;
		printf("%s: %s\n", layers[i].layerName, layers[i].description);
		
	}
	instInfo.ppEnabledLayerNames = (const char * const *) names;
	printf("\n");

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
	printPhysicalDevicesInfo(devCount, devs);

	// Create logical device
	int devInd = 0;
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(devs[devInd], &queueFamilyCount, NULL);
	VkQueueFamilyProperties queueFamilies[queueFamilyCount];
	vkGetPhysicalDeviceQueueFamilyProperties(devs[devInd], &queueFamilyCount, queueFamilies);

	VkDeviceQueueCreateInfo queueInfos[queueFamilyCount];
	for (int i=0; i<queueFamilyCount; i++) {
		queueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfos[i].pNext = NULL;
		queueInfos[i].flags = 0;
		queueInfos[i].queueFamilyIndex = i;
		queueInfos[i].queueCount = queueFamilies[i].queueCount;
		float *priorities = (float *) malloc(queueInfos[i].queueCount * sizeof(float));
		for (int j=0; j<queueInfos[i].queueCount; j++) {
			priorities[j] = 1.0; // all same priority
		}
		queueInfos[i].pQueuePriorities = priorities;
	}

	// Add features from supportedFeatures to requiredFeatures to enable them
	VkPhysicalDeviceFeatures supportedFeatures, requiredFeatures = {};
	vkGetPhysicalDeviceFeatures(devs[devInd], &supportedFeatures);

	VkDeviceCreateInfo devInfo;
	devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devInfo.pNext = NULL;
	devInfo.flags = 0;
	devInfo.queueCreateInfoCount = queueFamilyCount;
	devInfo.pQueueCreateInfos = queueInfos;
	devInfo.enabledLayerCount = 0;
	devInfo.ppEnabledLayerNames = NULL;
	devInfo.enabledExtensionCount = 0;
	devInfo.ppEnabledExtensionNames = NULL;
	devInfo.pEnabledFeatures = &requiredFeatures;

	result = vkCreateDevice(devs[devInd], &devInfo, NULL, &dev);
	vkFail("Failed to create logical device\n");

	return;
}
