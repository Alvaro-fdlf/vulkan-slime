#include <xf86drm.h>
#include <xf86drmMode.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>

extern uint32_t *frontBuf, *backBuf;
extern unsigned int xSize, ySize;

void getDumbBuffers(int monitorIndex);
void waitVBlankAndSwapBuffers();
