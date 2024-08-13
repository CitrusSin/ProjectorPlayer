#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "framebuffer.h"

int main(int argc, char **argv) {
    char dev_name[64] = "/dev/fb1";

    if (argc >= 2) {
        strncpy(dev_name, argv[1], sizeof(dev_name)-1);
        dev_name[sizeof(dev_name)-1] = '\0';
    }

    fb_handler fb = fb_init(dev_name);
    if (fb == NULL) {
        fprintf(stderr, "Framebuffer %s init failed: %s\n", dev_name, strerror(errno));
        return 0;
    }

    const struct fb_var_screeninfo *fb_inf = &fb->fb_inf;

    printf("Resolution: %dx%d\n", fb_inf->xres, fb_inf->yres);
    printf("RGB: %d %d %d\n", fb_inf->red.length, fb_inf->green.length, fb_inf->blue.length);

    unsigned int colors[3] = {
        fb_color(fb, 0xFF0000),
        fb_color(fb, 0x00FF00),
        fb_color(fb, 0x0000FF)
    };

    for (int i=0; i<10; i++) {
        fb_fill(fb, colors[i%3]);
        sleep(1);
    }

    if (fb_release(fb) != 0) {
        fprintf(stderr, "Failed to release framebuffer %s: %s\n", dev_name, strerror(errno));
    }
    return 0;
}