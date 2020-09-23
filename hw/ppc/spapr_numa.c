/*
 * QEMU PowerPC pSeries Logical Partition NUMA associativity handling
 *
 * Copyright IBM Corp. 2020
 *
 * Authors:
 *  Daniel Henrique Barboza      <danielhb413@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/ppc/spapr_numa.h"
#include "hw/pci-host/spapr.h"
#include "hw/ppc/fdt.h"

/* Moved from hw/ppc/spapr_pci_nvlink2.c */
#define SPAPR_GPU_NUMA_ID           (cpu_to_be32(1))

static bool spapr_numa_is_symmetrical(MachineState *ms)
{
    int src, dst;
    int nb_numa_nodes = ms->numa_state->num_nodes;
    NodeInfo *numa_info = ms->numa_state->nodes;

    for (src = 0; src < nb_numa_nodes; src++) {
        for (dst = src; dst < nb_numa_nodes; dst++) {
            if (numa_info[src].distance[dst] !=
                numa_info[dst].distance[src]) {
                return false;
            }
        }
    }

    return true;
}

/*
 * This function will translate the user distances into
 * what the kernel understand as possible values: 10
 * (local distance), 20, 40, 80 and 160. Current heuristic
 * is:
 *
 *  - distances between 11 and 30 -> rounded to 20
 *  - distances between 31 and 60 -> rounded to 40
 *  - distances between 61 and 120 -> rounded to 80
 *  - everything above 120 -> 160
 *
 * This step can also be done in the same time as the NUMA
 * associativity domains calculation, at the cost of extra
 * complexity. We chose to keep it simpler.
 *
 * Note: this will overwrite the distance values in
 * ms->numa_state->nodes.
 */
static void spapr_numa_PAPRify_distances(MachineState *ms)
{
    int src, dst;
    int nb_numa_nodes = ms->numa_state->num_nodes;
    NodeInfo *numa_info = ms->numa_state->nodes;

    for (src = 0; src < nb_numa_nodes; src++) {
        for (dst = src; dst < nb_numa_nodes; dst++) {
            uint8_t distance = numa_info[src].distance[dst];
            uint8_t rounded_distance = 160;

            if (distance > 11 && distance < 30) {
                rounded_distance = 20;
            } else if (distance > 31 && distance < 60) {
                rounded_distance = 40;
            } else if (distance > 61 && distance < 120) {
                rounded_distance = 80;
            }

            numa_info[src].distance[dst] = rounded_distance;
            numa_info[dst].distance[src] = rounded_distance;
        }
    }
}

static uint8_t spapr_numa_get_NUMA_level(uint8_t distance)
{
    uint8_t numa_level;

    switch (distance) {
    case 20:
        numa_level = 0x3;
        break;
    case 40:
        numa_level = 0x2;
        break;
    case 80:
        numa_level = 0x1;
        break;
    default:
        numa_level = 0;
    }

    return numa_level;
}

static void spapr_numa_define_associativity_domains(SpaprMachineState *spapr,
                                                    MachineState *ms)
{
    int src, dst;
    int nb_numa_nodes = ms->numa_state->num_nodes;
    NodeInfo *numa_info = ms->numa_state->nodes;

    for (src = 0; src < nb_numa_nodes; src++) {
        for (dst = src; dst < nb_numa_nodes; dst++) {
            /*
             * This is how the associativity domain between A and B
             * is calculated:
             *
             * - get the distance between them
             * - get the correspondent NUMA level for this distance
             * - the arrays were initialized with their own numa_ids,
             * and we're calculating the distance in node_id ascending order,
             * starting from node 0. This will have a cascade effect in the
             * algorithm because the associativity domains that node 0 defines
             * will be carried over to the other nodes, and node 1
             * associativities will be carried over unless there's already a
             * node 0 associativity assigned, and so on. This happens because
             * we'll assign the lowest value of assoc_src and assoc_dst to be
             * the associativity domain of both, for the given NUMA level.
             *
             * The PPC kernel expects the associativity domains of node 0 to
             * be always 0, and this algorithm will grant that by default.
             */
            uint8_t distance = numa_info[src].distance[dst];
            uint8_t n_level = spapr_numa_get_NUMA_level(distance);
            uint32_t assoc_src, assoc_dst;

            assoc_src = be32_to_cpu(spapr->numa_assoc_array[src][n_level]);
            assoc_dst = be32_to_cpu(spapr->numa_assoc_array[dst][n_level]);

            if (assoc_src < assoc_dst) {
                spapr->numa_assoc_array[dst][n_level] = cpu_to_be32(assoc_src);
            } else {
                spapr->numa_assoc_array[src][n_level] = cpu_to_be32(assoc_dst);
            }
        }
    }

}

void spapr_numa_associativity_init(SpaprMachineState *spapr,
                                   MachineState *machine)
{
    SpaprMachineClass *smc = SPAPR_MACHINE_GET_CLASS(spapr);
    int nb_numa_nodes = machine->numa_state->num_nodes;
    int i, j, max_nodes_with_gpus;
    bool using_legacy_numa = spapr_machine_using_legacy_numa(spapr);

    /*
     * For all associativity arrays: first position is the size,
     * position MAX_DISTANCE_REF_POINTS is always the numa_id,
     * represented by the index 'i'.
     *
     * This will break on sparse NUMA setups, when/if QEMU starts
     * to support it, because there will be no more guarantee that
     * 'i' will be a valid node_id set by the user.
     */
    for (i = 0; i < nb_numa_nodes; i++) {
        spapr->numa_assoc_array[i][0] = cpu_to_be32(MAX_DISTANCE_REF_POINTS);
        spapr->numa_assoc_array[i][MAX_DISTANCE_REF_POINTS] = cpu_to_be32(i);

        /*
         * Fill all associativity domains of the node with node_id.
         * This is required because the kernel makes valid associativity
         * matches with the zeroes if we leave the matrix unitialized.
         */
        if (!using_legacy_numa) {
            for (j = 1; j < MAX_DISTANCE_REF_POINTS; j++) {
                spapr->numa_assoc_array[i][j] = cpu_to_be32(i);
            }
        }
    }

    /*
     * Initialize NVLink GPU associativity arrays. We know that
     * the first GPU will take the first available NUMA id, and
     * we'll have a maximum of NVGPU_MAX_NUM GPUs in the machine.
     * At this point we're not sure if there are GPUs or not, but
     * let's initialize the associativity arrays and allow NVLink
     * GPUs to be handled like regular NUMA nodes later on.
     */
    max_nodes_with_gpus = nb_numa_nodes + NVGPU_MAX_NUM;

    for (i = nb_numa_nodes; i < max_nodes_with_gpus; i++) {
        spapr->numa_assoc_array[i][0] = cpu_to_be32(MAX_DISTANCE_REF_POINTS);

        for (j = 1; j < MAX_DISTANCE_REF_POINTS; j++) {
            uint32_t gpu_assoc = smc->pre_5_1_assoc_refpoints ?
                                 SPAPR_GPU_NUMA_ID : cpu_to_be32(i);
            spapr->numa_assoc_array[i][j] = gpu_assoc;
        }

        spapr->numa_assoc_array[i][MAX_DISTANCE_REF_POINTS] = cpu_to_be32(i);
    }

    /*
     * Legacy NUMA guests (pseries-5.1 and order, or guests with only
     * 1 NUMA node) will not benefit from anything we're going to do
     * after this point.
     */
    if (using_legacy_numa) {
        return;
    }

    if (!spapr_numa_is_symmetrical(machine)) {
        error_report("Asymmetrical NUMA topologies aren't supported "
                     "in the pSeries machine");
        exit(1);
    }

    spapr_numa_PAPRify_distances(machine);
    spapr_numa_define_associativity_domains(spapr, machine);
}

void spapr_numa_write_associativity_dt(SpaprMachineState *spapr, void *fdt,
                                       int offset, int nodeid)
{
    _FDT((fdt_setprop(fdt, offset, "ibm,associativity",
                      spapr->numa_assoc_array[nodeid],
                      sizeof(spapr->numa_assoc_array[nodeid]))));
}

static uint32_t *spapr_numa_get_vcpu_assoc(SpaprMachineState *spapr,
                                           PowerPCCPU *cpu)
{
    uint32_t *vcpu_assoc = g_new(uint32_t, VCPU_ASSOC_SIZE);
    int index = spapr_get_vcpu_id(cpu);

    /*
     * VCPUs have an extra 'cpu_id' value in ibm,associativity
     * compared to other resources. Increment the size at index
     * 0, put cpu_id last, then copy the remaining associativity
     * domains.
     */
    vcpu_assoc[0] = cpu_to_be32(MAX_DISTANCE_REF_POINTS + 1);
    vcpu_assoc[VCPU_ASSOC_SIZE - 1] = cpu_to_be32(index);
    memcpy(vcpu_assoc + 1, spapr->numa_assoc_array[cpu->node_id] + 1,
           (VCPU_ASSOC_SIZE - 2) * sizeof(uint32_t));

    return vcpu_assoc;
}

int spapr_numa_fixup_cpu_dt(SpaprMachineState *spapr, void *fdt,
                            int offset, PowerPCCPU *cpu)
{
    g_autofree uint32_t *vcpu_assoc = NULL;

    vcpu_assoc = spapr_numa_get_vcpu_assoc(spapr, cpu);

    /* Advertise NUMA via ibm,associativity */
    return fdt_setprop(fdt, offset, "ibm,associativity", vcpu_assoc,
                       VCPU_ASSOC_SIZE * sizeof(uint32_t));
}


int spapr_numa_write_assoc_lookup_arrays(SpaprMachineState *spapr, void *fdt,
                                         int offset)
{
    MachineState *machine = MACHINE(spapr);
    int nb_numa_nodes = machine->numa_state->num_nodes;
    int nr_nodes = nb_numa_nodes ? nb_numa_nodes : 1;
    uint32_t *int_buf, *cur_index, buf_len;
    int ret, i;

    /* ibm,associativity-lookup-arrays */
    buf_len = (nr_nodes * MAX_DISTANCE_REF_POINTS + 2) * sizeof(uint32_t);
    cur_index = int_buf = g_malloc0(buf_len);
    int_buf[0] = cpu_to_be32(nr_nodes);
     /* Number of entries per associativity list */
    int_buf[1] = cpu_to_be32(MAX_DISTANCE_REF_POINTS);
    cur_index += 2;
    for (i = 0; i < nr_nodes; i++) {
        /*
         * For the lookup-array we use the ibm,associativity array,
         * from numa_assoc_array. without the first element (size).
         */
        uint32_t *associativity = spapr->numa_assoc_array[i];
        memcpy(cur_index, ++associativity,
               sizeof(uint32_t) * MAX_DISTANCE_REF_POINTS);
        cur_index += MAX_DISTANCE_REF_POINTS;
    }
    ret = fdt_setprop(fdt, offset, "ibm,associativity-lookup-arrays", int_buf,
                      (cur_index - int_buf) * sizeof(uint32_t));
    g_free(int_buf);

    return ret;
}

/*
 * Helper that writes ibm,associativity-reference-points and
 * max-associativity-domains in the RTAS pointed by @rtas
 * in the DT @fdt.
 */
void spapr_numa_write_rtas_dt(SpaprMachineState *spapr, void *fdt, int rtas)
{
    MachineState *ms = MACHINE(spapr);
    SpaprMachineClass *smc = SPAPR_MACHINE_GET_CLASS(spapr);
    uint32_t refpoints[] = {
        cpu_to_be32(0x4),
        cpu_to_be32(0x3),
        cpu_to_be32(0x2),
        cpu_to_be32(0x1),
    };
    uint32_t nr_refpoints = ARRAY_SIZE(refpoints);
    uint32_t maxdomain = cpu_to_be32(ms->numa_state->num_nodes +
                                     spapr->gpu_numa_id);
    uint32_t maxdomains[] = {0x4, maxdomain, maxdomain, maxdomain, maxdomain};

    if (spapr_machine_using_legacy_numa(spapr)) {
        refpoints[1] =  cpu_to_be32(0x4);
        refpoints[2] =  cpu_to_be32(0x2);
        nr_refpoints = 3;

        maxdomain = cpu_to_be32(spapr->gpu_numa_id > 1 ? 1 : 0);
        maxdomains[1] = maxdomain;
        maxdomains[2] = maxdomain;
        maxdomains[3] = maxdomain;
        maxdomains[4] = cpu_to_be32(spapr->gpu_numa_id);
    }

    if (smc->pre_5_1_assoc_refpoints) {
        nr_refpoints = 2;
    }

    _FDT(fdt_setprop(fdt, rtas, "ibm,associativity-reference-points",
                     refpoints, nr_refpoints * sizeof(refpoints[0])));

    _FDT(fdt_setprop(fdt, rtas, "ibm,max-associativity-domains",
                     maxdomains, sizeof(maxdomains)));
}

static target_ulong h_home_node_associativity(PowerPCCPU *cpu,
                                              SpaprMachineState *spapr,
                                              target_ulong opcode,
                                              target_ulong *args)
{
    g_autofree uint32_t *vcpu_assoc = NULL;
    target_ulong flags = args[0];
    target_ulong procno = args[1];
    PowerPCCPU *tcpu;
    int idx, assoc_idx;

    /* only support procno from H_REGISTER_VPA */
    if (flags != 0x1) {
        return H_FUNCTION;
    }

    tcpu = spapr_find_cpu(procno);
    if (tcpu == NULL) {
        return H_P2;
    }

    /*
     * Given that we want to be flexible with the sizes and indexes,
     * we must consider that there is a hard limit of how many
     * associativities domain we can fit in R4 up to R9, which would be
     * 12 associativity domains for vcpus. Assert and bail if that's
     * not the case.
     */
    G_STATIC_ASSERT((VCPU_ASSOC_SIZE - 1) <= 12);

    vcpu_assoc = spapr_numa_get_vcpu_assoc(spapr, tcpu);
    /* assoc_idx starts at 1 to skip associativity size */
    assoc_idx = 1;

#define ASSOCIATIVITY(a, b) (((uint64_t)(a) << 32) | \
                             ((uint64_t)(b) & 0xffffffff))

    for (idx = 0; idx < 6; idx++) {
        int32_t a, b;

        /*
         * vcpu_assoc[] will contain the associativity domains for tcpu,
         * including tcpu->node_id and procno, meaning that we don't
         * need to use these variables here.
         *
         * We'll read 2 values at a time to fill up the ASSOCIATIVITY()
         * macro. The ternary will fill the remaining registers with -1
         * after we went through vcpu_assoc[].
         */
        a = assoc_idx < VCPU_ASSOC_SIZE ?
            be32_to_cpu(vcpu_assoc[assoc_idx++]) : -1;
        b = assoc_idx < VCPU_ASSOC_SIZE ?
            be32_to_cpu(vcpu_assoc[assoc_idx++]) : -1;

        args[idx] = ASSOCIATIVITY(a, b);
    }
#undef ASSOCIATIVITY

    return H_SUCCESS;
}

static void spapr_numa_register_types(void)
{
    /* Virtual Processor Home Node */
    spapr_register_hypercall(H_HOME_NODE_ASSOCIATIVITY,
                             h_home_node_associativity);
}

type_init(spapr_numa_register_types)
