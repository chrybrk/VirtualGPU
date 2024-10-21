#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define DEBUG

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Err: provide dri device.\n");
		return -EINVAL;
	}

	const char *card = argv[1];

	int fd = open(card, O_RDWR | O_CLOEXEC);
	if (fd < 0)
	{
		perror("Err: failed to open dri device: ");
		return -EINVAL;
	}

	drmModeResPtr res = drmModeGetResources(fd);
	if (!res)
	{
		perror("Err: cannot get drm resources: ");
		return -EINVAL;
	}

#ifdef DEBUG
	printf(
		"count_fbs: %d\n"
		"count_crtcs: %d\n"
		"count_connectors: %d\n"
		"count_encoders: %d\n"
		"min_width: %d, max_width: %d\n"
		"min_height: %d, max_height: %d\n",
		res->count_fbs,
		res->count_crtcs,
		res->count_connectors,
		res->count_encoders,
		res->min_width, res->max_width,
		res->min_height, res->max_height
	);
#endif

	drmModeConnectorPtr connector = drmModeGetConnectorCurrent(fd, res->connectors[0]);
	if (!connector)
	{
		perror("Err: failed to get connector: ");
		return -EINVAL;
	}

	drmModeModeInfoPtr resolution = 0;
	for (int i = 0; i < connector->count_modes; i++) {
		resolution = &connector->modes[i];
		if (resolution->type & DRM_MODE_TYPE_PREFERRED)
			break;
	}

#ifdef DEBUG
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
#endif

	struct drm_mode_create_dumb creq;
	struct drm_mode_destroy_dumb dreq;
	struct drm_mode_map_dumb mreq;
	uint32_t fb;
	int ret;

	uint8_t *map;

	memset(&creq, 0, sizeof(creq));
	creq.width = resolution->hdisplay;
	creq.height = resolution->vdisplay;
	creq.bpp = 32;
	
	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
	if (ret < 0)
	{
		perror("err: Failed to create dumb buffer: ");
		return -EINVAL;
	}

	ret = drmModeAddFB(fd, creq.width, creq.height, 24, creq.bpp, creq.pitch, creq.handle, &fb);
	if (ret)
	{
		perror("err: Failed to create framebuffer: ");
		return -EINVAL;
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.handle = creq.handle;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	if (ret)
	{
		perror("err: DRM buffer preparation failed: ");
		return -EINVAL;
	}

	map = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
	if (map == MAP_FAILED)
	{
		perror("err: Memory-mapping failed: ");
		return -EINVAL;
	}

	/* clear framebuffer to 0 */
	memset(map, 0, creq.size);

	printf("[INFO]: Memory allocated for framebuffer.\n");

	/* get encoder */
	drmModeEncoderPtr encoder = drmModeGetEncoder(fd, connector->encoder_id);
	if (!encoder)
	{
		perror("err: Failed to get encoder: ");
		return -EINVAL;
	}

	/* get crtc */
	drmModeCrtcPtr crtc = drmModeGetCrtc(fd, encoder->crtc_id);

	// Fill the framebuffer with a solid color (e.g., red)
	for (uint32_t y = 0; y < creq.height; y++) {
		for (uint32_t x = 0; x < creq.width; x++) {
			uint32_t pixel_offset = (y * creq.pitch) + (x * 4);
			map[pixel_offset] = 0xFF;        // Blue
			map[pixel_offset + 1] = 0x00;    // Green
			map[pixel_offset + 2] = 0xaa;    // Red
			map[pixel_offset + 3] = 0xFF;    // Alpha
		}
	}

	// Set the CRTC
	if (drmModeSetCrtc(fd, crtc->crtc_id, fb, 0, 0, &connector->connector_id, 1, resolution))
	{
		perror("err: Failed to set CRTC: ");
		return -EINVAL;
	}

	getchar();

	// TODO: free

	return 0;
}
