#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_mode.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

#define DRI_DEVICE "/dev/dri/card0"

int main(void)
{
	int drm_fd = open(DRI_DEVICE, O_RDWR);
	if (drm_fd < 0)
	{
		printf("Failed to open dri device %s\n", DRI_DEVICE);
		return -EINVAL;
	}

	drmModeResPtr res = drmModeGetResources(drm_fd);
	if (!res)
	{
		printf("Failed to get drm resources.\n");
		return -EINVAL;
	}

	printf("connector_count: %d\n", res->count_connectors);

	drmModeConnectorPtr connector = drmModeGetConnectorCurrent(drm_fd, res->connectors[0]);
	if (!connector)
	{
		printf("Failed to get connector.\n");
		return -EINVAL;
	}

	printf("count_modes: %d\n", connector->count_modes);

	drmModeModeInfoPtr resolution = 0;
	for (int i = 0; i < connector->count_modes; ++i)
	{
		resolution = &connector->modes[i];
		/*
			uint32_t clock;
			uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
			uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

			uint32_t vrefresh;

			uint32_t flags;
			uint32_t type;
			char name[DRM_DISPLAY_MODE_LEN];
		*/

		printf(
				"clock: %d\n"
				"hdisplay: %d, hsync_start: %d, hsync_end: %d, htotal: %d, hskew: %d\n"
				"vdisplay: %d, vsync_start: %d, vsync_end: %d, vtotal: %d, vscan: %d\n"
				"vrefresh: %d\n"
				"flags: %d\n"
				"type: %d\n"
				"name: %s\n",
				resolution->clock,
				resolution->hdisplay, resolution->hsync_start, resolution->hsync_end, resolution->htotal, resolution->hskew,
				resolution->vdisplay, resolution->vsync_start, resolution->vsync_end, resolution->vtotal, resolution->vscan,
				resolution->vrefresh,
				resolution->flags,
				resolution->type,
				resolution->name
		);

		if (resolution->type & DRM_MODE_TYPE_PREFERRED)
			break;
	}

	return 0;
}
