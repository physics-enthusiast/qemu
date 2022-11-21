/*
 *  Test program for MSA instruction AVER_S.H
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
    char *group_name = "Int Average";
    char *instruction_name =  "AVER_S.H";
    uint32_t i, j;
    struct timeval start, end;
    double elapsed_time;

    uint64_t b128_result[TEST_COUNT_TOTAL][2];
    uint64_t b128_expect[TEST_COUNT_TOTAL][2] = {
        { 0xffffffffffffffffULL, 0xffffffffffffffffULL, },    /*   0  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xd555d555d555d555ULL, 0xd555d555d555d555ULL, },
        { 0x2aaa2aaa2aaa2aaaULL, 0x2aaa2aaa2aaa2aaaULL, },
        { 0xe666e666e666e666ULL, 0xe666e666e666e666ULL, },
        { 0x1999199919991999ULL, 0x1999199919991999ULL, },
        { 0xf1c71c71c71cf1c7ULL, 0x1c71c71cf1c71c71ULL, },
        { 0x0e38e38e38e30e38ULL, 0xe38e38e30e38e38eULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },    /*   8  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xd555d555d555d555ULL, 0xd555d555d555d555ULL, },
        { 0x2aab2aab2aab2aabULL, 0x2aab2aab2aab2aabULL, },
        { 0xe666e666e666e666ULL, 0xe666e666e666e666ULL, },
        { 0x199a199a199a199aULL, 0x199a199a199a199aULL, },
        { 0xf1c71c72c71cf1c7ULL, 0x1c72c71cf1c71c72ULL, },
        { 0x0e39e38e38e40e39ULL, 0xe38e38e40e39e38eULL, },
        { 0xd555d555d555d555ULL, 0xd555d555d555d555ULL, },    /*  16  */
        { 0xd555d555d555d555ULL, 0xd555d555d555d555ULL, },
        { 0xaaaaaaaaaaaaaaaaULL, 0xaaaaaaaaaaaaaaaaULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xbbbbbbbbbbbbbbbbULL, 0xbbbbbbbbbbbbbbbbULL, },
        { 0xeeefeeefeeefeeefULL, 0xeeefeeefeeefeeefULL, },
        { 0xc71cf1c79c71c71cULL, 0xf1c79c71c71cf1c7ULL, },
        { 0xe38eb8e30e39e38eULL, 0xb8e30e39e38eb8e3ULL, },
        { 0x2aaa2aaa2aaa2aaaULL, 0x2aaa2aaa2aaa2aaaULL, },    /*  24  */
        { 0x2aab2aab2aab2aabULL, 0x2aab2aab2aab2aabULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x5555555555555555ULL, 0x5555555555555555ULL, },
        { 0x1111111111111111ULL, 0x1111111111111111ULL, },
        { 0x4444444444444444ULL, 0x4444444444444444ULL, },
        { 0x1c72471cf1c71c72ULL, 0x471cf1c71c72471cULL, },
        { 0x38e30e39638e38e3ULL, 0x0e39638e38e30e39ULL, },
        { 0xe666e666e666e666ULL, 0xe666e666e666e666ULL, },    /*  32  */
        { 0xe666e666e666e666ULL, 0xe666e666e666e666ULL, },
        { 0xbbbbbbbbbbbbbbbbULL, 0xbbbbbbbbbbbbbbbbULL, },
        { 0x1111111111111111ULL, 0x1111111111111111ULL, },
        { 0xccccccccccccccccULL, 0xccccccccccccccccULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xd82d02d8ad82d82dULL, 0x02d8ad82d82d02d8ULL, },
        { 0xf49fc9f41f4af49fULL, 0xc9f41f4af49fc9f4ULL, },
        { 0x1999199919991999ULL, 0x1999199919991999ULL, },    /*  40  */
        { 0x199a199a199a199aULL, 0x199a199a199a199aULL, },
        { 0xeeefeeefeeefeeefULL, 0xeeefeeefeeefeeefULL, },
        { 0x4444444444444444ULL, 0x4444444444444444ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x3333333333333333ULL, 0x3333333333333333ULL, },
        { 0x0b61360be0b60b61ULL, 0x360be0b60b61360bULL, },
        { 0x27d2fd28527d27d2ULL, 0xfd28527d27d2fd28ULL, },
        { 0xf1c71c71c71cf1c7ULL, 0x1c71c71cf1c71c71ULL, },    /*  48  */
        { 0xf1c71c72c71cf1c7ULL, 0x1c72c71cf1c71c72ULL, },
        { 0xc71cf1c79c71c71cULL, 0xf1c79c71c71cf1c7ULL, },
        { 0x1c72471cf1c71c72ULL, 0x471cf1c71c72471cULL, },
        { 0xd82d02d8ad82d82dULL, 0x02d8ad82d82d02d8ULL, },
        { 0x0b61360be0b60b61ULL, 0x360be0b60b61360bULL, },
        { 0xe38e38e38e38e38eULL, 0x38e38e38e38e38e3ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0e38e38e38e30e38ULL, 0xe38e38e30e38e38eULL, },    /*  56  */
        { 0x0e39e38e38e40e39ULL, 0xe38e38e40e39e38eULL, },
        { 0xe38eb8e30e39e38eULL, 0xb8e30e39e38eb8e3ULL, },
        { 0x38e30e39638e38e3ULL, 0x0e39638e38e30e39ULL, },
        { 0xf49fc9f41f4af49fULL, 0xc9f41f4af49fc9f4ULL, },
        { 0x27d2fd28527d27d2ULL, 0xfd28527d27d2fd28ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x1c71c71c71c71c71ULL, 0xc71c71c71c71c71cULL, },
        { 0x886ae6cc28625540ULL, 0x4b670b5efe7bb00cULL, },    /*  64  */
        { 0xc214f3983afb0e24ULL, 0x2f2fe33c09dd0184ULL, },
        { 0x9a62cabbf119f060ULL, 0x39a0e92fd4d3ea90ULL, },
        { 0xfc5dfe8d434a1bc7ULL, 0xecacca1bd3dfc956ULL, },
        { 0xc214f3983afb0e24ULL, 0x2f2fe33c09dd0184ULL, },
        { 0xfbbe00634d93c708ULL, 0x12f7bb1a153f52fcULL, },
        { 0xd40cd78703b1a944ULL, 0x1d68c10de0353c08ULL, },
        { 0x36070b5855e2d4abULL, 0xd074a1f9df411aceULL, },
        { 0x9a62cabbf119f060ULL, 0x39a0e92fd4d3ea90ULL, },    /*  72  */
        { 0xd40cd78703b1a944ULL, 0x1d68c10de0353c08ULL, },
        { 0xac5aaeaab9cf8b80ULL, 0x27d8c6ffab2b2514ULL, },
        { 0x0e55e27c0c00b6e7ULL, 0xdae5a7ecaa3703daULL, },
        { 0xfc5dfe8d434a1bc7ULL, 0xecacca1bd3dfc956ULL, },
        { 0x36070b5855e2d4abULL, 0xd074a1f9df411aceULL, },
        { 0x0e55e27c0c00b6e7ULL, 0xdae5a7ecaa3703daULL, },
        { 0x704f164d5e31e24eULL, 0x8df188d8a942e2a0ULL, },
};

    reset_msa_registers();

    gettimeofday(&start, NULL);

    for (i = 0; i < PATTERN_INPUTS_SHORT_COUNT; i++) {
        for (j = 0; j < PATTERN_INPUTS_SHORT_COUNT; j++) {
            do_msa_AVER_S_H(b128_pattern[i], b128_pattern[j],
                           b128_result[PATTERN_INPUTS_SHORT_COUNT * i + j]);
        }
    }

    for (i = 0; i < RANDOM_INPUTS_SHORT_COUNT; i++) {
        for (j = 0; j < RANDOM_INPUTS_SHORT_COUNT; j++) {
            do_msa_AVER_S_H(b128_random[i], b128_random[j],
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
