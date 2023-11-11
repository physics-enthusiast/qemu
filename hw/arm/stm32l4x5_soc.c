/*
 * STM32L4x5 SoC family
 *
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2014 Alistair Francis <alistair@alistair23.me>
 * Copyright (c) 2023 Arnaud Minier <arnaud.minier@telecom-paris.fr>
 * Copyright (c) 2023 Inès Varhol <ines.varhol@telecom-paris.fr>
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
#include "qemu/units.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/arm/stm32l4x5_soc.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"

/* STM32L4x5 SoC family implementation is derived from stm32f405_soc */

#define FLASH_BASE_ADDRESS 0x08000000
#define SRAM1_BASE_ADDRESS 0x20000000
#define SRAM1_SIZE (96 * KiB)
#define SRAM2_BASE_ADDRESS 0x10000000
#define SRAM2_SIZE (32 * KiB)

#define EXTI_ADDR 0x40010400

#define NUM_EXTI_IRQ 40
/* Match exti line connections with their CPU IRQ number */
/* See Vector Table (Reference Manual p.396) */
static const int exti_irq[NUM_EXTI_IRQ] = {
    6,                      /* GPIO[0]                 */
    7,                      /* GPIO[1]                 */
    8,                      /* GPIO[2]                 */
    9,                      /* GPIO[3]                 */
    10,                     /* GPIO[4]                 */
    23, 23, 23, 23, 23,     /* GPIO[5..9]              */
    40, 40, 40, 40, 40, 40, /* GPIO[10..15]            */
    1,                      /* PVD                     */
    67,                     /* OTG_FS_WKUP, Direct     */
    41,                     /* RTC_ALARM               */
    2,                      /* RTC_TAMP_STAMP2/CSS_LSE */
    3,                      /* RTC wakeup timer        */
    63,                     /* COMP1                   */
    63,                     /* COMP2                   */
    31,                     /* I2C1 wakeup, Direct     */
    33,                     /* I2C2 wakeup, Direct     */
    72,                     /* I2C3 wakeup, Direct     */
    37,                     /* USART1 wakeup, Direct   */
    38,                     /* USART2 wakeup, Direct   */
    39,                     /* USART3 wakeup, Direct   */
    52,                     /* UART4 wakeup, Direct    */
    53,                     /* UART4 wakeup, Direct    */
    70,                     /* LPUART1 wakeup, Direct  */
    65,                     /* LPTIM1, Direct          */
    66,                     /* LPTIM2, Direct          */
    76,                     /* SWPMI1 wakeup, Direct   */
    1,                      /* PVM1 wakeup             */
    1,                      /* PVM2 wakeup             */
    1,                      /* PVM3 wakeup             */
    1,                      /* PVM4 wakeup             */
    78                      /* LCD wakeup, Direct      */
};

static void stm32l4x5_soc_initfn(Object *obj)
{
    Stm32l4x5SocState *s = STM32L4X5_SOC(obj);

    object_initialize_child(obj, "exti", &s->exti, TYPE_STM32L4X5_EXTI);

    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
    s->refclk = qdev_init_clock_in(DEVICE(s), "refclk", NULL, NULL, 0);
}

static void stm32l4x5_soc_realize(DeviceState *dev_soc, Error **errp)
{
    ERRP_GUARD();
    Stm32l4x5SocState *s = STM32L4X5_SOC(dev_soc);
    const Stm32l4x5SocClass *sc = STM32L4X5_SOC_GET_CLASS(dev_soc);
    MemoryRegion *system_memory = get_system_memory();
    DeviceState *dev, *armv7m;
    SysBusDevice *busdev;
    int i;

    /*
     * We use s->refclk internally and only define it with qdev_init_clock_in()
     * so it is correctly parented and not leaked on an init/deinit; it is not
     * intended as an externally exposed clock.
     */
    if (clock_has_source(s->refclk)) {
        error_setg(errp, "refclk clock must not be wired up by the board code");
        return;
    }

    if (!clock_has_source(s->sysclk)) {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    /*
     * TODO: ideally we should model the SoC RCC and its ability to
     * change the sysclk frequency and define different sysclk sources.
     */

    /* The refclk always runs at frequency HCLK / 8 */
    clock_set_mul_div(s->refclk, 8, 1);
    clock_set_source(s->refclk, s->sysclk);

    memory_region_init_rom(&s->flash, OBJECT(dev_soc), "flash",
                           sc->flash_size, errp);
    if (*errp) {
        return;
    }
    memory_region_init_alias(&s->flash_alias, OBJECT(dev_soc),
                             "flash_boot_alias", &s->flash, 0,
                             sc->flash_size);

    memory_region_add_subregion(system_memory, FLASH_BASE_ADDRESS, &s->flash);
    memory_region_add_subregion(system_memory, 0, &s->flash_alias);

    memory_region_init_ram(&s->sram1, OBJECT(dev_soc), "SRAM1", SRAM1_SIZE,
                           errp);
    if (*errp) {
        return;
    }
    memory_region_add_subregion(system_memory, SRAM1_BASE_ADDRESS, &s->sram1);

    memory_region_init_ram(&s->sram2, OBJECT(dev_soc), "SRAM2", SRAM2_SIZE,
                           errp);
    if (*errp) {
        return;
    }
    memory_region_add_subregion(system_memory, SRAM2_BASE_ADDRESS, &s->sram2);

    object_initialize_child(OBJECT(dev_soc), "armv7m", &s->armv7m, TYPE_ARMV7M);
    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 96);
    qdev_prop_set_string(armv7m, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m4"));
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_connect_clock_in(armv7m, "refclk", s->refclk);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }

    dev = DEVICE(&s->exti);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->exti), errp)) {
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, EXTI_ADDR);
    for (i = 0; i < NUM_EXTI_IRQ; i++) {
        /* IRQ seems not to be connected ? */
        sysbus_connect_irq(busdev, i, qdev_get_gpio_in(armv7m, exti_irq[i]));
    }

    /*
     * Uncomment when Syscfg is implemented
     * for (i = 0; i < 16; i++) {
     *     qdev_connect_gpio_out(DEVICE(&s->syscfg), i,
     *                           qdev_get_gpio_in(dev, i));
     * }
     */

    /* APB1 BUS */
    create_unimplemented_device("TIM2",      0x40000000, 0x400);
    create_unimplemented_device("TIM3",      0x40000400, 0x400);
    create_unimplemented_device("TIM4",      0x40000800, 0x400);
    create_unimplemented_device("TIM5",      0x40000C00, 0x400);
    create_unimplemented_device("TIM6",      0x40001000, 0x400);
    create_unimplemented_device("TIM7",      0x40001400, 0x400);
    /* RESERVED:    0x40001800, 0x1000 */
    create_unimplemented_device("RTC",       0x40002800, 0x400);
    create_unimplemented_device("WWDG",      0x40002C00, 0x400);
    create_unimplemented_device("IWDG",      0x40003000, 0x400);
    /* RESERVED:    0x40001800, 0x400 */
    create_unimplemented_device("SPI2",      0x40003800, 0x400);
    create_unimplemented_device("SPI3",      0x40003C00, 0x400);
    /* RESERVED:    0x40004000, 0x400 */
    create_unimplemented_device("USART2",    0x40004400, 0x400);
    create_unimplemented_device("USART3",    0x40004800, 0x400);
    create_unimplemented_device("UART4",     0x40004C00, 0x400);
    create_unimplemented_device("UART5",     0x40005000, 0x400);
    create_unimplemented_device("I2C1",      0x40005400, 0x400);
    create_unimplemented_device("I2C2",      0x40005800, 0x400);
    create_unimplemented_device("I2C3",      0x40005C00, 0x400);
    /* RESERVED:    0x40006000, 0x400 */
    create_unimplemented_device("CAN1",      0x40006400, 0x400);
    /* RESERVED:    0x40006800, 0x400 */
    create_unimplemented_device("PWR",       0x40007000, 0x400);
    create_unimplemented_device("DAC1",      0x40007400, 0x400);
    create_unimplemented_device("OPAMP",     0x40007800, 0x400);
    create_unimplemented_device("LPTIM1",    0x40007C00, 0x400);
    create_unimplemented_device("LPUART1",   0x40008000, 0x400);
    /* RESERVED:    0x40008400, 0x400 */
    create_unimplemented_device("SWPMI1",    0x40008800, 0x400);
    /* RESERVED:    0x40008C00, 0x800 */
    create_unimplemented_device("LPTIM2",    0x40009400, 0x400);
    /* RESERVED:    0x40009800, 0x6800 */

    /* APB2 BUS */
    create_unimplemented_device("SYSCFG",    0x40010000, 0x30);
    create_unimplemented_device("VREFBUF",   0x40010030, 0x1D0);
    create_unimplemented_device("COMP",      0x40010200, 0x200);
    /* RESERVED:    0x40010800, 0x1400 */
    create_unimplemented_device("FIREWALL",  0x40011C00, 0x400);
    /* RESERVED:    0x40012000, 0x800 */
    create_unimplemented_device("SDMMC1",    0x40012800, 0x400);
    create_unimplemented_device("TIM1",      0x40012C00, 0x400);
    create_unimplemented_device("SPI1",      0x40013000, 0x400);
    create_unimplemented_device("TIM8",      0x40013400, 0x400);
    create_unimplemented_device("USART1",    0x40013800, 0x400);
    /* RESERVED:    0x40013C00, 0x400 */
    create_unimplemented_device("TIM15",     0x40014000, 0x400);
    create_unimplemented_device("TIM16",     0x40014400, 0x400);
    create_unimplemented_device("TIM17",     0x40014800, 0x400);
    /* RESERVED:    0x40014C00, 0x800 */
    create_unimplemented_device("SAI1",      0x40015400, 0x400);
    create_unimplemented_device("SAI2",      0x40015800, 0x400);
    /* RESERVED:    0x40015C00, 0x400 */
    create_unimplemented_device("DFSDM1",    0x40016000, 0x400);
    /* RESERVED:    0x40016400, 0x9C00 */

    /* AHB1 BUS */
    create_unimplemented_device("DMA1",      0x40020000, 0x400);
    create_unimplemented_device("DMA2",      0x40020400, 0x400);
    /* RESERVED:    0x40020800, 0x800 */
    create_unimplemented_device("RCC",       0x40021000, 0x400);
    /* RESERVED:    0x40021400, 0xC00 */
    create_unimplemented_device("FLASH",     0x40022000, 0x400);
    /* RESERVED:    0x40022400, 0xC00 */
    create_unimplemented_device("CRC",       0x40023000, 0x400);
    /* RESERVED:    0x40023400, 0x400 */
    create_unimplemented_device("TSC",       0x40024000, 0x400);

    /* RESERVED:    0x40024400, 0x7FDBC00 */

    /* AHB2 BUS */
    create_unimplemented_device("GPIOA",     0x48000000, 0x400);
    create_unimplemented_device("GPIOB",     0x48000400, 0x400);
    create_unimplemented_device("GPIOC",     0x48000800, 0x400);
    create_unimplemented_device("GPIOD",     0x48000C00, 0x400);
    create_unimplemented_device("GPIOE",     0x48001000, 0x400);
    create_unimplemented_device("GPIOF",     0x48001400, 0x400);
    create_unimplemented_device("GPIOG",     0x48001800, 0x400);
    create_unimplemented_device("GPIOH",     0x48001C00, 0x400);
    /* RESERVED:    0x48002000, 0x7FDBC00 */
    create_unimplemented_device("OTG_FS",    0x50000000, 0x40000);
    create_unimplemented_device("ADC",       0x50040000, 0x400);
    /* RESERVED:    0x50040400, 0x20400 */
    create_unimplemented_device("RNG",       0x50060800, 0x400);

    /* AHB3 BUS */
    create_unimplemented_device("FMC",       0xA0000000, 0x1000);
    create_unimplemented_device("QUADSPI",   0xA0001000, 0x400);
}

static void stm32l4x5_soc_class_init(ObjectClass *klass, void *data)
{

    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32l4x5_soc_realize;
    /* Reason: Mapped at fixed location on the system bus */
    dc->user_creatable = false;
    /* No vmstate or reset required: device has no internal state */
}

static void stm32l4x5xc_soc_class_init(ObjectClass *oc, void *data)
{
    Stm32l4x5SocClass *ssc = STM32L4X5_SOC_CLASS(oc);

    ssc->flash_size = 256 * KiB;
}

static void stm32l4x5xe_soc_class_init(ObjectClass *oc, void *data)
{
    Stm32l4x5SocClass *ssc = STM32L4X5_SOC_CLASS(oc);

    ssc->flash_size = 512 * KiB;
}

static void stm32l4x5xg_soc_class_init(ObjectClass *oc, void *data)
{
    Stm32l4x5SocClass *ssc = STM32L4X5_SOC_CLASS(oc);

    ssc->flash_size = 1 * MiB;
}

static const TypeInfo stm32l4x5_soc_types[] = {
    {
        .name           = TYPE_STM32L4X5XC_SOC,
        .parent         = TYPE_STM32L4X5_SOC,
        .class_init     = stm32l4x5xc_soc_class_init,
    }, {
        .name           = TYPE_STM32L4X5XE_SOC,
        .parent         = TYPE_STM32L4X5_SOC,
        .class_init     = stm32l4x5xe_soc_class_init,
    }, {
        .name           = TYPE_STM32L4X5XG_SOC,
        .parent         = TYPE_STM32L4X5_SOC,
        .class_init     = stm32l4x5xg_soc_class_init,
    }, {
        .name           = TYPE_STM32L4X5_SOC,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(Stm32l4x5SocState),
        .instance_init  = stm32l4x5_soc_initfn,
        .class_size     = sizeof(Stm32l4x5SocClass),
        .class_init     = stm32l4x5_soc_class_init,
        .abstract       = true,
    }
};

DEFINE_TYPES(stm32l4x5_soc_types)
