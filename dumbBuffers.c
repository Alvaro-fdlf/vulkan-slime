#include "dumbBuffers.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

uint32_t *frontBuf, *backBuf;
unsigned int xSize, ySize;

static int fd;
static drmModeResPtr res;
static drmModeConnectorPtr connector;
static drmModeCrtcPtr crtc;
static drmModeModeInfoPtr mode;
static uint32_t frontBufId, backBufId;

static uint64_t dumbBuffersSize;

void cleanUpDumbBuffers() {
	drmModeFreeResources(res);
	//drmModeFreeConnector(connector); gets freed somewhere else?
	drmModeFreeCrtc(crtc);
	drmModeFreeModeInfo(mode);

	munmap(frontBuf, dumbBuffersSize);
	munmap(backBuf, dumbBuffersSize);
	close(fd);
}

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

static xcb_connection_t *c;
static int getDrmLeaseFromX(int outputIndex) {
	int leasefd;
	xcb_window_t win;
	xcb_randr_lease_t leaseId;
	xcb_randr_output_t outputId;
	xcb_randr_crtc_t crtcId;

	c = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(c))
		abort();

	leaseId = xcb_generate_id(c);

	// get root window of screen
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator (xcb_get_setup (c));
	win = iter.data->root;

	// get screen resources, containing outputs (and crtcs, but not important here)
	xcb_randr_get_screen_resources_cookie_t resCookie = xcb_randr_get_screen_resources(c, win);
	xcb_randr_get_screen_resources_reply_t *resources = xcb_randr_get_screen_resources_reply (c, resCookie, NULL);
	int outputNum = xcb_randr_get_screen_resources_outputs_length(resources);
	if (outputNum < outputIndex+1) {
		fprintf(stderr, "There are only %d outputs, asked for output %d\n", outputNum, outputIndex+1);
	}
	if (outputIndex == 0)
		outputId = xcb_randr_get_screen_resources_outputs(resources)[0];
	else {
		outputId = xcb_randr_get_screen_resources_outputs(resources)[outputIndex-1];
	}

	// get crtc available to this output, make sure if we don't take the first output then don't take a crtc being used
	xcb_randr_get_output_info_cookie_t outputCookie = xcb_randr_get_output_info(c, outputId, resources->config_timestamp);
	xcb_randr_get_output_info_reply_t *outputReply = xcb_randr_get_output_info_reply(c, outputCookie, NULL);
	if (outputIndex > 1) {
		for (int i=0; i<xcb_randr_get_output_info_crtcs_length(outputReply); i++) {
			crtcId = xcb_randr_get_output_info_crtcs(outputReply)[i];
			xcb_randr_get_crtc_info_cookie_t crtcCookie = xcb_randr_get_crtc_info(c, crtcId, resources->config_timestamp);
			xcb_randr_get_crtc_info_reply_t *crtcReply = xcb_randr_get_crtc_info_reply(c, crtcCookie, NULL);
			if (crtcReply->width == 0 && crtcReply->height == 0) { // indicates crtc is unused
				free(crtcReply);
				break;
			}
			free(crtcReply);
		}
	} else {
		crtcId = xcb_randr_get_output_info_crtcs(outputReply)[0];
	}

	// create lease having all parameters
	xcb_randr_create_lease_cookie_t leaseCookie = xcb_randr_create_lease(c, win, leaseId, 1, 1, &crtcId, &outputId);
	xcb_randr_create_lease_reply_t *reply = xcb_randr_create_lease_reply(c, leaseCookie, NULL);
	leasefd = *xcb_randr_create_lease_reply_fds(c, reply);

	free(resources);
	free(outputReply);
	free(reply);
	return leasefd;
}

// Functions exposed in the header file
// monitorIndex > 1 asks for monitorIndex-1th monitor
// monitorIndex == 0 asks for first available monitor it finds
void getDumbBuffers(int monitorIndex) {
	if (monitorIndex < 0) {
		fprintf(stderr, "The monitor index must be at least 0");
		abort();
	}

	int cardNum = 0;
	char cardName[30];
	while (1) {
		sprintf(cardName, "/dev/dri/card%d", cardNum);
		fd = open(cardName, O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "No cards in /dev/dri/ that support KMS\n");
			abort();
		}

		if (drmIsKMS(fd))
			break;

		close(fd);
		cardNum += 1;
	}

	bool leased = false;

	drmSetMaster(fd);
	if (!drmIsMaster(fd)) {
		leased = true;
		close(fd);
		fd = getDrmLeaseFromX(monitorIndex);
	}

	res = drmModeGetResources(fd);
	if (res == NULL) {
		fprintf(stderr, "Couldn't get resources from card %s\n", cardName);
		abort();
	}
	if (leased) {
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
