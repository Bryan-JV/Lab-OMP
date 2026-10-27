#include "ppm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int img_alloc(image_t *img, int width, int height)
{
    if (width <= 0 || height <= 0)
        return -1;

    img->width = width;
    img->height = height;
    img->pixels = malloc((size_t)width * (size_t)height * 3u);
    if (img->pixels == NULL) {
        fprintf(stderr, "img_alloc: out of memory\n");
        return -1;
    }
    return 0;
}

void img_free(image_t *img)
{
    free(img->pixels);
    img->pixels = NULL;
    img->width = 0;
    img->height = 0;
}

/* Skip whitespace and # comments, which a PPM header may contain anywhere. */
static int skip_blanks(FILE *f)
{
    int c;
    for (;;) {
        c = fgetc(f);
        if (c == '#') {
            while (c != '\n' && c != EOF)
                c = fgetc(f);
        } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return c;
        }
    }
}

static int read_int(FILE *f, int *out)
{
    int c = skip_blanks(f);
    if (c < '0' || c > '9')
        return -1;

    int value = 0;
    while (c >= '0' && c <= '9') {
        value = value * 10 + (c - '0');
        c = fgetc(f);
    }
    *out = value;
    return 0;
}

int ppm_read(const char *path, image_t *img)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        return -1;
    }

    char magic[3] = {0};
    if (fread(magic, 1, 2, f) != 2 || strcmp(magic, "P6") != 0) {
        fprintf(stderr, "%s: not a binary PPM (expected P6)\n", path);
        fclose(f);
        return -1;
    }

    int w = 0, h = 0, maxval = 0;
    if (read_int(f, &w) < 0 || read_int(f, &h) < 0 || read_int(f, &maxval) < 0) {
        fprintf(stderr, "%s: malformed header\n", path);
        fclose(f);
        return -1;
    }
    if (maxval != 255) {
        fprintf(stderr, "%s: only 8-bit PPM is supported (maxval=%d)\n",
                path, maxval);
        fclose(f);
        return -1;
    }

    if (img_alloc(img, w, h) < 0) {
        fclose(f);
        return -1;
    }

    size_t n = (size_t)w * (size_t)h * 3u;
    if (fread(img->pixels, 1, n, f) != n) {
        fprintf(stderr, "%s: truncated pixel data\n", path);
        img_free(img);
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

int ppm_write(const char *path, const image_t *img)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        return -1;
    }

    fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);

    size_t n = (size_t)img->width * (size_t)img->height * 3u;
    if (fwrite(img->pixels, 1, n, f) != n) {
        perror(path);
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

int ppm_write_gray(const char *path, const unsigned char *gray,
                   int width, int height)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        perror(path);
        return -1;
    }

    fprintf(f, "P6\n%d %d\n255\n", width, height);

    /* Expand one channel to RGB in memory, then write it in one call.
     * Twelve million three-byte fwrite calls would dominate the whole run. */
    size_t n = (size_t)width * (size_t)height;
    unsigned char *rgb = malloc(n * 3u);
    if (rgb == NULL) {
        fprintf(stderr, "ppm_write_gray: out of memory\n");
        fclose(f);
        return -1;
    }
    for (size_t i = 0; i < n; ++i) {
        rgb[i * 3 + 0] = gray[i];
        rgb[i * 3 + 1] = gray[i];
        rgb[i * 3 + 2] = gray[i];
    }
    if (fwrite(rgb, 1, n * 3u, f) != n * 3u) {
        perror(path);
        free(rgb);
        fclose(f);
        return -1;
    }
    free(rgb);

    fclose(f);
    return 0;
}
