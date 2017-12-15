/*
 * QTest testcase for SDHCI controllers
 *
 * Written by Philippe Mathieu-Daudé <f4bug@amsat.org>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "libqtest.h"

#define SDHC_HCVER                      0xFE

static const struct sdhci_t {
    const char *arch;
    const char *machine;
    struct {
        uintptr_t addr;
        uint8_t version;
    } sdhci;
} models[] = {
    { "arm",    "smdkc210",
        {0x12510000, 2} },
    { "arm",    "sabrelite",
        {0x02190000, 3} },
    { "arm",    "raspi2",           /* bcm2835 */
        {0x3f300000, 3} },
    { "arm",    "xilinx-zynq-a9",   /* exynos4210 */
        {0xe0100000, 3} },
};

static uint32_t sdhci_readl(uintptr_t base, uint32_t reg_addr)
{
    QTestState *qtest = global_qtest;

    return qtest_readl(qtest, base + reg_addr);
}

static void check_specs_version(uintptr_t addr, uint8_t version)
{
    uint32_t v;

    v = sdhci_readl(addr, SDHC_HCVER);
    v &= 0xff;
    v += 1;
    g_assert_cmpuint(v, ==, version);
}

static void test_machine(const void *data)
{
    const struct sdhci_t *test = data;

    global_qtest = qtest_startf("-machine %s -d unimp", test->machine);

    check_specs_version(test->sdhci.addr, test->sdhci.version);

    qtest_quit(global_qtest);
}

int main(int argc, char *argv[])
{
    char *name;
    int i;

    g_test_init(&argc, &argv, NULL);

    for (i = 0; i < ARRAY_SIZE(models); i++) {
        name = g_strdup_printf("sdhci/%s", models[i].machine);
        qtest_add_data_func(name, &models[i], test_machine);
        g_free(name);
    }

    return g_test_run();
}
