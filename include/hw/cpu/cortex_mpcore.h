/*
 * Cortex-MPCore internal peripheral emulation.
 *
 * Copyright (c) 2009 CodeSourcery.
 * Copyright (c) 2011, 2012, 2023 Linaro Limited.
 * Written by Paul Brook, Peter Maydell.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_CPU_CORTEX_MPCORE_H
#define HW_CPU_CORTEX_MPCORE_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "hw/intc/arm_gic.h"
#include "hw/misc/a9scu.h"
#include "hw/timer/a9gtimer.h"
#include "hw/timer/arm_mptimer.h"

/*
 * This is a model of the Arm Cortex-A MPCore family of hardware.
 *
 * The A7 and A15 MPCore contain:
 *  up to 4 Cortex-A cores
 *  a GIC
 * The A9 MPCore additionally contains:
 *  a System Control Unit
 *  some timers and watchdogs
 *
 * QEMU interface:
 *  + QOM property "num-cores" which set the number of cores present in
 *    the cluster.
 *  + QOM properties "cpu-has-el3", "cpu-has-el2" which set whether the CPUs
 *    have the exception level features present.
 *  + QOM property "gic-spi-num" sets the number of GIC Shared Peripheral
 *    Interrupts.
 */
#define TYPE_CORTEX_MPCORE_PRIV "cortex_mpcore_priv"
OBJECT_DECLARE_TYPE(CortexMPPrivState, CortexMPPrivClass, CORTEX_MPCORE_PRIV)

/**
 * CortexMPPrivClass:
 * @container_size - size of the device's MMIO region
 * @gic_spi_default - default number of GIC SPIs
 * @gic_spi_max - maximum number of GIC SPIs
 */
struct CortexMPPrivClass {
    SysBusDeviceClass parent_class;

    DeviceRealize parent_realize;

    uint64_t container_size;

    unsigned gic_spi_default;
    unsigned gic_spi_max;
};

struct CortexMPPrivState {
    SysBusDevice parent_obj;

    MemoryRegion container;

    /* Properties */
    uint32_t num_cores;

    bool cpu_has_el3;
    bool cpu_has_el2;

    uint32_t gic_spi_num;
};

#define TYPE_A9MPCORE_PRIV "a9mpcore_priv"
OBJECT_DECLARE_SIMPLE_TYPE(A9MPPrivState, A9MPCORE_PRIV)

struct A9MPPrivState {
    CortexMPPrivState parent_obj;

    A9SCUState scu;
    GICState gic;
    A9GTimerState gtimer;
    ARMMPTimerState mptimer;
    ARMMPTimerState wdt;
};

#define TYPE_A15MPCORE_PRIV "a15mpcore_priv"
OBJECT_DECLARE_SIMPLE_TYPE(A15MPPrivState, A15MPCORE_PRIV)

struct A15MPPrivState {
    CortexMPPrivState parent_obj;

    GICState gic;
};

#endif
