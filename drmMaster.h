#include <xf86drm.h>
#include <xf86drmMode.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>

extern char cardName[];
extern drmModeResPtr res;
extern drmModeConnectorPtr connector;
extern drmModeCrtcPtr crtc;
extern drmModeModeInfoPtr mode;

void cleanUpDrmMaster();
int getDrmMasterFd(int monitorIndex, int *isLeased);
void getCrtcFromCurrentConnector();
void getConnectorWithCrtc(int monitorIndex);
