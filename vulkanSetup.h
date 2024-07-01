#include <vulkan/vulkan.h>

extern uint32_t qFamGraphicsIndex, qFamComputeIndex, qFamTransferIndex;
extern VkSwapchainKHR swapchain;
extern uint32_t imgCount;
extern VkImage *images;
extern VkDevice dev;

extern VkQueue graphicsQueue, computeQueue, transferQueue;
extern uint32_t screenWidth, screenHeight, refreshRate;

extern VkBuffer vertexBuf;
extern VkImage frontImg, backImg;

extern VkPipelineLayout computePipelineLayout, graphicsPipelineLayout;
extern VkPipeline computePipeline, graphicsPipeline;
extern VkFramebuffer backFb, frontFb;
extern VkRenderPass renderPass;
extern VkDescriptorSet compBackToFront, compFrontToBack, graphicsBack, graphicsFront;

extern VkCommandPool computePool, graphicsPool, transferPool;
extern VkSemaphore commandSem;
extern VkFence swapFence, commandFence1, commandFence2;

void vkSetup(int monitorIndex);
void vkCleanup();

extern VkResult (*vkQueuePresent) (VkQueue queue, const VkPresentInfoKHR *pPresentInfo);
