/*
 * gen_image.c — makes a deterministic test image so nobody has to download
 * anything. Same seed on every machine, so the histogram is reproducible and
 * two students can compare numbers.
 *
 *   ./bin/gen_image data/input.ppm 4000 3000
 */
#include "ppm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Small deterministic PRNG: the same on every compiler and platform. */
static unsigned int seed = 20260101u;
static unsigned int next_rand(void)
{
    seed = seed * 1103515245u + 12345u;
    return (seed >> 16) & 0x7FFFu;
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "data/input.ppm";
    const int width  = (argc > 2) ? atoi(argv[2]) : 4000;
    const int height = (argc > 3) ? atoi(argv[3]) : 3000;

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "usage: %s <out.ppm> <width> <height>\n", argv[0]);
        return EXIT_FAILURE;
    }

    image_t img;
    if (img_alloc(&img, width, height) < 0)
        return EXIT_FAILURE;

    const double cx = width / 2.0, cy = height / 2.0;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double dx = x - cx, dy = y - cy;
            const double d = sqrt(dx * dx + dy * dy);

            /* Concentric rings give plenty of edges for Sobel to find. */
            const int ring = ((int)(d / 40.0) % 2 == 0) ? 190 : 60;

            /* A diagonal gradient, so the histogram is not two spikes. */
            const int grad = (int)(80.0 * (x + y) / (double)(width + height));

            /* A little noise, so the edge map is not perfectly smooth. */
            const int noise = (int)(next_rand() % 25u) - 12;

            int r = ring + grad + noise;
            int g = ring - grad / 2 + noise;
            int b = 255 - ring + noise;

            if (r < 0) { r = 0; } else if (r > 255) { r = 255; }
            if (g < 0) { g = 0; } else if (g > 255) { g = 255; }
            if (b < 0) { b = 0; } else if (b > 255) { b = 255; }

            const size_t i = ((size_t)y * width + x) * 3u;
            img.pixels[i + 0] = (unsigned char)r;
            img.pixels[i + 1] = (unsigned char)g;
            img.pixels[i + 2] = (unsigned char)b;
        }
    }

    if (ppm_write(path, &img) < 0) {
        img_free(&img);
        return EXIT_FAILURE;
    }

    printf("wrote %s  (%d x %d, %.1f MB)\n", path, width, height,
           (double)width * height * 3.0 / 1e6);

    img_free(&img);
    return EXIT_SUCCESS;
}
