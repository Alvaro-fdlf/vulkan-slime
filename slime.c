/*
 * CONSTANTS TO MODIFY BEHAVIOR AT COMPILE TIME GO HERE
 */
const int monitorIndex = 0; // Maybe make it command line option later

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

unsigned long long getMicros();
void draw(uint32_t *buf);

int main(int argc, char *argv[]) {
	getDumbBuffers(monitorIndex);

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

void draw(uint32_t *buf) {
	// test by drawing a moving circle
	for (int i=0; i<xSize*ySize; i++)
		buf[i] = 0;
	static int frame = 0;
	frame++;
	const int yCenter = ySize/2;
	const int xCenter = xSize/2;
	const int radius = min(xSize/3, ySize/3) + sin(frame / 10.0) * 100;
	const int thickness = 5;
	const int inner = radius-thickness;
	const int outer = radius+thickness;

	for (int x=0; x<xSize; x++) {
		int xDist = abs(x - xCenter);
		for (int y=0; y<ySize; y++) {
			int yDist = abs(y - yCenter);
			int distSquare = yDist*yDist + xDist*xDist;
			if (distSquare <= outer*outer && distSquare >= inner * inner) {
				buf[pixel(x, y)] = 0x00FF0000;
			}
		}
	}
}
