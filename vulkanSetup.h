#include <vulkan/vulkan.h>

extern VkDevice dev;

extern uint32_t screenWidth, screenHeight, refreshRate;

extern VkBuffer vertexBuf;

extern VkPipelineLayout computePipelineLayout, graphicsPipelineLayout;
extern VkPipeline computePipeline, graphicsPipeline;
extern VkFramebuffer backFb, frontFb;
extern VkRenderPass renderPass;
extern VkDescriptorSet compBackToFront, compFrontToBack, graphicsBack, graphicsFront;

extern VkCommandPool computePool, graphicsPool;

void vkSetup(int monitorIndex);
void vkCleanup();
