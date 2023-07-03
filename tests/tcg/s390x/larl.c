/*
 * Test the LARL instruction.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdlib.h>

int main(void)
{
    long algfi = (long)main;
    long larl;

    asm("algfi %[r],0xd0000000" : [r] "+r" (algfi) : : "cc");
    asm("larl %[r],main+0xd0000000" : [r] "=r" (larl));

    return algfi == larl ? EXIT_SUCCESS : EXIT_FAILURE;
}
