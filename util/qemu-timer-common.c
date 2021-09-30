/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"
#include "qemu/timer.h"

/***********************************************************/
/* real time host monotonic timer */

int64_t clock_start;

#ifdef _WIN32

int64_t clock_freq;

static void __attribute__((constructor)) init_get_clock(void)
{
    LARGE_INTEGER freq;
    int ret;
    ret = QueryPerformanceFrequency(&freq);
    if (ret == 0) {
        fprintf(stderr, "Could not calibrate ticks\n");
        exit(1);
    }
    clock_freq = freq.QuadPart;
    clock_start = get_clock();
}

#else

int use_rt_clock;
clockid_t rt_clock;

static void __attribute__((constructor)) init_get_clock(void)
{
    struct timespec ts;

    use_rt_clock = 0;
#if (defined(__APPLE__) || defined(__linux__)) && defined(CLOCK_MONOTONIC_RAW)
    /* CLOCK_MONOTONIC_RAW is not available on all platforms or with all
     * compiler flags.
     */
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0) {
        rt_clock = CLOCK_MONOTONIC_RAW;
        use_rt_clock = 1;
    } else
#endif
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        rt_clock = CLOCK_MONOTONIC;
        use_rt_clock = 1;
    }
    clock_start = get_clock();
}
#endif
