#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <memory.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#include "framebuffer.h"


static inline int fbs_map(fb_handler hd) {
    hd->fb_size = (size_t)hd->fb_inf.xres * hd->fb_inf.yres * hd->fb_inf.bits_per_pixel / 8;

    hd->fb_base = mmap(NULL, hd->fb_size * (hd->buffered ? 2 : 1), PROT_READ | PROT_WRITE, MAP_SHARED, hd->fd_fb, 0);
    if (hd->fb_base == MAP_FAILED) {
        hd->fb_base = NULL;
        return -1;
    }
    if (hd->buffered) {
        hd->fb_swap = ((unsigned char *)hd->fb_base) + hd->fb_size;
    }
    return 0;
}

static inline void fbs_unmap(fb_handler hd) {
    munmap(hd->fb_base, hd->fb_size);
    hd->fb_base = hd->fb_swap = NULL;
}

/*
 * Opens a framebuffer device and creates an execution context on it.
 * Arguments:
 *     const char *dev_name: Device name of framebuffer device, e.g. /dev/fb0
 * Returns:
 *     fb_handler hd: Handler of a framebuffer library execution context.
 *                    NULL with errno set if failed.
 */
fb_handler fb_init(const char* dev_name) {
    fb_handler hd = (fb_handler)malloc(sizeof(struct fb_env));
    if (hd == NULL) {
        return NULL;
    }

    hd->fd_fb = open(dev_name, O_RDWR);

    if (ioctl(hd->fd_fb, FBIOGET_VSCREENINFO, &hd->fb_inf)) {
        goto error_cleanup;
    }

    // Initially no double-buffering
    hd->fb_inf.xres_virtual = hd->fb_inf.xres;
    hd->fb_inf.yres_virtual = hd->fb_inf.yres;
    hd->fb_inf.xoffset = hd->fb_inf.yoffset = 0;
    if (ioctl(hd->fd_fb, FBIOPUT_VSCREENINFO, &hd->fb_inf)) {
        goto error_cleanup;
    }

    hd->fb_size = (size_t)hd->fb_inf.xres * hd->fb_inf.yres * hd->fb_inf.bits_per_pixel / 8;

    // Only supports msb_right with BPP 8, 16 or 32
    if (
        hd->fb_inf.red.msb_right   != 0 ||
        hd->fb_inf.green.msb_right != 0 ||
        hd->fb_inf.blue.msb_right  != 0 ||
        !(
            hd->fb_inf.bits_per_pixel == 8  ||
            hd->fb_inf.bits_per_pixel == 16 ||
            hd->fb_inf.bits_per_pixel == 32
        )
    ) {
        goto error_cleanup;
    }

    hd->buffered = false;
    hd->swap_state = 0;
    hd->fb_base = hd->fb_swap = NULL;
    if (fbs_map(hd) != 0) {
        goto error_cleanup;
    }
    return hd;

error_cleanup:
    close(hd->fd_fb);
    free(hd);
    return NULL;
}


/*
 * Releases an execution context and closes the relevant framebuffer device.
 * Arguments:
 *     fb_handler hd: Created by calling fb_init(); Handler of the framebuffer library execution context
 * Returns:
 *     int err: 0 if nothing wrong. -1 otherwise.
 */
int fb_release(fb_handler hd) {
    fbs_unmap(hd);

    close(hd->fd_fb);
    free(hd);

    return 0;
}


/*
 * Convert formal RGB888 type color code to the type that the framebuffer device accepts.
 * Arguments:
 *     fb_handler hd:       Created by calling fb_init(); Handler of the framebuffer library execution context
 *     unsigned int color:  Color code in RGB888 type.
 * Returns:
 *     unsigned int color_out:       Color code in the target type, e.g. RGB565 if the framebuffer accepts RGB565
 */
unsigned int fb_color(fb_handler hd, unsigned int color) {
    unsigned int    r = (color >> 16) & 0xff,
                    g = (color >> 8)  & 0xff,
                    b = (color)       & 0xff;
    r >>= 8-hd->fb_inf.red.length;
    g >>= 8-hd->fb_inf.green.length;
    b >>= 8-hd->fb_inf.blue.length;
    
    return
        (r << hd->fb_inf.red.offset)   |
        (g << hd->fb_inf.green.offset) |
        (b << hd->fb_inf.blue.offset);
}


/*
 * Set whether double-buffering enabled.
 * Arguments:
 *     fb_handler hd: Created by calling fb_init(); Handler of the framebuffer library execution context
 *     bool enable:   true to enable double-buffering, false otherwise
 * Returns:
 *     int err: 0 if nothing wrong. -1 otherwise.
 */
int fb_set_doublebuffer(fb_handler hd, bool enable, int new_xres, int new_yres) {
    if (new_xres <= 0) new_xres = hd->fb_inf.xres;
    if (new_yres <= 0) new_yres = hd->fb_inf.yres;

    size_t new_px = (size_t) new_xres * new_yres * hd->fb_inf.bits_per_pixel * (enable ? 2 : 1) / 8;

    struct fb_fix_screeninfo fb_fix;
    if (ioctl(hd->fd_fb, FBIOGET_FSCREENINFO, &fb_fix)) {
        return -1;
    }
    if (fb_fix.smem_len < new_px) {
        return -1;
    }

    struct fb_var_screeninfo fb_old_inf = hd->fb_inf;

    fbs_unmap(hd);

    hd->buffered            = enable;
    hd->swap_state          = 0;
    hd->fb_inf.xres         = hd->fb_inf.xres_virtual = new_xres;
    hd->fb_inf.yres         = new_yres;
    hd->fb_inf.yres_virtual = new_yres * (enable ? 2 : 1);
    hd->fb_inf.yoffset      = enable ? hd->fb_inf.yres : 0;

    if (ioctl(hd->fd_fb, FBIOPUT_VSCREENINFO, &hd->fb_inf)) {
        goto error_cleanup;
    }

    hd->fb_size = (size_t)hd->fb_inf.xres * hd->fb_inf.yres * hd->fb_inf.bits_per_pixel / 8;

    if (enable && hd->fb_inf.yres_virtual < 2 * hd->fb_inf.yres) {
        hd->buffered = false;
        goto error_cleanup;
    }

    if (fbs_map(hd) != 0) {
        goto error_cleanup;
    }

    return 0;

error_cleanup:
    hd->fb_inf = fb_old_inf;
    hd->fb_size = (size_t)hd->fb_inf.xres * hd->fb_inf.yres * hd->fb_inf.bits_per_pixel / 8;
    fbs_map(hd);
    return -1;
}


/*
 * Flush buffer. Only effective and well-formed when double-buffering enabled.
 * Arguments:
 *     fb_handler hd: Created by calling fb_init(); Handler of the framebuffer library execution context
 * Returns:
 *     int err: 0 if nothing wrong. -1 otherwise.
 */
int fb_flush(fb_handler hd) {
    if (!hd->buffered) {
        // Swap not enabled
        return -1;
    }

    // Switch frame
    hd->fb_inf.yoffset = hd->swap_state * hd->fb_size;
    hd->swap_state = (hd->swap_state + 1) % 2;

    // Apply switch
    if (ioctl(hd->fd_fb, FBIOPAN_DISPLAY, &hd->fb_inf)) {
        goto error_cleanup;
    }
    
    // Swap pointer
    void *s = hd->fb_swap;
    hd->fb_swap = hd->fb_base;
    hd->fb_base = s;

    return 0;
error_cleanup:
    // Switch back
    hd->fb_inf.yoffset = hd->swap_state * hd->fb_size;
    hd->swap_state = (hd->swap_state + 1) % 2;
    return -1;
}


/*
 * Update the present buffer space to the visible area.
 * Arguments:
 *     fb_handler hd: Created by calling fb_init(); Handler of the framebuffer library execution context
 * This function has no return values.
 */
void fb_update_buffer(fb_handler hd) {
    memcpy(hd->fb_base, hd->fb_swap, hd->fb_size);
}


/*
 * Set the color of the pixel at position (x, y) 
 * Arguments:
 *     fb_handler hd:       Created by calling fb_init(); Handler of the framebuffer library execution context
 *     int x:               Position X value in pixel, which indicates column
 *     int y:               Position Y value in pixel, which indicates row
 *     unsigned int color:  Color code in the type that fb_color() returns.
 * This function has no return values.
 */
void fb_draw_pixel(fb_handler hd, int x, int y, unsigned int color) {
    size_t offset = x + (size_t)y * hd->fb_inf.xres;

    switch (hd->fb_inf.bits_per_pixel) {
    case 8:
        ((uint8_t  *)hd->fb_base)[offset] = color;
        break;
    case 16:
        ((uint16_t *)hd->fb_base)[offset] = color;
        break;
    case 32:
        ((uint32_t *)hd->fb_base)[offset] = color;
        break;
    }
}


/*
 * Fill the entire frame with the color given in argument.
 * Arguments:
 *     fb_handler hd:       Created by calling fb_init(); Handler of the framebuffer library execution context
 *     unsigned int color:  Color code in the type that fb_color() returns.
 * This function has no return values.
 */
void fb_fill(fb_handler hd, unsigned int color) {
    size_t i;

    switch (hd->fb_inf.bits_per_pixel) {
    case 8:
        for (i=0; i<hd->fb_size; i++)
            ((uint8_t  *)hd->fb_base)[i] = color;
        break;
    case 16:
        for (i=0; i<hd->fb_size/2; i++)
            ((uint16_t *)hd->fb_base)[i] = color;
        break;
    case 32:
        for (i=0; i<hd->fb_size/4; i++)
            ((uint32_t *)hd->fb_base)[i] = color;
        break;
    }
}
