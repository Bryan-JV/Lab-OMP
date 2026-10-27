/*
 * main_omp.c — YOUR FILE. This is the only one you have to modify.
 *
 *   ./bin/pipeline_omp data/input.ppm [threads]
 *
 * It compiles and runs as handed to you, but every stage still calls the
 * serial version, so it is exactly as slow as the baseline. Your job is to
 * replace the four TODOs.
 *
 * Rules:
 *   - do not modify filters.c, ppm.c or main_serial.c
 *   - the edge image must come out byte-for-byte identical to the serial one
 *   - the histogram must match the serial one in all 256 bins
 *
 * Check yourself with:
 *   cmp data/edges_serial.ppm data/edges_omp.ppm
 */
#include "filters.h"
#include "ppm.h"

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =====================================================================
 * TASK 1 — stage 1: grayscale
 *
 * Every output pixel depends only on the input pixel at the same index,
 * so this loop is embarrassingly parallel. One pragma is enough.
 *
 * Ask yourself before writing it: does `i` need to be private? Why?
 * ===================================================================== */
static void grayscale_omp(const image_t *img, unsigned char *gray)
{
    const int n = img->width * img->height;

    #pragma omp parallel for
    for (int i = 0; i < n; ++i) {
        const unsigned char r = img->pixels[i * 3 + 0];
        const unsigned char g = img->pixels[i * 3 + 1];
        const unsigned char b = img->pixels[i * 3 + 2];

        gray[i] = (unsigned char)(0.299 * r + 0.587 * g + 0.114 * b);
    }
}

/* =====================================================================
 * TASK 2 — stage 2: 5x5 Gaussian blur
 *
 * This is the heavy stage, and the one where parallelising pays most.
 * It reads a 5x5 neighbourhood of `gray`, so stage 1 must be completely
 * finished before the first thread starts here.
 *
 * Question to answer in your report: where is the barrier that guarantees
 * that? You should not need to write #pragma omp barrier anywhere.
 *
 * Hint: the two nested loops can be merged with collapse(2).
 * ===================================================================== */


static const int GAUSS5[5][5] = {
    {  1,  4,  6,  4,  1 },
    {  4, 16, 24, 16,  4 },
    {  6, 24, 36, 24,  6 },
    {  4, 16, 24, 16,  4 },
    {  1,  4,  6,  4,  1 },
};

static void blur_omp(const unsigned char *gray, unsigned char *out,
                     int width, int height)
{
   
    #pragma omp parallel for collapse(2)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int i = y * width + x;

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

/* =====================================================================
 * TASK 3 — stage 3: Sobel
 *
 * Same shape as the blur: reads a neighbourhood, writes one pixel.
 * ===================================================================== */
static void sobel_omp(const unsigned char *blurred, unsigned char *edges,
                      int width, int height){

    for (int x = 0; x < width; ++x) {
        edges[x] = 0;
        edges[(height - 1) * width + x] = 0;
    }
    for (int y = 0; y < height; ++y) {
        edges[y * width] = 0;
        edges[y * width + width - 1] = 0;
    }

    #pragma omp parallel for collapse(2)
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const int i = y * width + x;

            const int tl = blurred[i - width - 1], tc = blurred[i - width], tr = blurred[i - width + 1];
            const int ml = blurred[i - 1],                                  mr = blurred[i + 1];
            const int bl = blurred[i + width - 1], bc = blurred[i + width], br = blurred[i + width + 1];

            const int gx = (tr + 2 * mr + br) - (tl + 2 * ml + bl);
            const int gy = (bl + 2 * bc + br) - (tl + 2 * tc + tr);


            int mag = abs(gx) + abs(gy);
            if (mag > 255)
                mag = 255;

            edges[i] = (unsigned char)mag;
        }
    }
}

/* =====================================================================
 * TASK 4 — stage 4: the histogram, four ways
 *
 * All four write into the same 256 bins from every thread. Implement them
 * in this order and run each one, because the point of the exercise is the
 * comparison, not any single version.
 *
 *   A. naive      just add the pragma, protect nothing.
 *                 Run it with 1 thread, then with 4. Explain what you see.
 *
 *   B. critical   put #pragma omp critical around the increment.
 *                 Correct. Time it and compare against A.
 *
 *   C. atomic     use #pragma omp atomic instead.
 *                 Also correct. Why is it faster than critical?
 *
 *   D. reduction  reduction(+ : hist[:HIST_BINS]) on the loop.
 *                 Needs OpenMP 4.5, which gcc has had since version 6.
 * ===================================================================== */

/* A. no protection at all — this one is SUPPOSED to be wrong */
static void histogram_naive(const unsigned char *edges, int n, long *hist)
{
    memset(hist, 0, sizeof(long) * HIST_BINS);

    #pragma omp parallel for
    for (int i = 0; i < n; ++i)
        hist[edges[i]]++;
}

/* B. critical */
static void histogram_critical(const unsigned char *edges, int n, long *hist)
{
    memset(hist, 0, sizeof(long) * HIST_BINS);

    #pragma omp parallel for
    for (int i = 0; i < n; ++i) {
        #pragma omp critical
        hist[edges[i]]++;
    }
}

/* C. atomic */
static void histogram_atomic(const unsigned char *edges, int n, long *hist)
{
    memset(hist, 0, sizeof(long) * HIST_BINS);

    #pragma omp parallel for
    for (int i = 0; i < n; ++i) {
        #pragma omp atomic
        hist[edges[i]]++;
    }
}

/* D. array reduction */
static void histogram_reduction(const unsigned char *edges, int n, long *hist)
{
    memset(hist, 0, sizeof(long) * HIST_BINS);

    #pragma omp parallel for reduction(+ : hist[:HIST_BINS])
    for (int i = 0; i < n; ++i)
        hist[edges[i]]++;
}

/* ===================================================================== */

static double bench(void (*fn)(const unsigned char *, int, long *),
                    const unsigned char *edges, int n, long *hist)
{
    const double t0 = omp_get_wtime();
    fn(edges, n, hist);
    return omp_get_wtime() - t0;
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "data/input.ppm";
    if (argc > 2)
        omp_set_num_threads(atoi(argv[2]));

    /* ---------- strictly serial: load ---------- */
    double t0 = omp_get_wtime();

    image_t img;
    if (ppm_read(path, &img) < 0)
        return EXIT_FAILURE;

    const int n = img.width * img.height;
    unsigned char *gray    = malloc((size_t)n);
    unsigned char *blurred = malloc((size_t)n);
    unsigned char *edges   = malloc((size_t)n);
    long *hist = malloc(sizeof(long) * HIST_BINS);
    long *ref  = malloc(sizeof(long) * HIST_BINS);
    if (gray == NULL || blurred == NULL || edges == NULL ||
        hist == NULL || ref == NULL) {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }

    const double t_load = omp_get_wtime() - t0;

    int threads = 0;
    #pragma omp parallel
    {
        #pragma omp single
        threads = omp_get_num_threads();
    }

    /* ---------- the parallel stages ---------- */
    t0 = omp_get_wtime();
    grayscale_omp(&img, gray);
    const double t_gray = omp_get_wtime() - t0;

    t0 = omp_get_wtime();
    blur_omp(gray, blurred, img.width, img.height);
    const double t_blur = omp_get_wtime() - t0;

    t0 = omp_get_wtime();
    sobel_omp(blurred, edges, img.width, img.height);
    const double t_sobel = omp_get_wtime() - t0;

    /* The serial histogram is the reference every version is checked against. */
    histogram_serial(edges, img.width, img.height, ref);

    const double t_naive     = bench(histogram_naive, edges, n, hist);
    const int    lost_naive  = (int)(histogram_total(ref) - histogram_total(hist));
    const int    bins_naive  = histogram_diff(ref, hist);

    const double t_critical  = bench(histogram_critical, edges, n, hist);
    const int    bins_crit   = histogram_diff(ref, hist);

    const double t_atomic    = bench(histogram_atomic, edges, n, hist);
    const int    bins_atomic = histogram_diff(ref, hist);

    const double t_reduction = bench(histogram_reduction, edges, n, hist);
    const int    bins_red    = histogram_diff(ref, hist);

    /* ---------- strictly serial: save ---------- */
    t0 = omp_get_wtime();
    ppm_write_gray("data/edges_omp.ppm", edges, img.width, img.height);
    const double t_save = omp_get_wtime() - t0;

    const double t_par = t_gray + t_blur + t_sobel + t_reduction;
    const double t_ser = t_load + t_save;
    const double t_tot = t_par + t_ser;

    printf("\nthreads        %d\n", threads);
    printf("image          %d x %d  (%d pixels)\n\n", img.width, img.height, n);

    printf("  load            %7.3f s   (serial)\n", t_load);
    printf("  stage 1 gray    %7.3f s\n", t_gray);
    printf("  stage 2 blur    %7.3f s\n", t_blur);
    printf("  stage 3 sobel   %7.3f s\n", t_sobel);
    printf("  stage 4 hist    %7.3f s   (reduction)\n", t_reduction);
    printf("  save            %7.3f s   (serial)\n", t_save);
    printf("  ----------------------------\n");
    printf("  total           %7.3f s\n\n", t_tot);

    printf("stage 4, four ways\n");
    printf("  %-11s %8.3f s   wrong bins %3d   counts lost %d\n",
           "naive", t_naive, bins_naive, lost_naive);
    printf("  %-11s %8.3f s   wrong bins %3d\n", "critical",  t_critical,  bins_crit);
    printf("  %-11s %8.3f s   wrong bins %3d\n", "atomic",    t_atomic,    bins_atomic);
    printf("  %-11s %8.3f s   wrong bins %3d\n", "reduction", t_reduction, bins_red);

    printf("\nparallelisable  %7.3f s   P = %.3f\n", t_par, t_par / t_tot);
    printf("strictly serial %7.3f s   1 - P = %.3f\n", t_ser, t_ser / t_tot);
    printf("Amdahl ceiling  %7.2fx\n\n", t_tot / t_ser);

    free(ref);
    free(hist);
    free(edges);
    free(blurred);
    free(gray);
    img_free(&img);
    return EXIT_SUCCESS;
}
