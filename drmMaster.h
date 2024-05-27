#include <xf86drm.h>
#include <xf86drmMode.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>

int getDrmMasterFd(int monitorIndex, int *isLeased, const char *fileName);
