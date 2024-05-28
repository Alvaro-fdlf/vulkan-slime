#include "vulkanSetup.h"
#include "drmMaster.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static VkInstance inst;
static VkPhysicalDevice physDev;
VkSurfaceKHR surface;
VkSwapchainKHR swapchain;
VkExtent2D displayExtent;
static VkDevice dev;
uint32_t queueFamilyCount;
VkQueueFamilyProperties *queueFamilies;
static int fd;

static VkResult result;
#define vkFail(msg) \
	if (result != VK_SUCCESS) {\
		fprintf(stderr, (msg)); \
		abort(); \
	}

static void printPhysicalDevicesInfo(uint32_t devCount, VkPhysicalDevice *devs) {
	printf("Enumerationg devices available\n");
	for (unsigned int i=0; i<devCount; i++) {
		VkPhysicalDeviceProperties props;
		VkPhysicalDeviceMemoryProperties memProps;

		vkGetPhysicalDeviceProperties(devs[i], &props);
		printf("--------------\n");
		printf("Name: %s, API version: %u\n", props.deviceName, props.apiVersion);

		vkGetPhysicalDeviceMemoryProperties(devs[i], &memProps);
		printf("Memory type count: %u, memory heap count: %u\n", memProps.memoryTypeCount, memProps.memoryHeapCount);

		for (unsigned int j=0; j<memProps.memoryTypeCount; j++) {
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

		for (unsigned int j=0; j<queueFamilyCount; j++) {
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

static void createInstance() {
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

	// Need presentation extensions, specifically for full screen with DRM KMS on Linux
	const char * const extNames[] = {
		"VK_KHR_surface",
		"VK_KHR_display",
		"VK_EXT_direct_mode_display",
		"VK_EXT_acquire_drm_display"
	};
	instInfo.enabledExtensionCount = sizeof(extNames) / sizeof(char *);
	instInfo.ppEnabledExtensionNames = extNames;

	// Enumerate layers and enable all of them in the instance
	uint32_t layerCount;
	result = vkEnumerateInstanceLayerProperties(&layerCount, NULL);
	vkFail("Failed to get amount of available layers\n");
	VkLayerProperties layers[layerCount];
	result = vkEnumerateInstanceLayerProperties(&layerCount, layers);

	printf("There are %d layers to enable:\n", layerCount);
	instInfo.enabledLayerCount = layerCount;
	char **names = (char**) malloc(layerCount * sizeof(char *));
	for (unsigned int i=0; i<layerCount; i++) {
		names[i] = layers[i].layerName;
		printf("%s: %s\n", layers[i].layerName, layers[i].description);
		
	}
	instInfo.ppEnabledLayerNames = (const char * const *) names;
	printf("\n");

	result = vkCreateInstance(&instInfo, NULL, &inst);
	vkFail("Failed to create vulkan instance\n");

	free(names);
}


VkResult (*vkGetDrmDisplay) (VkPhysicalDevice physicalDevice, int32_t drmFd, uint32_t connectorId, VkDisplayKHR *display);
VkResult (*vkAcquireDrmDisplay) (VkPhysicalDevice physicalDevice, int32_t fd, VkDisplayKHR display);
VkResult (*vkGetPhysicalDeviceDisplayPlaneProperties) (VkPhysicalDevice physicalDevice,
							uint32_t *pPropertyCount,
							VkDisplayPlanePropertiesKHR *pProperties);
VkResult (*vkGetDisplayPlaneSupportedDisplays) (VkPhysicalDevice physicalDevice,
						uint32_t planeIndex,
						uint32_t *pDisplayCount,
						VkDisplayKHR *pDisplays);
VkResult (*vkGetDisplayModeProperties) (VkPhysicalDevice physicalDevice,
					VkDisplayKHR display,
					uint32_t *pPropertyCount,
					VkDisplayModePropertiesKHR *pProperties);
VkResult (*vkCreateDisplayPlaneSurface) (VkInstance instance,
					const VkDisplaySurfaceCreateInfoKHR *pCreateInfo,
					const VkAllocationCallbacks *pAllocator,
					VkSurfaceKHR *pSurface);
VkResult (*vkGetPhysicalDeviceSurfaceFormats) (VkPhysicalDevice physicalDevice,
						VkSurfaceKHR surface,
						uint32_t *pSurfaceFormatCount,
						VkSurfaceFormatKHR *pSurfaceFormats);
VkResult (*vkCreateSwapchain) (VkDevice device,
				const VkSwapchainCreateInfoKHR *pCreateInfo,
				const VkAllocationCallbacks *pAllocator,
				VkSwapchainKHR *pSwapchain);
void getExtensionFunctions() {
	vkGetDrmDisplay = vkGetInstanceProcAddr(inst, "vkGetDrmDisplayEXT");
	vkAcquireDrmDisplay = vkGetInstanceProcAddr(inst, "vkAcquireDrmDisplayEXT");
	vkGetPhysicalDeviceDisplayPlaneProperties = vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
	vkGetDisplayPlaneSupportedDisplays = vkGetInstanceProcAddr(inst, "vkGetDisplayPlaneSupportedDisplaysKHR");
	vkGetDisplayModeProperties = vkGetInstanceProcAddr(inst, "vkGetDisplayModePropertiesKHR");
	vkCreateDisplayPlaneSurface = vkGetInstanceProcAddr(inst, "vkCreateDisplayPlaneSurfaceKHR");
	vkGetPhysicalDeviceSurfaceFormats = vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	vkCreateSwapchain = vkGetInstanceProcAddr(inst, "vkCreateSwapchainKHR");
}

void createLogicalDevice() {
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
	vkGetPhysicalDeviceQueueFamilyProperties(devs[devInd], &queueFamilyCount, NULL);
	queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(devs[devInd], &queueFamilyCount, queueFamilies);

	VkDeviceQueueCreateInfo queueInfos[queueFamilyCount];
	for (unsigned int i=0; i<queueFamilyCount; i++) {
		queueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfos[i].pNext = NULL;
		queueInfos[i].flags = 0;
		queueInfos[i].queueFamilyIndex = i;
		queueInfos[i].queueCount = queueFamilies[i].queueCount;
		float *priorities = (float *) malloc(queueInfos[i].queueCount * sizeof(float));
		for (unsigned int j=0; j<queueInfos[i].queueCount; j++) {
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
	const char * const extNames[] = {
		"VK_KHR_swapchain"
	};
	devInfo.enabledExtensionCount = sizeof(extNames) / sizeof(char *);
	devInfo.ppEnabledExtensionNames = extNames;

	devInfo.pEnabledFeatures = &requiredFeatures;

	result = vkCreateDevice(devs[devInd], &devInfo, NULL, &dev);
	vkFail("Failed to create logical device\n");
	physDev = devs[devInd];

	for (unsigned int i=0; i<queueFamilyCount; i++) {
		free((void *) queueInfos[i].pQueuePriorities);
	}
}

void createDisplaySurface(int monitorIndex) {
	getConnectorWithCrtc(monitorIndex);
	VkDisplayKHR display;
	result = vkGetDrmDisplay(physDev, fd, connector->connector_id, &display);
	vkFail("Failed to get DRM display\n");

	result = vkAcquireDrmDisplay(physDev, fd, display);
	vkFail("Failed to get permission to display to the DRM display\n");

	// Enumerate planes, then choose one that connects to the already chosen display
	// It seems like a vulkan plane is not a DRM plane, but a DRM CRTC.
	// When checking DRM objects I see 3 CRTCs but more than 3 planes, but
	// on the tty screen vulkan says there are 3 planes. Also on X with the lease it says
	// there's 1 plane, so hopefully it's considering only the leased CRTC thanks to some previous function
	uint32_t planeCount;
	result = vkGetPhysicalDeviceDisplayPlaneProperties(physDev, &planeCount, NULL);
	vkFail("Failed to get plane count");
	/*VkDisplayPlanePropertiesKHR planeProps[planeCount];
	result = vkGetPhysicalDeviceDisplayPlaneProperties(physDev, &planeCount, planeProps);
	vkFail("Failed to get plane properties");*/

	uint32_t planeIndex;
	int foundPlane = 0;
	for (planeIndex=0; planeIndex<planeCount; planeIndex++) {
		uint32_t displayCount;
		result = vkGetDisplayPlaneSupportedDisplays(physDev, planeIndex, &displayCount, NULL);
		if (result != VK_SUCCESS)
			continue;
		VkDisplayKHR displays[displayCount];
		result = vkGetDisplayPlaneSupportedDisplays(physDev, planeIndex, &displayCount, displays);
		if (result != VK_SUCCESS)
			continue;

		for (uint32_t j=0; j<displayCount; j++) {
			if (displays[j] == display) {
				foundPlane = 1;
				break;
			}
		}
		if (foundPlane)
			break;
	}
	if (!foundPlane) {
		fprintf(stderr, "Couldn't find a suitable plane (crtc?) for the display");
		abort();
	}

	// Choose a mode, the first one should be the best
	VkDisplayModeKHR displayMode;
	uint32_t modeCount;
	result = vkGetDisplayModeProperties(physDev, display, &modeCount, NULL);
	vkFail("Failed to get mode count");
	VkDisplayModePropertiesKHR modes[modeCount];
	result = vkGetDisplayModeProperties(physDev, display, &modeCount, modes);
	vkFail("Failed to get mode properties");
	int modeIdx = 0;
	displayMode = modes[modeIdx].displayMode;
	printf("Chosen mode: %ux%u, %f fps\n", modes[modeIdx].parameters.visibleRegion.width,
						modes[modeIdx].parameters.visibleRegion.height,
						modes[modeIdx].parameters.refreshRate / 1000.0);

	VkDisplaySurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.pNext = NULL;
	surfaceCreateInfo.flags = 0;
	surfaceCreateInfo.displayMode = displayMode;
	surfaceCreateInfo.planeIndex = planeIndex;
	surfaceCreateInfo.planeStackIndex = 0;
	surfaceCreateInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	surfaceCreateInfo.globalAlpha = 0.0;
	surfaceCreateInfo.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR; // no alpha
	displayExtent = modes[modeIdx].parameters.visibleRegion;
	surfaceCreateInfo.imageExtent = displayExtent;

	result = vkCreateDisplayPlaneSurface(inst, &surfaceCreateInfo, NULL, &surface);
	vkFail("Failed to create surface to display\n");
}

void createSwapchain() {
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormats(physDev, surface, &formatCount, NULL);
	VkSurfaceFormatKHR formats[formatCount];
	vkGetPhysicalDeviceSurfaceFormats(physDev, surface, &formatCount, formats);
	int formatIndex = -1;
	for (uint32_t i=0; i<formatCount; i++) {
		if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
			formatIndex = i;
			break;
		}
	}
	if (formatIndex == -1) {
		fprintf(stderr, "Couldn't choose rgba or bgra formats for the swapchain\n");
		abort();
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = NULL;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = 2;
	swapchainCreateInfo.imageFormat = formats[formatIndex].format;
	swapchainCreateInfo.imageColorSpace = formats[formatIndex].colorSpace;
	swapchainCreateInfo.imageExtent = displayExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0; // ignored because exclusive
	swapchainCreateInfo.pQueueFamilyIndices = NULL;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = 0;

	vkCreateSwapchain(dev, &swapchainCreateInfo, NULL, &swapchain);
	vkFail("Failed to create a swapchain\n");
}

void vkSetup(int monitorIndex) {
	int isLeased;
	// Do this now, drmIsMaster(fd) may returns false otherwise
	fd = getDrmMasterFd(monitorIndex, &isLeased);

	createInstance();
	getExtensionFunctions();
	createLogicalDevice();

	if (isLeased)
		monitorIndex = 0;
	createDisplaySurface(monitorIndex);
	createSwapchain();

	return;
}

void vkCleanup() {
	vkDeviceWaitIdle(dev);
	vkDestroySwapchainKHR(dev, swapchain, NULL);
	vkDestroyDevice(dev, NULL);
	vkDestroySurfaceKHR(inst, surface, NULL);
	vkDestroyInstance(inst, NULL);

	close(fd);
}
