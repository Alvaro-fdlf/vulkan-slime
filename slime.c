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
#define particleCount 10000
const double particleSpeed = 1.0; // distance traveled per frame
const double maxRandRadianChange = M_PI * 0.05; // Maximum random change of angle (in radians) per frame on top of the steering
const uint32_t particleColor = 0x00FFFFFF; // XRGB
const uint8_t redFade = 0x1, greenFade = 0x1, blueFade = 0x1; // change these to get different effects
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
	p->angle = (double) rand() / RAND_MAX * 2 * M_PI;
	p->dirX = particleSpeed * cos(p->angle);
	p->dirY = particleSpeed * sin(p->angle);
}

void draw(uint32_t *buf) {
	swap(tempBuf1, tempBuf2);
	// Blur trails
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
		temp = cc[i]*(1+2+2+4) + cr[i]*(1+2) + dc[i]*(1+2) + dr[i];
		target[i] = temp / 16;
	}
	target = (unsigned char*) (tempBuf1 + pixel(xSize-1, 0));
	cl = (unsigned char*) (tempBuf2 + pixel(xSize-2, 0));
	cc = (unsigned char*) (tempBuf2 + pixel(xSize-1, 0));
	dl = (unsigned char*) (tempBuf2 + pixel(xSize-2, 1));
	dc = (unsigned char*) (tempBuf2 + pixel(xSize-1, 1));
	for (int i=0; i<3; i++) {
		temp = cc[i]*(1+2+2+4) + cl[i]*(1+2) + dc[i]*(1+2) + dl[i];
		target[i] = temp / 16;
	}
	target = (unsigned char*) (tempBuf1 + pixel(0, ySize-1));
	uc = (unsigned char*) (tempBuf2 + pixel(0, ySize-2));
	ur = (unsigned char*) (tempBuf2 + pixel(1, ySize-2));
	cc = (unsigned char*) (tempBuf2 + pixel(0, ySize-1));
	cr = (unsigned char*) (tempBuf2 + pixel(1, ySize-1));
	for (int i=0; i<3; i++) {
		temp = cc[i]*(1+2+2+4) + uc[i]*(1+2) + cr[i]*(1+2) + ur[i];
		target[i] = temp / 16;
	}
	target = (unsigned char*) (tempBuf1 + pixel(xSize-1, ySize-1));
	ul = (unsigned char*) (tempBuf2 + pixel(xSize-2, ySize-2));
	uc = (unsigned char*) (tempBuf2 + pixel(xSize-1, ySize-2));
	cl = (unsigned char*) (tempBuf2 + pixel(xSize-2, ySize-1));
	cc = (unsigned char*) (tempBuf2 + pixel(xSize-1, ySize-1));
	for (int i=0; i<3; i++) {
		temp = cc[i]*(1+2+2+4) + uc[i]*(1+2) + cl[i]*(1+2) + cc[i];
		target[i] = temp / 16;
	}

	// edges
	for (int x=1; x<xSize-1; x++) { // up edge
		target = (unsigned char*) (tempBuf1 + pixel(x, 0));
		cl = (unsigned char*) (tempBuf2 + pixel(x-1, 0));
		cc = (unsigned char*) (tempBuf2 + pixel(x, 0));
		cr = (unsigned char*) (tempBuf2 + pixel(x+1, 0));
		dl = (unsigned char*) (tempBuf2 + pixel(x-1, 1));
		dc = (unsigned char*) (tempBuf2 + pixel(x, 1));
		dr = (unsigned char*) (tempBuf2 + pixel(x+1, 1));
		for (int i=0; i<3; i++) {
			temp = cc[i]*(2+4) + cl[i]*(1+2) + cr[i]*(1+2) + dl[i] + dc[0]*2 + dr[0];
			target[i] = temp / 16;
		}
	}
	for (int x=1; x<xSize-1; x++) { // down edge
		target = (unsigned char*) (tempBuf1 + pixel(x, ySize-1));
		ul = (unsigned char*) (tempBuf2 + pixel(x-1, ySize-2));
		uc = (unsigned char*) (tempBuf2 + pixel(x, ySize-2));
		ur = (unsigned char*) (tempBuf2 + pixel(x+1, ySize-2));
		cl = (unsigned char*) (tempBuf2 + pixel(x-1, ySize-1));
		cc = (unsigned char*) (tempBuf2 + pixel(x, ySize-1));
		cr = (unsigned char*) (tempBuf2 + pixel(x+1, ySize-1));
		for (int i=0; i<3; i++) {
			temp = cc[i]*(2+4) + cl[i]*(1+2) + cr[i]*(1+2) + ul[i] + uc[0]*2 + ur[0];
			target[i] = temp / 16;
		}
	}
	for (int y=1; y<ySize-1; y++) { // left edge
		target = (unsigned char*) (tempBuf1 + pixel(0, y));
		uc = (unsigned char*) (tempBuf2 + pixel(0, y-1));
		ur = (unsigned char*) (tempBuf2 + pixel(1, y-1));
		cc = (unsigned char*) (tempBuf2 + pixel(0, y));
		cr = (unsigned char*) (tempBuf2 + pixel(1, y));
		dc = (unsigned char*) (tempBuf2 + pixel(0, y+1));
		dr = (unsigned char*) (tempBuf2 + pixel(1, y+1));
		for (int i=0; i<3; i++) {
			temp = cc[i]*(2+4) + uc[i]*(1+2) + dc[i]*(1+2) + ur[i] + cr[0]*2 + dr[0];
			target[i] = temp / 16;
		}
	}
	for (int y=1; y<ySize-1; y++) { // right edge
		target = (unsigned char*) (tempBuf1 + pixel(xSize-1, y));
		ul = (unsigned char*) (tempBuf2 + pixel(xSize-2, y-1));
		uc = (unsigned char*) (tempBuf2 + pixel(xSize-1, y-1));
		cl = (unsigned char*) (tempBuf2 + pixel(xSize-2, y));
		cc = (unsigned char*) (tempBuf2 + pixel(xSize-1, y));
		dl = (unsigned char*) (tempBuf2 + pixel(xSize-2, y+1));
		dc = (unsigned char*) (tempBuf2 + pixel(xSize-1, y+1));
		for (int i=0; i<3; i++) {
			temp = cc[i]*(2+4) + uc[i]*(1+2) + dc[i]*(1+2) + ul[i] + cl[0]*2 + dl[0];
			target[i] = temp / 16;
		}
	}

	// center
	for (int x=1; x<xSize-1; x++) {
		for (int y=1; y<ySize-1; y++) {
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
				temp = ul[i] + uc[i]*2 + ur[i] + cl[i]*2 + cc[i]*4 + cr[i]*2 + dl[i] + dc[i]*2 + dr[i];
				target[i] = temp / 16;
			}
		}
	}

	// Fade away trails
	for (int i=0; i<xSize*ySize; i++) {
		unsigned char *color = (unsigned char*) (tempBuf1 + i);
		// blue
		color[0] = max(0, color[0] - blueFade);
		// green
		color[1] = max(0, color[1] - greenFade);
		// red
		color[2] = max(0, color[2] - redFade);
	}

	// Move particles
	for (int i=0; i<particleCount; i++) {
		particle *p = particles + i;

		// First change direction randomly a bit
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

		tempBuf1[pixel((int)particles[i].posX, (int)particles[i].posY)] = particleColor;
	}

	// Copy final result
	for (int i=0; i<xSize*ySize; i++) {
		buf[i] = tempBuf1[i];
	}
}
