#include <stddef.h>
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <math.h>
#include <signal.h>
#include "dumbBuffers.h"
#include "vulkanSetup.h"

/*
 * VARIABLES TO MODIFY BEHAVIOR AT COMPILE TIME GO HERE
 */
int useVulkan = 1;
const int monitorIndex = 0; // Maybe make it command line option later
const int particleCount = 200000;
double particleSpeed = 5.0; // distance traveled per frame
double steerAmplitude = M_PI * 0.16; // Angle of field of vision of particle and how much it steers in one frame
int steerLength = 25; // How many steps away to look for pixels to steer towards
double maxRandRadianChange = M_PI * 0.08; // Maximum random change of angle (in radians) per frame on top of the steering
uint32_t particleColor = 0x0060FFE0; // XRGB
uint8_t redFade = 0x1, greenFade = 0x3, blueFade = 0x1; // change these to get different effects
const unsigned int blurKernel[9] = {4, 2, 4, // can be not const, but blur is the bottleneck and it makes a bit of a difference
				2, 1, 2,
				4, 2, 4};
unsigned int blurDivide = 25; // should be set to the sum of elements of blurkernel
/*
 *
 */
// Doesn't actually change the compute shader behavior, just SHOULD be changed to reflect it
const int localGroupSize = 4*4*8;
const int particlesPerInvocation = 1;
const int particlesPerGroup = particlesPerInvocation * localGroupSize;


#define pixel(x, y) ((y)*xSize + (x))
#define min(x, y) ({ \
	__typeof__ (x) x2 = (x); \
	__typeof__ (y) y2 = (y); \
	(x2 < y2) ? x2 : y2; \
})
#define max(x, y) ({ \
	__typeof__ (x) x2 = (x); \
	__typeof__ (y) y2 = (y); \
	(x2 > y2) ? x2 : y2; \
})
#define swap(x, y) {\
	__typeof__ (x) temp = x; \
	x = y; \
	y = temp; \
}

typedef struct {
	double posX, posY, dirX, dirY, angle;
} particle;
particle *particles;
uint32_t *tempBuf1, *tempBuf2; // copying from frontBuf to backBuf is slower than from a usual tempBuf to backBuf (why?)

unsigned long long getMicros();
void draw(uint32_t *buf);
void genParticle(particle *p);

// SIGINT is the normal way this program terminates, so make sure atexit() functions can clean up
void sigintHandler(int _) {
	exit(0);
}

void cleanUpOtherBuffers() {
	free(particles);
	free(tempBuf1);
	free(tempBuf2);
}

int main(int argc, char *argv[]) {
	struct sigaction sigact;
	sigact.sa_handler = sigintHandler;
	sigact.sa_flags = 0;
	sigemptyset(&sigact.sa_mask);
	if (sigaction(SIGINT, &sigact, NULL))
		abort();

	if (useVulkan) {
		vkSetup(monitorIndex);

		VkCommandBuffer graphicsBackToFrontBuf, graphicsFrontToBackBuf,
				computeBackToFrontBuf, computeFrontToBackBuf,
				transferBuf,
				setupBuf;

		// Set up graphics commands
		VkCommandBufferAllocateInfo commandBufferInfo;
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferInfo.pNext = NULL;
		commandBufferInfo.commandPool = graphicsPool;
		commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferInfo.commandBufferCount = 1;

		VkCommandBufferBeginInfo commandBufferBeginInfo;
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.pNext = NULL;
		commandBufferBeginInfo.flags = 0;
		commandBufferBeginInfo.pInheritanceInfo = NULL;

		VkRenderPassBeginInfo renderpassBeginInfo;
		renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpassBeginInfo.pNext = NULL;
		renderpassBeginInfo.renderPass = renderPass;
		renderpassBeginInfo.renderArea.offset.x = 0;
		renderpassBeginInfo.renderArea.offset.y = 0;
		renderpassBeginInfo.renderArea.extent.width = screenWidth;
		renderpassBeginInfo.renderArea.extent.height = screenHeight;
		renderpassBeginInfo.clearValueCount = 0;
		renderpassBeginInfo.pClearValues = NULL;

		VkImageSubresourceRange subResourceRange;
		subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subResourceRange.baseMipLevel = 0;
		subResourceRange.levelCount = 1;
		subResourceRange.baseArrayLayer = 0;
		subResourceRange.layerCount = 1;

		VkImageMemoryBarrier imageMemBarrier;
		imageMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemBarrier.pNext = NULL;
		imageMemBarrier.srcAccessMask = 0;
		imageMemBarrier.dstAccessMask = 0;
		imageMemBarrier.subresourceRange = subResourceRange;

		// First create setup command buffer to be executed once
		// When the loop starts the graphics command buffer should see the images as if they
		// had just come from a previous finished loop
		// Also move all swapchain images from undefined to present layout
		vkAllocateCommandBuffers(dev, &commandBufferInfo, &setupBuf);
		vkBeginCommandBuffer(setupBuf, &commandBufferBeginInfo);
		imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemBarrier.srcQueueFamilyIndex = 0;
		imageMemBarrier.dstQueueFamilyIndex = qFamTransferIndex;
		imageMemBarrier.image = frontImg;
		vkCmdPipelineBarrier(setupBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);
		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageMemBarrier.dstQueueFamilyIndex = qFamComputeIndex;
		imageMemBarrier.image = backImg;
		vkCmdPipelineBarrier(setupBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);

		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imageMemBarrier.dstQueueFamilyIndex = qFamGraphicsIndex;
		for (uint32_t i=0; i<imgCount; i++) {
			imageMemBarrier.image = images[i];
			vkCmdPipelineBarrier(setupBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);
		}
		vkEndCommandBuffer(setupBuf);

		vkAllocateCommandBuffers(dev, &commandBufferInfo, &graphicsBackToFrontBuf);
		vkAllocateCommandBuffers(dev, &commandBufferInfo, &graphicsFrontToBackBuf);
		vkBeginCommandBuffer(graphicsBackToFrontBuf, &commandBufferBeginInfo);
		vkBeginCommandBuffer(graphicsFrontToBackBuf, &commandBufferBeginInfo);
		imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imageMemBarrier.srcQueueFamilyIndex = qFamTransferIndex;
		imageMemBarrier.dstQueueFamilyIndex = qFamGraphicsIndex;
		imageMemBarrier.image = frontImg;
		vkCmdPipelineBarrier(graphicsBackToFrontBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);
		imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemBarrier.srcQueueFamilyIndex = qFamComputeIndex;
		imageMemBarrier.dstQueueFamilyIndex = qFamGraphicsIndex;
		imageMemBarrier.image = backImg;
		vkCmdPipelineBarrier(graphicsBackToFrontBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);

		imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imageMemBarrier.srcQueueFamilyIndex = qFamTransferIndex;
		imageMemBarrier.dstQueueFamilyIndex = qFamGraphicsIndex;
		imageMemBarrier.image = backImg;
		vkCmdPipelineBarrier(graphicsFrontToBackBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);
		imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemBarrier.srcQueueFamilyIndex = qFamComputeIndex;
		imageMemBarrier.dstQueueFamilyIndex = qFamGraphicsIndex;
		imageMemBarrier.image = frontImg;
		vkCmdPipelineBarrier(graphicsFrontToBackBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);



		vkCmdBindPipeline(graphicsBackToFrontBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		vkCmdBindPipeline(graphicsFrontToBackBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		vkCmdBindDescriptorSets(graphicsBackToFrontBuf,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					graphicsPipelineLayout,
					0, 1, &graphicsBack,
					0, NULL);
		vkCmdBindDescriptorSets(graphicsFrontToBackBuf,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					graphicsPipelineLayout,
					0, 1, &graphicsFront,
					0, NULL);
		renderpassBeginInfo.framebuffer = frontFb;
		vkCmdBeginRenderPass(graphicsBackToFrontBuf, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		renderpassBeginInfo.framebuffer = backFb;
		vkCmdBeginRenderPass(graphicsFrontToBackBuf, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(graphicsBackToFrontBuf, 0, 1, &vertexBuf, &offset);
		vkCmdBindVertexBuffers(graphicsFrontToBackBuf, 0, 1, &vertexBuf, &offset);
		vkCmdDraw(graphicsBackToFrontBuf, 3, 0, 0, 0);
		vkCmdDraw(graphicsFrontToBackBuf, 3, 0, 0, 0);
		vkCmdEndRenderPass(graphicsBackToFrontBuf);
		vkCmdEndRenderPass(graphicsFrontToBackBuf);
		vkEndCommandBuffer(graphicsBackToFrontBuf);
		vkEndCommandBuffer(graphicsFrontToBackBuf);

		// Set up compute commands
		// The front image is already in general layout, and the back image moves to general
		// beacuse of the renderpass final layout
		commandBufferInfo.commandPool = computePool;
		vkAllocateCommandBuffers(dev, &commandBufferInfo, &computeBackToFrontBuf);
		vkAllocateCommandBuffers(dev, &commandBufferInfo, &computeFrontToBackBuf);
		vkBeginCommandBuffer(computeBackToFrontBuf, &commandBufferBeginInfo);
		vkBeginCommandBuffer(computeFrontToBackBuf, &commandBufferBeginInfo);
		vkCmdBindPipeline(computeBackToFrontBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		vkCmdBindPipeline(computeFrontToBackBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		vkCmdBindDescriptorSets(computeBackToFrontBuf,
					VK_PIPELINE_BIND_POINT_COMPUTE,
					computePipelineLayout,
					0, 1, &compBackToFront,
					0, NULL);
		vkCmdBindDescriptorSets(computeFrontToBackBuf,
					VK_PIPELINE_BIND_POINT_COMPUTE,
					computePipelineLayout,
					0, 1, &compFrontToBack,
					0, NULL);

		int localGroupsNeeded = particleCount / particlesPerGroup;
		int cubeSide = 1;
		while (cubeSide*cubeSide*cubeSide < localGroupsNeeded) // I don't know if this dumb or not
			cubeSide++;
		vkCmdDispatch(computeFrontToBackBuf, cubeSide, cubeSide, cubeSide);
		vkCmdDispatch(computeBackToFrontBuf, cubeSide, cubeSide, cubeSide);
		vkEndCommandBuffer(computeBackToFrontBuf);
		vkEndCommandBuffer(computeFrontToBackBuf);

		// Create transfer command buffer and copy region struct
		commandBufferInfo.commandPool = transferPool;
		vkAllocateCommandBuffers(dev, &commandBufferInfo, &transferBuf);

		VkImageSubresourceLayers subresource;
		subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource.mipLevel = 0;
		subresource.baseArrayLayer = 0;
		subresource.layerCount = 1;

		VkOffset3D copyOffset;
		copyOffset.x = 0;
		copyOffset.y = 0;
		copyOffset.z = 0;

		VkImageCopy copyRegion;
		copyRegion.srcSubresource = subresource;
		copyRegion.srcOffset = copyOffset;
		copyRegion.dstSubresource = subresource;
		copyRegion.dstOffset = copyOffset;
		copyRegion.extent.width = screenWidth;
		copyRegion.extent.height = screenHeight;
		copyRegion.extent.depth = 1;

		VkSubmitInfo submitInfo;
		VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = NULL;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = NULL;
		submitInfo.pWaitDstStageMask = &stageFlags;
		submitInfo.commandBufferCount = 1;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &commandSem;

		submitInfo.pCommandBuffers = &setupBuf;
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, commandFence1);
		vkWaitForFences(dev, 1, &commandFence1, VK_TRUE, ~0ull);
		vkResetFences(dev, 1, &commandFence1);
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &commandSem;
		while (true) {
			// Swap front and back
			swap(frontImg, backImg);
			swap(graphicsBackToFrontBuf, graphicsFrontToBackBuf);
			swap(computeBackToFrontBuf, computeFrontToBackBuf);

			// Submit graphics and compute command buffers
			submitInfo.pCommandBuffers = &graphicsFrontToBackBuf;
			vkQueueSubmit(graphicsQueue, 1, &submitInfo, commandFence1);
			submitInfo.pCommandBuffers = &computeFrontToBackBuf;
			vkQueueSubmit(computeQueue, 1, &submitInfo, commandFence2);

			// Get next swapchain image and wait first command execution
			uint32_t imgIndex;
			vkAcquireNextImageKHR(dev, swapchain, ~0ull-1, VK_NULL_HANDLE, swapFence, &imgIndex);
			vkWaitForFences(dev, 1, &swapFence, VK_TRUE, ~0ull);
			vkWaitForFences(dev, 1, &commandFence1, VK_TRUE, ~0ull);
			vkResetFences(dev, 1, &swapFence);
			vkResetFences(dev, 1, &commandFence1);

			// Transfer and present
			// First move backImg and swapchain image to transfer layouts, then transfer, then move
			// swapchain image back to present layout
			vkResetCommandBuffer(transferBuf, 0);
			vkBeginCommandBuffer(transferBuf, &commandBufferBeginInfo);
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageMemBarrier.srcQueueFamilyIndex = qFamComputeIndex;
			imageMemBarrier.dstQueueFamilyIndex = qFamTransferIndex;
			imageMemBarrier.image = backImg;
			vkCmdPipelineBarrier(transferBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemBarrier.srcQueueFamilyIndex = qFamGraphicsIndex;
			imageMemBarrier.dstQueueFamilyIndex = qFamGraphicsIndex;
			imageMemBarrier.image = images[imgIndex];
			vkCmdPipelineBarrier(transferBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);
			vkCmdCopyImage(transferBuf, backImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					images[imgIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			imageMemBarrier.srcQueueFamilyIndex = qFamGraphicsIndex;
			imageMemBarrier.dstQueueFamilyIndex = qFamGraphicsIndex;
			imageMemBarrier.image = images[imgIndex];
			vkCmdPipelineBarrier(transferBuf,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					0, 0, NULL, 0, NULL, 1, &imageMemBarrier);
			vkEndCommandBuffer(transferBuf);

			submitInfo.pCommandBuffers = &transferBuf;
			vkQueueSubmit(transferQueue, 1, &submitInfo, commandFence1);

			vkWaitForFences(dev, 1, &commandFence2, VK_TRUE, ~0ull);
			vkResetFences(dev, 1, &commandFence2);
			vkWaitForFences(dev, 1, &commandFence1, VK_TRUE, ~0ull);
			vkResetFences(dev, 1, &commandFence1);
		}
		atexit(vkCleanup);
	} else {
		getDumbBuffers(monitorIndex);
		atexit(cleanUpDumbBuffers);

		particles = malloc(particleCount * sizeof(particle));
		tempBuf1 = (uint32_t*) calloc(xSize * ySize, sizeof(uint32_t));
		tempBuf2 = (uint32_t*) calloc(xSize * ySize, sizeof(uint32_t));
		atexit(cleanUpOtherBuffers);

		srand(getMicros());
		for (int i=0; i<particleCount; i++) {
			genParticle(particles + i);
		}

		while (1) {
			unsigned long long start = getMicros();
			draw(backBuf);
			unsigned long long end = getMicros();
			waitVBlankAndSwapBuffers();

			unsigned long long elapsed = end - start;
			printf("%llu microseconds this frame\n", elapsed);
		}
	}
	return 0;
}

unsigned long long getMicros() {
	struct timespec tms;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tms);
	return tms.tv_sec*1000000llu + tms.tv_nsec/1000llu;
}

void genParticle(particle *p) {
	p->posX = rand() % (xSize/2) + xSize/4;
	p->posY = rand() % (ySize/2) + ySize/4;
	p->angle = (double) rand() / RAND_MAX * 2 * M_PI;
	p->dirX = particleSpeed * cos(p->angle);
	p->dirY = particleSpeed * sin(p->angle);
}

void blur() {
	// In edges and corners treat the out of bounds as if extended infinitely by the same value as the edge
	unsigned char *target, *ul, *uc, *ur, *cl, *cc, *cr, *dl, *dc, *dr; // up left, up center, up right, etc
	unsigned int temp;
	// corners
	target = (unsigned char*) (tempBuf1 + pixel(0, 0));
	cc = (unsigned char*) (tempBuf2 + pixel(0, 0));
	cr = (unsigned char*) (tempBuf2 + pixel(1, 0));
	dc = (unsigned char*) (tempBuf2 + pixel(0, 1));
	dr = (unsigned char*) (tempBuf2 + pixel(1, 1));
	for (int i=0; i<3; i++) {
		temp = cc[i]*(blurKernel[0]+blurKernel[1]+blurKernel[3]+blurKernel[4]) +
			cr[i]*(blurKernel[2]+blurKernel[5]) + dc[i]*(blurKernel[6]+blurKernel[7]) + dr[i]*blurKernel[8];
		target[i] = temp / blurDivide;
	}
	target = (unsigned char*) (tempBuf1 + pixel(xSize-1, 0));
	cl = (unsigned char*) (tempBuf2 + pixel(xSize-2, 0));
	cc = (unsigned char*) (tempBuf2 + pixel(xSize-1, 0));
	dl = (unsigned char*) (tempBuf2 + pixel(xSize-2, 1));
	dc = (unsigned char*) (tempBuf2 + pixel(xSize-1, 1));
	for (int i=0; i<3; i++) {
		temp = cc[i]*(blurKernel[1]+blurKernel[2]+blurKernel[4]+blurKernel[5]) +
			cl[i]*(blurKernel[0]+blurKernel[3]) + dc[i]*(blurKernel[7]+blurKernel[8]) + dl[i]*blurKernel[6];
		target[i] = temp / blurDivide;
	}
	target = (unsigned char*) (tempBuf1 + pixel(0, ySize-1));
	uc = (unsigned char*) (tempBuf2 + pixel(0, ySize-2));
	ur = (unsigned char*) (tempBuf2 + pixel(1, ySize-2));
	cc = (unsigned char*) (tempBuf2 + pixel(0, ySize-1));
	cr = (unsigned char*) (tempBuf2 + pixel(1, ySize-1));
	for (int i=0; i<3; i++) {
		temp = cc[i]*(blurKernel[3]+blurKernel[4]+blurKernel[6]+blurKernel[7]) +
			uc[i]*(blurKernel[0]+blurKernel[1]) + cr[i]*(blurKernel[5]+blurKernel[8]) + ur[i]*blurKernel[2];
		target[i] = temp / blurDivide;
	}
	target = (unsigned char*) (tempBuf1 + pixel(xSize-1, ySize-1));
	ul = (unsigned char*) (tempBuf2 + pixel(xSize-2, ySize-2));
	uc = (unsigned char*) (tempBuf2 + pixel(xSize-1, ySize-2));
	cl = (unsigned char*) (tempBuf2 + pixel(xSize-2, ySize-1));
	cc = (unsigned char*) (tempBuf2 + pixel(xSize-1, ySize-1));
	for (int i=0; i<3; i++) {
		temp = cc[i]*(blurKernel[4]+blurKernel[5]+blurKernel[7]+blurKernel[8]) +
			uc[i]*(blurKernel[1]+blurKernel[2]) + cl[i]*(blurKernel[3]+blurKernel[6]) + cc[i]*blurKernel[0];
		target[i] = temp / blurDivide;
	}

	// edges
	for (unsigned int x=1; x<xSize-1; x++) { // up edge
		target = (unsigned char*) (tempBuf1 + pixel(x, 0));
		cl = (unsigned char*) (tempBuf2 + pixel(x-1, 0));
		cc = (unsigned char*) (tempBuf2 + pixel(x, 0));
		cr = (unsigned char*) (tempBuf2 + pixel(x+1, 0));
		dl = (unsigned char*) (tempBuf2 + pixel(x-1, 1));
		dc = (unsigned char*) (tempBuf2 + pixel(x, 1));
		dr = (unsigned char*) (tempBuf2 + pixel(x+1, 1));
		for (int i=0; i<3; i++) {
			temp = cc[i]*(blurKernel[1]+blurKernel[4]) + cl[i]*(blurKernel[0]+blurKernel[3]) +
				cr[i]*(blurKernel[2]+blurKernel[5]) + dl[i]*blurKernel[6] + dc[i]*blurKernel[7] + dr[i]*blurKernel[8];
			target[i] = temp / blurDivide;
		}
	}
	for (unsigned int x=1; x<xSize-1; x++) { // down edge
		target = (unsigned char*) (tempBuf1 + pixel(x, ySize-1));
		ul = (unsigned char*) (tempBuf2 + pixel(x-1, ySize-2));
		uc = (unsigned char*) (tempBuf2 + pixel(x, ySize-2));
		ur = (unsigned char*) (tempBuf2 + pixel(x+1, ySize-2));
		cl = (unsigned char*) (tempBuf2 + pixel(x-1, ySize-1));
		cc = (unsigned char*) (tempBuf2 + pixel(x, ySize-1));
		cr = (unsigned char*) (tempBuf2 + pixel(x+1, ySize-1));
		for (int i=0; i<3; i++) {
			temp = cc[i]*(blurKernel[4]+blurKernel[7]) + cl[i]*(blurKernel[3]+blurKernel[6]) +
				cr[i]*(blurKernel[5]+blurKernel[8]) + ul[i]*blurKernel[0] + uc[i]*blurKernel[1] + ur[i]*blurKernel[2];
			target[i] = temp / blurDivide;
		}
	}
	for (unsigned int y=1; y<ySize-1; y++) { // left edge
		target = (unsigned char*) (tempBuf1 + pixel(0, y));
		uc = (unsigned char*) (tempBuf2 + pixel(0, y-1));
		ur = (unsigned char*) (tempBuf2 + pixel(1, y-1));
		cc = (unsigned char*) (tempBuf2 + pixel(0, y));
		cr = (unsigned char*) (tempBuf2 + pixel(1, y));
		dc = (unsigned char*) (tempBuf2 + pixel(0, y+1));
		dr = (unsigned char*) (tempBuf2 + pixel(1, y+1));
		for (int i=0; i<3; i++) {
			temp = cc[i]*(blurKernel[3]+blurKernel[4]) + uc[i]*(blurKernel[0]+blurKernel[1]) +
				dc[i]*(blurKernel[6]+blurKernel[7]) + ur[i]*blurKernel[2] + cr[i]*blurKernel[5] + dr[i]*blurKernel[8];
			target[i] = temp / blurDivide;
		}
	}
	for (unsigned int y=1; y<ySize-1; y++) { // right edge
		target = (unsigned char*) (tempBuf1 + pixel(xSize-1, y));
		ul = (unsigned char*) (tempBuf2 + pixel(xSize-2, y-1));
		uc = (unsigned char*) (tempBuf2 + pixel(xSize-1, y-1));
		cl = (unsigned char*) (tempBuf2 + pixel(xSize-2, y));
		cc = (unsigned char*) (tempBuf2 + pixel(xSize-1, y));
		dl = (unsigned char*) (tempBuf2 + pixel(xSize-2, y+1));
		dc = (unsigned char*) (tempBuf2 + pixel(xSize-1, y+1));
		for (int i=0; i<3; i++) {
			temp = cc[i]*(blurKernel[4]+blurKernel[5]) + uc[i]*(blurKernel[1]+blurKernel[2]) +
				dc[i]*(blurKernel[7]+blurKernel[8]) + ul[i]*blurKernel[0] + cl[i]*blurKernel[3] + dl[i]*blurKernel[6];
			target[i] = temp / blurDivide;
		}
	}

	// center
	for (unsigned int x=1; x<xSize-1; x++) {
		for (unsigned int y=1; y<ySize-1; y++) {
			target = (unsigned char*) (tempBuf1 + pixel(x, y));
			ul = (unsigned char*) (tempBuf2 + pixel(x-1, y-1));
			uc = (unsigned char*) (tempBuf2 + pixel(x, y-1));
			ur = (unsigned char*) (tempBuf2 + pixel(x+1, y-1));
			cl = (unsigned char*) (tempBuf2 + pixel(x-1, y));
			cc = (unsigned char*) (tempBuf2 + pixel(x, y));
			cr = (unsigned char*) (tempBuf2 + pixel(x+1, y));
			dl = (unsigned char*) (tempBuf2 + pixel(x-1, y+1));
			dc = (unsigned char*) (tempBuf2 + pixel(x, y+1));
			dr = (unsigned char*) (tempBuf2 + pixel(x+1, y+1));
			for (int i=0; i<3; i++) {
				temp = ul[i]*blurKernel[0] + uc[i]*blurKernel[1] + ur[i]*blurKernel[2] +
					cl[i]*blurKernel[3] + cc[i]*blurKernel[4] + cr[i]*blurKernel[5] +
					dl[i]*blurKernel[6] + dc[i]*blurKernel[7] + dr[i]*blurKernel[8];
				target[i] = temp / blurDivide;
			}
		}
	}
}

void fade() {
	for (unsigned int i=0; i<xSize*ySize; i++) {
		unsigned char *color = (unsigned char*) (tempBuf1 + i);
		// blue
		color[0] = max(0, color[0] - blueFade);
		// green
		color[1] = max(0, color[1] - greenFade);
		// red
		color[2] = max(0, color[2] - redFade);
	}
}

void moveParticles() {
	for (int i=0; i<particleCount; i++) {
		particle *p = particles + i;

		// Steer towards highest luma pixel some steps away in 3 directions
		double angles[3] = {p->angle - steerAmplitude, p->angle, p->angle + steerAmplitude};
		unsigned int pixels[3];
		double lumas[3] = {0, 0, 0};
		for (int j=0; j<3; j++) {
			int lookPosX = p->posX + particleSpeed * steerLength * cos(angles[j]);
			int lookPosY = p->posY + particleSpeed * steerLength * sin(angles[j]);
			if (lookPosX < 0 || lookPosX > xSize-1 || lookPosY < 0 || lookPosY > ySize-1)
				continue;
			pixels[j] = pixel(lookPosX, lookPosY);
			// https://en.wikipedia.org/wiki/Luma_(video)
			unsigned char *color = (unsigned char*) (tempBuf1 + pixels[j]);
			lumas[j] += color[0] * 0.0722;
			lumas[j] += color[1] * 0.7152;
			lumas[j] += color[2] * 0.2126;
		}
		if (lumas[0] > lumas[1] && lumas[0] > lumas[2]) {
			p->angle = angles[0];
		} else if (lumas[2] > lumas[0] && lumas[2] > lumas[1]) {
			p->angle = angles[2];
		} else if (lumas[1] > 0.3) { // promotes more complex looking paths and more new paths
			if (lumas[0] > lumas[2])
				p->angle = angles[0];
			else
				p->angle = angles[2];
		}

		// Change direction randomly a bit
		p->angle += (rand() % 201 - 100) / (100.0 / maxRandRadianChange);
		p->dirX = particleSpeed * cos(p->angle);
		p->dirY = particleSpeed * sin(p->angle);

		particles[i].posX += particles[i].dirX;
		particles[i].posY += particles[i].dirY;
		if (particles[i].posX < 0) {
			particles[i].posX = fabs(particles[i].posX);
			particles[i].dirX *= -1;
			particles[i].angle = (particles[i].angle + M_PI) * -1;
		}
		if (particles[i].posX > xSize-1) {
			particles[i].posX = xSize-1 - fabs(xSize-1 - particles[i].posX);
			particles[i].dirX *= -1;
			particles[i].angle = (particles[i].angle + M_PI) * -1;
		}
		if (particles[i].posY < 0) {
			particles[i].posY = fabs(particles[i].posY);
			particles[i].dirY *= -1;
			particles[i].angle = particles[i].angle * -1;
		}
		if (particles[i].posY > ySize-1) {
			particles[i].posY = ySize-1 - fabs(ySize-1 - particles[i].posY);
			particles[i].dirY *= -1;
			particles[i].angle = particles[i].angle * -1;
		}

		tempBuf1[pixel((unsigned int)particles[i].posX, (unsigned int)particles[i].posY)] = particleColor;
	}
}

void draw(uint32_t *buf) {
	swap(tempBuf1, tempBuf2);
	
	blur();
	fade();
	moveParticles();

	// Copy final result
	for (unsigned int i=0; i<xSize*ySize; i++) {
		buf[i] = tempBuf1[i];
	}
}
