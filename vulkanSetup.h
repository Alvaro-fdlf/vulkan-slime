#include <vulkan/vulkan.h>

extern VkDevice dev;

extern uint32_t screenWidth, screenHeight, refreshRate;

extern VkBuffer vertexBuf;

extern VkPipelineLayout graphicsPipelineLayout;
extern VkPipeline graphicsPipeline;
extern VkFramebuffer backFb, frontFb;
extern VkRenderPass renderPass;
extern VkDescriptorSet graphicsBack, graphicsFront;

extern VkCommandPool graphicsPool;

void vkSetup(int monitorIndex);
void vkCleanup();
