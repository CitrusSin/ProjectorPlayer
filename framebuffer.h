#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdlib.h>
#include <stdbool.h>
#include <linux/fb.h>

/* Execution environment struct of this library. */
struct fb_env {
    int fd_fb;
    void *fb_base, *fb_swap;
    int swap_state;
    size_t fb_size;
    struct fb_var_screeninfo fb_inf;
    const struct fb_fix_screeninfo fb_fix;
};

/* Handler is a pointer of an execution environment */
typedef struct fb_env *fb_handler;

/*
 * Opens a framebuffer device and creates an execution context on it.
 * Arguments:
 *     const char *dev_name: Device name of framebuffer device, e.g. /dev/fb0
 * Returns:
 *     fb_handler hd: Handler of a framebuffer library execution context.
 *                    NULL with errno set if failed.
 */
fb_handler fb_init(const char *dev_name);

/*
 * Releases an execution context and closes the relevant framebuffer device.
 * Arguments:
 *     fb_handler hd: Created by calling fb_init(); Handler of the framebuffer library execution context
 * Returns:
 *     int err: 0 if nothing wrong. -1 otherwise.
 */
int fb_release(fb_handler hd);

/*
 * Convert formal RGB888 type color code to the type that the framebuffer device accepts.
 * Arguments:
 *     fb_handler hd:       Created by calling fb_init(); Handler of the framebuffer library execution context
 *     unsigned int color:  Color code in RGB888 type.
 * Returns:
 *     unsigned int color_out:       Color code in the target type, e.g. RGB565 if the framebuffer accepts RGB565
 */
unsigned int fb_color(fb_handler hd, unsigned int color);

/*
 * Set whether double-buffering enabled.
 * Arguments:
 *     fb_handler hd: Created by calling fb_init(); Handler of the framebuffer library execution context
 *     bool enable:   true to enable double-buffering, false otherwise
 * Returns:
 *     int err: 0 if nothing wrong. -1 otherwise.
 */
int fb_set_doublebuffer(fb_handler hd, bool enable);

/*
 * Flush buffer. Only effective and well-formed when double-buffering enabled.
 * Arguments:
 *     fb_handler hd: Created by calling fb_init(); Handler of the framebuffer library execution context
 * Returns:
 *     int err: 0 if nothing wrong. -1 otherwise.
 */
int fb_flush(fb_handler hd);

/*
 * Update the present buffer space to the visible area.
 * Arguments:
 *     fb_handler hd: Created by calling fb_init(); Handler of the framebuffer library execution context
 * This function has no return values.
 */
void fb_update_buffer(fb_handler hd);

/*
 * Set the color of the pixel at position (x, y) 
 * Arguments:
 *     fb_handler hd:       Created by calling fb_init(); Handler of the framebuffer library execution context
 *     int x:               Position X value in pixel, which indicates column
 *     int y:               Position Y value in pixel, which indicates row
 *     unsigned int color:  Color code in the type that fb_color() returns.
 * This function has no return values.
 */
void fb_draw_pixel(fb_handler hd, int x, int y, unsigned int color);

/*
 * Fill the entire frame with the color given in argument.
 * Arguments:
 *     fb_handler hd:       Created by calling fb_init(); Handler of the framebuffer library execution context
 *     unsigned int color:  Color code in the type that fb_color() returns.
 * This function has no return values.
 */
void fb_fill(fb_handler hd, unsigned int color);

#endif