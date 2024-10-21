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

typedef enum {
	BLACK 	= 0,
	RED 		= 1,
	GREEN 	= 2,
	YELLOW 	= 3,
	BLUE 		= 4,
	MAGENTA = 5,
	CYAN 		= 6,
	WHITE 	= 7
} TERM_COLOR;

typedef enum {
	TEXT = 0,
	BOLD_TEXT,
	UNDERLINE_TEXT,
	BACKGROUND,
	HIGH_INTEN_BG,
	HIGH_INTEN_TEXT,
	BOLD_HIGH_INTEN_TEXT,
	RESET
} TERM_KIND;

#define writef(...) ({  writef_function(__VA_ARGS__, NULL); })
#define INFO(...) printf("%s[INFO]:%s %s\n", get_term_color(TEXT, GREEN), get_term_color(RESET, 0), writef(__VA_ARGS__))
#define WARN(...) printf("%s[WARN]:%s %s\n", get_term_color(TEXT, YELLOW), get_term_color(RESET, 0), writef(__VA_ARGS__))
#define ERROR(...) printf("%s[ERROR]:%s %s\n", get_term_color(TEXT, RED), get_term_color(RESET, 0), writef(__VA_ARGS__)), exit(1);

/*
 * writef_function(char *, args) 
 * It works like printf but it returns string.
*/
char *writef_function(char *s, ...);

/*
 * get_term_color(TERM_KIND, TERM_COLOR)
 * return terminal color string
*/
const char *get_term_color(TERM_KIND kind, TERM_COLOR color)
{
	switch (kind)
	{
		case TEXT: return writef("\e[0;3%dm", color);
		case BOLD_TEXT: return writef("\e[1;3%dm", color);
		case UNDERLINE_TEXT: return writef("\e[4;3%dm", color);
		case BACKGROUND: return writef("\e[4%dm", color);
		case HIGH_INTEN_BG: return writef("\e[0;10%dm", color);
		case HIGH_INTEN_TEXT: return writef("\e[0;9%dm", color);
		case BOLD_HIGH_INTEN_TEXT: return writef("\e[1;9%dm", color);
		case RESET: return writef("\e[0m");
	}
}

char *writef_function(char *s, ...)
{
	// allocate small size buffer
	size_t buffer_size = 64; // 64 bytes
	char *buffer = (char*)malloc(buffer_size);

	if (buffer == NULL)
	{
		WARN("writef: Failed to allocate buffer.");
		return NULL;
	}

	va_list ap;
	va_start(ap, s);

	int nSize = vsnprintf(buffer, buffer_size, s, ap);
	if (nSize < 0)
	{
		free(buffer);
		va_end(ap);
	}

	// if buffer does not have enough space then extend it.
	if (nSize >= buffer_size)
	{
		buffer_size = nSize + 1;
		buffer = (char*)realloc(buffer, buffer_size);

		if (buffer == NULL)
		{
			WARN("writef: Failed to re-allocate buffer.");
			return NULL;
		}

		va_end(ap);

		va_start(ap, s);
		vsnprintf(buffer, buffer_size, s, ap);
	}

	va_end(ap);

	return buffer;
}


/** remove connect to enable debug which will 
		print information about resources and connector. **/

// #define DEBUG

int main(int argc, char **argv)
{
	/** check if user has provided the dri device or not. **/
	if (argc < 2)
	{
		printf("Err: provide dri device.\n");
		return -EINVAL;
	}

	/** the first argument must be a dri device, else it might fail. **/
	const char *card = argv[1];

	/** open dri device in read and write mode **/
	int fd = open(card, O_RDWR | O_CLOEXEC);
	if (fd < 0)
	{
		perror("Err: failed to open dri device: ");
		return -EINVAL;
	}

	INFO("Successfully opened dri device: %s", card);

	/** get resource information from dri device **/
	drmModeResPtr res = drmModeGetResources(fd);
	if (!res)
	{
		perror("Err: cannot get drm resources: ");
		close(fd);
		return -EINVAL;
	}

	INFO("Successfully got drm resources");

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

	/** get current connector from resource **/

	/**
	 * NOTE: I only care about first connector,
	 * as i am running this in virtual machine,
	 * so it only shows 1 connector.
	**/
	drmModeConnectorPtr connector = drmModeGetConnectorCurrent(fd, res->connectors[0]);
	if (!connector)
	{
		perror("Err: failed to get connector: ");
		drmModeFreeResources(res);
		close(fd);
		return -EINVAL;
	}

	INFO("Successfully got connector");

	/** find appropriate mode for finding resolution **/
	drmModeModeInfoPtr resolution = 0;
	for (int i = 0; i < connector->count_modes; i++) {
		resolution = &connector->modes[i];
		if (resolution->type & DRM_MODE_TYPE_PREFERRED)
			break;
	}

	INFO("Mode has been selected");

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
	struct drm_mode_map_dumb mreq;
	uint32_t fb;
	int ret;
	uint8_t *map;

	/** create dumb buffer and fill with details **/
	memset(&creq, 0, sizeof(creq));
	creq.width = resolution->hdisplay;
	creq.height = resolution->vdisplay;
	creq.bpp = 32;

	/** fill it with appropriate data **/
	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
	if (ret < 0)
	{
		perror("err: Failed to create dumb buffer: ");
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		close(fd);
		return -EINVAL;
	}

	/** get framebuffer id **/
	ret = drmModeAddFB(fd, creq.width, creq.height, 24, creq.bpp, creq.pitch, creq.handle, &fb);
	if (ret)
	{
		perror("err: Failed to create framebuffer: ");
		ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &creq.handle);
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		close(fd);
		return -EINVAL;
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.handle = creq.handle;

	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	if (ret)
	{
		perror("err: DRM buffer preparation failed: ");
		drmModeRmFB(fd, fb);
		ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &creq.handle);
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		close(fd);
		return -EINVAL;
	}

	map = mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
	if (map == MAP_FAILED)
	{
		perror("err: Memory-mapping failed: ");
		drmModeRmFB(fd, fb);
		ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &creq.handle);
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		close(fd);
		return -EINVAL;
	}

	/* clear framebuffer to 0 */
	memset(map, 0, creq.size);

	INFO("Memory allocate for frameBuffer");

	/* get encoder */
	drmModeEncoderPtr encoder = drmModeGetEncoder(fd, connector->encoder_id);
	if (!encoder)
	{
		perror("err: Failed to get encoder: ");
		munmap(map, creq.size);
		drmModeRmFB(fd, fb);
		ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &creq.handle);
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		close(fd);
		return -EINVAL;
	}

	/* get crtc */
	drmModeCrtcPtr crtc = drmModeGetCrtc(fd, encoder->crtc_id);
	if (!crtc)
	{
		perror("err: Failed to get crtc: ");
		drmModeFreeEncoder(encoder);
		munmap(map, creq.size);
		drmModeRmFB(fd, fb);
		ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &creq.handle);
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		close(fd);
		return -EINVAL;
	}

	INFO("Successfully got Encoder & CRTC");

	// Fill the framebuffer with a solid color (e.g., red)
	for (uint32_t y = 0; y < creq.height; y++) {
		for (uint32_t x = 0; x < creq.width; x++) {
			uint32_t pixel_offset = (y * creq.pitch) + (x * 4);
			map[pixel_offset] = 0xFF;        // Blue
			map[pixel_offset + 1] = 0xbb;    // Green
			map[pixel_offset + 2] = 0xaa;    // Red
			map[pixel_offset + 3] = 0xFF;    // Alpha
		}
	}

	INFO("Modified color to the framebuffer");

	// Set the CRTC
	if (drmModeSetCrtc(fd, crtc->crtc_id, fb, 0, 0, &connector->connector_id, 1, resolution))
	{
		perror("err: Failed to set CRTC: ");
		drmModeFreeCrtc(crtc);
		drmModeFreeEncoder(encoder);
		munmap(map, creq.size);
		drmModeRmFB(fd, fb);
		ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &creq.handle);
		drmModeFreeConnector(connector);
		drmModeFreeResources(res);
		close(fd);
		return -EINVAL;
	}

	getchar();

	INFO("Leaving now...");

	drmModeFreeCrtc(crtc);
	drmModeFreeEncoder(encoder);
	munmap(map, creq.size);
	drmModeRmFB(fd, fb);
	ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &creq.handle);
	drmModeFreeConnector(connector);
	drmModeFreeResources(res);
	close(fd);

	return 0;
}
