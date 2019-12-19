#ifndef ALLWINNER_A10_PIT_H
#define ALLWINNER_A10_PIT_H

#include "hw/ptimer.h"
#include "hw/sysbus.h"

#define TYPE_AW_A10_PIT "allwinner-A10-timer"

#define AW_PIT_TIMER_MAX        6

typedef struct AllwinnerTmrCtrlState AllwinnerTmrCtrlState;

typedef struct AllwinnerTmrState {
    AllwinnerTmrCtrlState *container;
    int index;
    ptimer_state *ptimer;
    qemu_irq irq;
    uint32_t control;
    uint32_t interval;
    uint32_t count;
} AllwinnerTmrState;

struct AllwinnerTmrCtrlState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/
    size_t timer_count;
    AllwinnerTmrState timer[AW_PIT_TIMER_MAX];
    MemoryRegion iomem;
    uint32_t clk_freq[4];

    uint32_t irq_enable;
    uint32_t irq_status;
    uint32_t watch_dog_mode;
    uint32_t watch_dog_control;
    uint32_t count_lo;
    uint32_t count_hi;
    uint32_t count_ctl;
};

#endif
