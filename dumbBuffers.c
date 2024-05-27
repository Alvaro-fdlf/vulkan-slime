#include "dumbBuffers.h"
#include "drmMaster.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

uint32_t *frontBuf, *backBuf;
unsigned int xSize, ySize;

static int fd;
/*
static drmModeResPtr res;
static drmModeConnectorPtr connector;
static drmModeCrtcPtr crtc;
static drmModeModeInfoPtr mode;
*/
static uint32_t frontBufId, backBufId;

static uint64_t dumbBuffersSize;

void cleanUpDumbBuffers() {
	drmModeFreeResources(res);
	//drmModeFreeConnector(connector); gets freed somewhere else?
	drmModeFreeCrtc(crtc);
	drmModeFreeModeInfo(mode);

	munmap(frontBuf, dumbBuffersSize);
	munmap(backBuf, dumbBuffersSize);
	cleanUpDrmMaster();
}

/*
static void getCrtcFromCurrentConnector() {
	// make sure it's connected
	if (connector->connection != DRM_MODE_CONNECTED)
		return;

	// iterate over all possible encoders, and all possible crtcs
	// take first available crtc, the encoder is set up automatically
	for (int j=0; j<connector->count_encoders; j++) {
		drmModeEncoderPtr encoder = drmModeGetEncoder(fd, connector->encoders[j]);
		for (int k=0; k<res->count_crtcs; k++) {
			if (encoder->possible_crtcs && 1ul<<k) {
				crtc = drmModeGetCrtc(fd, res->crtcs[k]);
				drmModeFreeEncoder(encoder);
				return;
			}
		}
		drmModeFreeEncoder(encoder);
	}
}

static void getConnectorWithCrtc(int monitorIndex) {
	connector = NULL;
	crtc = NULL;

	if (monitorIndex == 0) {
		// Choose first available connector and a suitable crtc
		for (int i=0; i<res->count_connectors; i++) {
			connector = drmModeGetConnector(fd, res->connectors[i]);
			getCrtcFromCurrentConnector();
			if (crtc != NULL)
				return;
		}
	} else {
		if (monitorIndex > res->count_connectors) {
			fprintf(stderr, "There aren't enough monitors, choose a lower index\n");
			abort();
		}
		connector = drmModeGetConnector(fd, res->connectors[monitorIndex-1]);
		getCrtcFromCurrentConnector();
	}
}
*/

static void getDumbBuffersFromKMS(int monitorIndex) {
	getConnectorWithCrtc(monitorIndex);
	if (crtc == NULL) {
		fprintf(stderr, "Couldn't find a suitable crtc for the given connector, or any connector\n");
		abort();
	}

	mode = connector->modes; // First mode should be the best
	xSize = mode->hdisplay;
	ySize = mode->vdisplay;

	// create 2 buffers for page flipping
	uint32_t handle, pitch;
	uint64_t offset;
	drmModeCreateDumbBuffer(fd, xSize, ySize, 32, 0, &handle, &pitch, &dumbBuffersSize);
	drmModeMapDumbBuffer(fd, handle, &offset);
	drmModeAddFB(fd, xSize, ySize, 24, 32, pitch, handle, &frontBufId);
	frontBuf = mmap(NULL, dumbBuffersSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	
	drmModeCreateDumbBuffer(fd, xSize, ySize, 32, 0, &handle, &pitch, &dumbBuffersSize);
	drmModeMapDumbBuffer(fd, handle, &offset);
	drmModeAddFB(fd, xSize, ySize, 24, 32, pitch, handle, &backBufId);
	backBuf = mmap(NULL, dumbBuffersSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);

	drmModeSetCrtc(fd, crtc->crtc_id, frontBufId, 0, 0, &(connector->connector_id), 1, mode);
}

// Functions exposed in the header file
// monitorIndex > 1 asks for monitorIndex-1th monitor
// monitorIndex == 0 asks for first available monitor it finds
void getDumbBuffers(int monitorIndex) {
	int isLeased;
	char cardName[30] = "none";

	fd = getDrmMasterFd(monitorIndex, &isLeased, cardName);
	res = drmModeGetResources(fd);
	if (res == NULL) {
		fprintf(stderr, "Couldn't get resources from card %s\n", cardName);
		abort();
	}
	if (isLeased) {
		getDumbBuffersFromKMS(0);
	} else
		getDumbBuffersFromKMS(monitorIndex);
}

void waitVBlankAndSwapBuffers() {
	// doesn't flip immediately, only schedules to flip at next vblank
	drmModePageFlip(fd, crtc->crtc_id, backBufId, 0, NULL);

	drmVBlank vblank;
	vblank.request.type = DRM_VBLANK_RELATIVE;
	vblank.request.sequence = 1;
	drmWaitVBlank(fd, &vblank);


	uint32_t temp = frontBufId;
	frontBufId = backBufId;
	backBufId = temp;
	uint32_t *tempPtr = frontBuf;
	frontBuf = backBuf;
	backBuf = tempPtr;
}
