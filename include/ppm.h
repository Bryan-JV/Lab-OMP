/*
 * ppm.h — minimal binary PPM (P6) reader and writer.
 *
 * You do not need to modify this file. PPM is used because it needs no
 * external libraries: any image viewer on Linux and macOS opens it, and
 * `convert` or Preview will turn it into PNG if you want to hand one in.
 */
#ifndef PPM_H
#define PPM_H

#include <stddef.h>

typedef struct {
    int width;
    int height;
    unsigned char *pixels;   /* width * height * 3 bytes, RGB interleaved */
} image_t;

/* Allocate an image. Returns 0 on success, -1 on failure. */
int img_alloc(image_t *img, int width, int height);

/* Read a binary PPM (P6). Returns 0 on success, -1 on failure. */
int ppm_read(const char *path, image_t *img);

/* Write a binary PPM (P6). Returns 0 on success, -1 on failure. */
int ppm_write(const char *path, const image_t *img);

/* Write a single-channel buffer as a greyscale PPM. */
int ppm_write_gray(const char *path, const unsigned char *gray,
                   int width, int height);

void img_free(image_t *img);

#endif /* PPM_H */
