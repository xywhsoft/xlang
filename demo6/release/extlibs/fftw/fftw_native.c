/* fftw_native.c - FFTW-style FFT helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Calculate next power of 2 >= n */
long long xfftw_next_pow2(long long n) {
    long long p = 1;
    while (p < n) p <<= 1;
    return p;
}

/* Check if n is power of 2 */
long long xfftw_is_pow2(long long n) {
    return (n > 0 && (n & (n - 1)) == 0) ? 1 : 0;
}

/* Calculate frequency bin resolution */
double xfftw_freq_resolution(double sample_rate, long long fft_size) {
    return sample_rate / (double)fft_size;
}

/* Calculate bin index for frequency */
long long xfftw_freq_to_bin(double freq, double sample_rate, long long fft_size) {
    return (long long)(freq * fft_size / sample_rate + 0.5);
}

/* Calculate frequency for bin index */
double xfftw_bin_to_freq(long long bin, double sample_rate, long long fft_size) {
    return (double)bin * sample_rate / (double)fft_size;
}

/* Calculate magnitude from real/imag */
double xfftw_magnitude(double real, double imag) {
    return sqrt(real * real + imag * imag);
}

/* Calculate phase from real/imag */
double xfftw_phase(double real, double imag) {
    return atan2(imag, real);
}

/* Calculate power (magnitude squared) */
double xfftw_power(double real, double imag) {
    return real * real + imag * imag;
}

/* Convert magnitude to dB */
double xfftw_to_db(double magnitude) {
    if (magnitude <= 0) return -1000.0;
    return 20.0 * log10(magnitude);
}

/* Window function names */
const char *xfftw_window_name(long long wtype) {
    switch(wtype) {
        case 0: return "RECTANGULAR";
        case 1: return "HANN";
        case 2: return "HAMMING";
        case 3: return "BLACKMAN";
        case 4: return "BARTLETT";
        default: return "UNKNOWN";
    }
}

/* Calculate Hann window coefficient */
double xfftw_hann(long long n, long long N) {
    return 0.5 * (1.0 - cos(2.0 * M_PI * n / (N - 1)));
}

/* Calculate Hamming window coefficient */
double xfftw_hamming(long long n, long long N) {
    return 0.54 - 0.46 * cos(2.0 * M_PI * n / (N - 1));
}

/* FFT plan flags */
long long xfftw_flag_estimate(void) { return 1 << 6; }
long long xfftw_flag_measure(void) { return 0; }
long long xfftw_flag_patient(void) { return 1 << 5; }
long long xfftw_flag_exhaustive(void) { return 1 << 3; }

/* Transform direction */
long long xfftw_forward(void) { return -1; }
long long xfftw_backward(void) { return 1; }

/* Estimate computation complexity */
long long xfftw_complexity(long long n) {
    /* O(n log n) */
    long long log2n = 0, temp = n;
    while (temp > 1) { temp >>= 1; log2n++; }
    return n * log2n;
}
