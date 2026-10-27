/*
 * filters.h — the four stages of the pipeline.
 *
 * Stage 1  grayscale   every pixel is independent
 * Stage 2  blur        5x5 Gaussian, reads a neighbourhood: stage 1 must be done
 * Stage 3  sobel       3x3 gradient, reads a neighbourhood: stage 2 must be done
 * Stage 4  histogram   every thread writes into the same 256 bins
 *
 * Blurring before taking the gradient is what real edge detectors do: without
 * it, every speck of sensor noise shows up as an edge.
 *
 * The serial versions in filters.c are the reference. Your job is to write
 * the parallel ones in main_omp.c without changing these.
 */
#ifndef FILTERS_H
#define FILTERS_H

#include "ppm.h"

#define HIST_BINS 256

/* Stage 1: RGB -> luminance. gray must hold width * height bytes. */
void grayscale_serial(const image_t *img, unsigned char *gray);

/* Stage 2: 5x5 Gaussian blur. out must hold width * height bytes. */
void blur_serial(const unsigned char *gray, unsigned char *out,
                 int width, int height);

/* Stage 3: Sobel edge magnitude. edges must hold width * height bytes.
 * The one-pixel border is set to 0. */
void sobel_serial(const unsigned char *gray, unsigned char *edges,
                  int width, int height);

/* Stage 4: count edge magnitudes into 256 bins. hist must hold HIST_BINS. */
void histogram_serial(const unsigned char *edges, int width, int height,
                      long *hist);

/* Compare two histograms. Returns the number of bins that differ. */
int histogram_diff(const long *a, const long *b);

/* Sum of all bins — should always equal width * height. */
long histogram_total(const long *hist);

#endif /* FILTERS_H */
