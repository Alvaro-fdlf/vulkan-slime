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
#include "dumbBuffers.h"
#include "vulkanSetup.h"

/*
 * VARIABLES TO MODIFY BEHAVIOR AT COMPILE TIME GO HERE
 */
const int monitorIndex = 0; // Maybe make it command line option later
#define particleCount 200000
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
particle particles[particleCount];
uint32_t *tempBuf1, *tempBuf2; // copying from frontBuf to backBuf is slower than from a usual tempBuf to backBuf (why?)

unsigned long long getMicros();
void draw(uint32_t *buf);
void genParticle(particle *p);

int main(int argc, char *argv[]) {
	vkSetup(); // just for testing for now

	getDumbBuffers(monitorIndex);
	tempBuf1 = (uint32_t*) malloc(sizeof(uint32_t) * xSize * ySize);
	tempBuf2 = (uint32_t*) malloc(sizeof(uint32_t) * xSize * ySize);
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
			particles[i].posX = abs(particles[i].posX);
			particles[i].dirX *= -1;
			particles[i].angle = (particles[i].angle + M_PI) * -1;
		}
		if (particles[i].posX > xSize-1) {
			particles[i].posX = xSize-1 - abs(xSize-1 - particles[i].posX);
			particles[i].dirX *= -1;
			particles[i].angle = (particles[i].angle + M_PI) * -1;
		}
		if (particles[i].posY < 0) {
			particles[i].posY = abs(particles[i].posY);
			particles[i].dirY *= -1;
			particles[i].angle = particles[i].angle * -1;
		}
		if (particles[i].posY > ySize-1) {
			particles[i].posY = ySize-1 - abs(ySize-1 - particles[i].posY);
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
