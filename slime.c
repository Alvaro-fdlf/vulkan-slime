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

/*
 * CONSTANTS TO MODIFY BEHAVIOR AT COMPILE TIME GO HERE
 */
const int monitorIndex = 0; // Maybe make it command line option later
#define particleCount 100
const double particleSpeed = 1.0; // distance traveled per frame
const uint32_t particleColor = 0x00FFFFFF; // XRGB
const uint8_t redFade = 0x1, greenFade = 0x1, blueFade = 0x1; // change these to get different effects

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

typedef struct {
	double posX, posY, dirX, dirY;
} particle;
particle particles[particleCount];
uint32_t *tempBuf; // copying from frontBuf to backBuf is slower than from a usual tempBuf to backBuf (why?)

unsigned long long getMicros();
void draw(uint32_t *buf);
void genParticle(particle *p);

int main(int argc, char *argv[]) {
	getDumbBuffers(monitorIndex);
	tempBuf = (uint32_t*) malloc(sizeof(uint32_t) * xSize * ySize);
	srand(getMicros());
	for (int i=0; i<particleCount; i++) {
		genParticle(particles + i);
	}

	while (1) {
		unsigned long long start = getMicros();
		draw(backBuf);
		unsigned long long end = getMicros();
		waitVBlankAndSwapBuffers();

		long long elapsed = end - start;
		printf("%llu microseconds this frame\n", elapsed);
	}
	return 0;
}

unsigned long long getMicros() {
	struct timespec tms;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tms);
	return tms.tv_sec * 1000000 + tms.tv_nsec/1000;
}

void genParticle(particle *p) {
	p->posX = rand() % (xSize/2) + xSize/4;
	p->posY = rand() % (ySize/2) + ySize/4;
	int angle = rand();
	p->dirX = particleSpeed * cos(angle);
	p->dirY = particleSpeed * sin(angle);
}

void draw(uint32_t *buf) {
	// Fade away trails
	for (int i=0; i<xSize*ySize; i++) {
		unsigned char *color = (unsigned char*) (tempBuf + i);
		// blue
		color[0] = max(0, color[0] - blueFade);
		// green
		color[1] = max(0, color[1] - greenFade);
		// red
		color[2] = max(0, color[2] - redFade);
	}

	// Move particles
	for (int i=0; i<particleCount; i++) {
		particles[i].posX += particles[i].dirX;
		particles[i].posY += particles[i].dirY;
		if (particles[i].posX < 0) {
			particles[i].posX = abs(particles[i].posX);
			particles[i].dirX *= -1;
		}
		if (particles[i].posX > xSize+1) {
			particles[i].posX = xSize - abs(xSize - particles[i].posX);
			particles[i].dirX *= -1;
		}
		if (particles[i].posY < 0) {
			particles[i].posY = abs(particles[i].posY);
			particles[i].dirY *= -1;
		}
		if (particles[i].posY > ySize+1) {
			particles[i].posY = ySize - abs(ySize - particles[i].posY);
			particles[i].dirY *= -1;
		}

		tempBuf[pixel((int)particles[i].posX, (int)particles[i].posY)] = particleColor;
	}

	// Copy final result
	for (int i=0; i<xSize*ySize; i++) {
		buf[i] = tempBuf[i];
	}
}
