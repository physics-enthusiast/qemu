/*
 * Microsemi SmartFusion2 Timer.
 *
 * Copyright (c) 2017 Subbaraya Sundeep <sundeep.lkml@gmail.com>
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

#ifndef HW_MSF2_TIMER_H
#define HW_MSF2_TIMER_H

#include "hw/sysbus.h"
#include "hw/ptimer.h"
#include "sysemu/sysemu.h"

#define TYPE_MSF2_TIMER     "msf2-timer"
#define MSF2_TIMER(obj)     OBJECT_CHECK(MSF2TimerState, \
                              (obj), TYPE_MSF2_TIMER)

/*
 * There are two 32-bit down counting timers.
 * Timers 1 and 2 can be concatenated into a single 64-bit Timer
 * that operates either in Periodic mode or in One-shot mode.
 * Writing 1 to the TIM64_MODE register bit 0 sets the Timers in 64-bit mode.
 * In 64-bit mode, writing to the 32-bit registers has no effect.
 * Similarly, in 32-bit mode, writing to the 64-bit mode registers
 * has no effect. Only two 32-bit timers are supported currently.
 */
#define NUM_TIMERS        2

#define R_TIM_VAL         0
#define R_TIM_LOADVAL     1
#define R_TIM_BGLOADVAL   2
#define R_TIM_CTRL        3
#define R_TIM_RIS         4
#define R_TIM_MIS         5
#define R_TIM1_MAX        6

#define R_TIM_MODE       21
#define R_TIM_MAX        22 /* including 64-bit timer registers */

#define TIMER_CTRL_ENBL     (1 << 0)
#define TIMER_CTRL_ONESHOT  (1 << 1)
#define TIMER_CTRL_INTR     (1 << 2)
#define TIMER_RIS_ACK       (1 << 0)
#define TIMER_RST_CLR       (1 << 6)
#define TIMER_MODE          (1 << 0)

struct Msf2Timer {
    QEMUBH *bh;
    ptimer_state *ptimer;
    int nr; /* for debug. */

    uint32_t regs[R_TIM_MAX];
    qemu_irq irq;
};

typedef struct MSF2TimerState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    uint32_t freq_hz;
    struct Msf2Timer *timers;
} MSF2TimerState;

#endif /* HW_MSF2_TIMER_H */
