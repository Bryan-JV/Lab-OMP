/*
 * main_serial.c — the baseline. Do not modify: this is what you compare
 * against, and it also writes the reference histogram your parallel version
 * has to reproduce exactly.
 *
 *   ./bin/pipeline_serial data/input.ppm
 */
#include "filters.h"
#include "ppm.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "data/input.ppm";

    /* ---------- serial section: load ---------- */
    double t0 = now_seconds();

    image_t img;
    if (ppm_read(path, &img) < 0)
        return EXIT_FAILURE;

    const int n = img.width * img.height;
    unsigned char *gray    = malloc((size_t)n);
    unsigned char *blurred = malloc((size_t)n);
    unsigned char *edges   = malloc((size_t)n);
    long *hist = malloc(sizeof(long) * HIST_BINS);
    if (gray == NULL || blurred == NULL || edges == NULL || hist == NULL) {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }

    double t_load = now_seconds() - t0;

    /* ---------- stage 1 ---------- */
    t0 = now_seconds();
    grayscale_serial(&img, gray);
    double t_gray = now_seconds() - t0;

    /* ---------- stage 2 ---------- */
    t0 = now_seconds();
    blur_serial(gray, blurred, img.width, img.height);
    double t_blur = now_seconds() - t0;

    /* ---------- stage 3 ---------- */
    t0 = now_seconds();
    sobel_serial(blurred, edges, img.width, img.height);
    double t_sobel = now_seconds() - t0;

    /* ---------- stage 4 ---------- */
    t0 = now_seconds();
    histogram_serial(edges, img.width, img.height, hist);
    double t_hist = now_seconds() - t0;

    /* ---------- serial section: save ---------- */
    t0 = now_seconds();
    ppm_write_gray("data/edges_serial.ppm", edges, img.width, img.height);

    FILE *f = fopen("data/hist_serial.txt", "w");
    if (f != NULL) {
        for (int b = 0; b < HIST_BINS; ++b)
            fprintf(f, "%d %ld\n", b, hist[b]);
        fclose(f);
    }
    double t_save = now_seconds() - t0;

    const double t_parallelisable = t_gray + t_blur + t_sobel + t_hist;
    const double t_serial_part = t_load + t_save;
    const double t_total = t_parallelisable + t_serial_part;

    printf("\nimage          %d x %d  (%d pixels)\n", img.width, img.height, n);
    printf("histogram sum  %ld   (must equal the pixel count)\n\n",
           histogram_total(hist));

    printf("  load            %7.3f s\n", t_load);
    printf("  stage 1 gray    %7.3f s\n", t_gray);
    printf("  stage 2 blur    %7.3f s\n", t_blur);
    printf("  stage 3 sobel   %7.3f s\n", t_sobel);
    printf("  stage 4 hist    %7.3f s\n", t_hist);
    printf("  save            %7.3f s\n", t_save);
    printf("  ----------------------------\n");
    printf("  total           %7.3f s\n\n", t_total);

    printf("parallelisable  %7.3f s   P = %.3f\n",
           t_parallelisable, t_parallelisable / t_total);
    printf("strictly serial %7.3f s   1 - P = %.3f\n",
           t_serial_part, t_serial_part / t_total);
    printf("Amdahl ceiling  %7.2fx\n\n", t_total / t_serial_part);

    free(hist);
    free(edges);
    free(blurred);
    free(gray);
    img_free(&img);
    return EXIT_SUCCESS;
}
