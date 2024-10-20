#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// Function to find a connected connector
drmModeConnector *find_connector(int drm_fd, drmModeRes *resources) {
    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *connector = drmModeGetConnector(drm_fd, resources->connectors[i]);
        if (connector->connection == DRM_MODE_CONNECTED) {
            return connector;
        }
        drmModeFreeConnector(connector);
    }
    return NULL;
}

// Function to find an encoder
drmModeEncoder *find_encoder(int drm_fd, drmModeRes *resources, drmModeConnector *connector) {
    if (connector->encoder_id) {
        return drmModeGetEncoder(drm_fd, connector->encoder_id);
    }
    return NULL;
}

int main() {
    int drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
        fprintf(stderr, "Failed to open DRM device: %s\n", strerror(errno));
        return -1;
    }

    drmModeRes *resources = drmModeGetResources(drm_fd);
    if (!resources) {
        fprintf(stderr, "Failed to get DRM resources: %s\n", strerror(errno));
        close(drm_fd);
        return -1;
    }

    drmModeConnector *connector = find_connector(drm_fd, resources);
    if (!connector) {
        fprintf(stderr, "No connected connector found\n");
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    drmModeEncoder *encoder = find_encoder(drm_fd, resources, connector);
    if (!encoder) {
        fprintf(stderr, "Failed to find encoder\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    drmModeCrtc *crtc = drmModeGetCrtc(drm_fd, encoder->crtc_id);
    if (!crtc) {
        fprintf(stderr, "Failed to get CRTC\n");
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    // Select a mode
    drmModeModeInfo mode = connector->modes[0];

    // Create a dumb buffer
    struct drm_mode_create_dumb create_dumb = {0};
    create_dumb.width = mode.hdisplay;
    create_dumb.height = mode.vdisplay;
    create_dumb.bpp = 32;

    if (ioctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb) < 0) {
        fprintf(stderr, "Failed to create dumb buffer: %s\n", strerror(errno));
        drmModeFreeCrtc(crtc);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    uint32_t fb_id;
    if (drmModeAddFB(drm_fd, create_dumb.width, create_dumb.height, 24, create_dumb.bpp, create_dumb.pitch, create_dumb.handle, &fb_id)) {
        fprintf(stderr, "Failed to create framebuffer: %s\n", strerror(errno));
        ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &create_dumb.handle);
        drmModeFreeCrtc(crtc);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    // Map the dumb buffer
    struct drm_mode_map_dumb map_dumb = {0};
    map_dumb.handle = create_dumb.handle;
    if (ioctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb)) {
        fprintf(stderr, "Failed to map dumb buffer: %s\n", strerror(errno));
        drmModeRmFB(drm_fd, fb_id);
        ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &create_dumb.handle);
        drmModeFreeCrtc(crtc);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    uint8_t *framebuffer = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, map_dumb.offset);
    if (framebuffer == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap framebuffer: %s\n", strerror(errno));
        drmModeRmFB(drm_fd, fb_id);
        ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &create_dumb.handle);
        drmModeFreeCrtc(crtc);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

		printf("width: %d, height: %d\n", create_dumb.width, create_dumb.height);

    // Fill the framebuffer with a solid color (e.g., red)
    for (uint32_t y = 0; y < create_dumb.height; y++) {
        for (uint32_t x = 0; x < create_dumb.width; x++) {
            uint32_t pixel_offset = (y * create_dumb.pitch) + (x * 4);
            framebuffer[pixel_offset] = 0xFF;        // Blue
            framebuffer[pixel_offset + 1] = 0x00;    // Green
            framebuffer[pixel_offset + 2] = 0x00;    // Red
            framebuffer[pixel_offset + 3] = 0xFF;    // Alpha
        }
    }

    // Set the CRTC
    if (drmModeSetCrtc(drm_fd, crtc->crtc_id, fb_id, 0, 0, &connector->connector_id, 1, &mode)) {
        fprintf(stderr, "Failed to set CRTC: %s\n", strerror(errno));
    }

    // Wait for user input to exit
    getchar();

    // Cleanup
    munmap(framebuffer, create_dumb.size);
    drmModeRmFB(drm_fd, fb_id);
    ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &create_dumb.handle);
    drmModeFreeCrtc(crtc);
    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    close(drm_fd);

    return 0;
}
