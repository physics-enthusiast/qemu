/*
 *  Test program for MSA instruction ADDS_A.W
 *
 *  Copyright (C) 2019  Wave Computing, Inc.
 *  Copyright (C) 2019  Aleksandar Markovic <amarkovic@wavecomp.com>
 *  Copyright (C) 2019  RT-RK Computer Based Systems LLC
 *  Copyright (C) 2019  Mateja Marjanovic <mateja.marjanovic@rt-rk.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <sys/time.h>
#include <stdint.h>

#include "../../../../include/wrappers_msa.h"
#include "../../../../include/test_inputs_128.h"
#include "../../../../include/test_utils_128.h"

#define TEST_COUNT_TOTAL (                                                \
            (PATTERN_INPUTS_SHORT_COUNT) * (PATTERN_INPUTS_SHORT_COUNT) + \
            (RANDOM_INPUTS_SHORT_COUNT) * (RANDOM_INPUTS_SHORT_COUNT))


int32_t main(void)
{
    char *isa_ase_name = "MSA";
    char *group_name = "Int Add";
    char *instruction_name =  "ADDS_A.W";
    uint32_t i, j;
    struct timeval start, end;
    double elapsed_time;

    uint64_t b128_result[TEST_COUNT_TOTAL][2];
    uint64_t b128_expect[TEST_COUNT_TOTAL][2] = {
        { 0x0000000200000002ULL, 0x0000000200000002ULL, },    /*   0  */
        { 0x0000000100000001ULL, 0x0000000100000001ULL, },
        { 0x5555555755555557ULL, 0x5555555755555557ULL, },
        { 0x5555555655555556ULL, 0x5555555655555556ULL, },
        { 0x3333333533333335ULL, 0x3333333533333335ULL, },
        { 0x3333333433333334ULL, 0x3333333433333334ULL, },
        { 0x1c71c71e71c71c73ULL, 0x38e38e391c71c71eULL, },
        { 0x1c71c71d71c71c72ULL, 0x38e38e3a1c71c71dULL, },
        { 0x0000000100000001ULL, 0x0000000100000001ULL, },    /*   8  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x5555555655555556ULL, 0x5555555655555556ULL, },
        { 0x5555555555555555ULL, 0x5555555555555555ULL, },
        { 0x3333333433333334ULL, 0x3333333433333334ULL, },
        { 0x3333333333333333ULL, 0x3333333333333333ULL, },
        { 0x1c71c71d71c71c72ULL, 0x38e38e381c71c71dULL, },
        { 0x1c71c71c71c71c71ULL, 0x38e38e391c71c71cULL, },
        { 0x5555555755555557ULL, 0x5555555755555557ULL, },    /*  16  */
        { 0x5555555655555556ULL, 0x5555555655555556ULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x71c71c737fffffffULL, 0x7fffffff71c71c73ULL, },
        { 0x71c71c727fffffffULL, 0x7fffffff71c71c72ULL, },
        { 0x5555555655555556ULL, 0x5555555655555556ULL, },    /*  24  */
        { 0x5555555555555555ULL, 0x5555555555555555ULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x71c71c727fffffffULL, 0x7fffffff71c71c72ULL, },
        { 0x71c71c717fffffffULL, 0x7fffffff71c71c71ULL, },
        { 0x3333333533333335ULL, 0x3333333533333335ULL, },    /*  32  */
        { 0x3333333433333334ULL, 0x3333333433333334ULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x6666666866666668ULL, 0x6666666866666668ULL, },
        { 0x6666666766666667ULL, 0x6666666766666667ULL, },
        { 0x4fa4fa517fffffffULL, 0x6c16c16c4fa4fa51ULL, },
        { 0x4fa4fa507fffffffULL, 0x6c16c16d4fa4fa50ULL, },
        { 0x3333333433333334ULL, 0x3333333433333334ULL, },    /*  40  */
        { 0x3333333333333333ULL, 0x3333333333333333ULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x6666666766666667ULL, 0x6666666766666667ULL, },
        { 0x6666666666666666ULL, 0x6666666666666666ULL, },
        { 0x4fa4fa507fffffffULL, 0x6c16c16b4fa4fa50ULL, },
        { 0x4fa4fa4f7fffffffULL, 0x6c16c16c4fa4fa4fULL, },
        { 0x1c71c71e71c71c73ULL, 0x38e38e391c71c71eULL, },    /*  48  */
        { 0x1c71c71d71c71c72ULL, 0x38e38e381c71c71dULL, },
        { 0x71c71c737fffffffULL, 0x7fffffff71c71c73ULL, },
        { 0x71c71c727fffffffULL, 0x7fffffff71c71c72ULL, },
        { 0x4fa4fa517fffffffULL, 0x6c16c16c4fa4fa51ULL, },
        { 0x4fa4fa507fffffffULL, 0x6c16c16b4fa4fa50ULL, },
        { 0x38e38e3a7fffffffULL, 0x71c71c7038e38e3aULL, },
        { 0x38e38e397fffffffULL, 0x71c71c7138e38e39ULL, },
        { 0x1c71c71d71c71c72ULL, 0x38e38e3a1c71c71dULL, },    /*  56  */
        { 0x1c71c71c71c71c71ULL, 0x38e38e391c71c71cULL, },
        { 0x71c71c727fffffffULL, 0x7fffffff71c71c72ULL, },
        { 0x71c71c717fffffffULL, 0x7fffffff71c71c71ULL, },
        { 0x4fa4fa507fffffffULL, 0x6c16c16d4fa4fa50ULL, },
        { 0x4fa4fa4f7fffffffULL, 0x6c16c16c4fa4fa4fULL, },
        { 0x38e38e397fffffffULL, 0x71c71c7138e38e39ULL, },
        { 0x38e38e387fffffffULL, 0x71c71c7238e38e38ULL, },
        { 0x7fffffff50c4aa80ULL, 0x7fffffff03089fe8ULL, },    /*  64  */
        { 0x7bd718d175f61c48ULL, 0x5e5ec67816c3a2f0ULL, },
        { 0x7fffffff6e92c9c0ULL, 0x733fd25d56592ae0ULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff58416d54ULL, },
        { 0x7bd718d175f61c48ULL, 0x5e5ec67816c3a2f0ULL, },
        { 0x0883ff3a7fffffffULL, 0x25ef76342a7ea5f8ULL, },
        { 0x57e750f37fffffffULL, 0x3ad082196a142de8ULL, },
        { 0x749115ea7fffffffULL, 0x7fffffff6bfc705cULL, },
        { 0x7fffffff6e92c9c0ULL, 0x733fd25d56592ae0ULL, },    /*  72  */
        { 0x57e750f37fffffffULL, 0x3ad082196a142de8ULL, },
        { 0x7fffffff7fffffffULL, 0x4fb18dfe7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff58416d54ULL, },
        { 0x749115ea7fffffffULL, 0x7fffffff6bfc705cULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
        { 0x7fffffff7fffffffULL, 0x7fffffff7fffffffULL, },
};

    reset_msa_registers();

    gettimeofday(&start, NULL);

    for (i = 0; i < PATTERN_INPUTS_SHORT_COUNT; i++) {
        for (j = 0; j < PATTERN_INPUTS_SHORT_COUNT; j++) {
            do_msa_ADDS_A_W(b128_pattern[i], b128_pattern[j],
                           b128_result[PATTERN_INPUTS_SHORT_COUNT * i + j]);
        }
    }

    for (i = 0; i < RANDOM_INPUTS_SHORT_COUNT; i++) {
        for (j = 0; j < RANDOM_INPUTS_SHORT_COUNT; j++) {
            do_msa_ADDS_A_W(b128_random[i], b128_random[j],
                           b128_result[((PATTERN_INPUTS_SHORT_COUNT) *
                                        (PATTERN_INPUTS_SHORT_COUNT)) +
                                       RANDOM_INPUTS_SHORT_COUNT * i + j]);
        }
    }

    gettimeofday(&end, NULL);

    elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0;
    elapsed_time += (end.tv_usec - start.tv_usec) / 1000.0;

    return check_results_128(isa_ase_name, group_name, instruction_name,
                             TEST_COUNT_TOTAL, elapsed_time,
                             &b128_result[0][0], &b128_expect[0][0]);
}
