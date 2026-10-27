#include "filters.h"

#include <stdlib.h>

void grayscale_serial(const image_t *img, unsigned char *gray)
{
    const int n = img->width * img->height;

    for (int i = 0; i < n; ++i) {
        const unsigned char r = img->pixels[i * 3 + 0];
        const unsigned char g = img->pixels[i * 3 + 1];
        const unsigned char b = img->pixels[i * 3 + 2];

        /* Rec. 601 luminance */
        gray[i] = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
    }
}

/* 5x5 Gaussian kernel, integer weights summing to 256 so the division is
 * a shift. This is the heavy stage: 25 multiply-adds per pixel. */
static const int GAUSS5[5][5] = {
    {  1,  4,  6,  4,  1 },
    {  4, 16, 24, 16,  4 },
    {  6, 24, 36, 24,  6 },
    {  4, 16, 24, 16,  4 },
    {  1,  4,  6,  4,  1 },
};

void blur_serial(const unsigned char *gray, unsigned char *out,
                 int width, int height)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int i = y * width + x;

            /* Near the border there is no full window: copy the pixel. */
            if (y < 2 || y >= height - 2 || x < 2 || x >= width - 2) {
                out[i] = gray[i];
                continue;
            }

            int acc = 0;
            for (int ky = -2; ky <= 2; ++ky)
                for (int kx = -2; kx <= 2; ++kx)
                    acc += GAUSS5[ky + 2][kx + 2] * gray[(y + ky) * width + (x + kx)];

            out[i] = (unsigned char)(acc / 256);
        }
    }
}

void sobel_serial(const unsigned char *gray, unsigned char *edges,
                  int width, int height)
{
    /* Clear the border: the 3x3 window does not fit there. */
    for (int x = 0; x < width; ++x) {
        edges[x] = 0;
        edges[(height - 1) * width + x] = 0;
    }
    for (int y = 0; y < height; ++y) {
        edges[y * width] = 0;
        edges[y * width + width - 1] = 0;
    }

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const int i = y * width + x;

            const int tl = gray[i - width - 1], tc = gray[i - width], tr = gray[i - width + 1];
            const int ml = gray[i - 1],                               mr = gray[i + 1];
            const int bl = gray[i + width - 1], bc = gray[i + width], br = gray[i + width + 1];

            const int gx = (tr + 2 * mr + br) - (tl + 2 * ml + bl);
            const int gy = (bl + 2 * bc + br) - (tl + 2 * tc + tr);

            /* Manhattan approximation of sqrt(gx^2 + gy^2): cheaper and
             * accurate enough for an edge map. */
            int mag = abs(gx) + abs(gy);
            if (mag > 255)
                mag = 255;

            edges[i] = (unsigned char)mag;
        }
    }
}

void histogram_serial(const unsigned char *edges, int width, int height,
                      long *hist)
{
    for (int b = 0; b < HIST_BINS; ++b)
        hist[b] = 0;

    const int n = width * height;
    for (int i = 0; i < n; ++i)
        hist[edges[i]]++;
}

int histogram_diff(const long *a, const long *b)
{
    int differing = 0;
    for (int i = 0; i < HIST_BINS; ++i)
        if (a[i] != b[i])
            differing++;
    return differing;
}

long histogram_total(const long *hist)
{
    long total = 0;
    for (int i = 0; i < HIST_BINS; ++i)
        total += hist[i];
    return total;
}
