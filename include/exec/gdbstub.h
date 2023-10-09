#ifndef GDBSTUB_H
#define GDBSTUB_H

#define DEFAULT_GDBSTUB_PORT "1234"

/* GDB breakpoint/watchpoint types */
#define GDB_BREAKPOINT_SW        0
#define GDB_BREAKPOINT_HW        1
#define GDB_WATCHPOINT_WRITE     2
#define GDB_WATCHPOINT_READ      3
#define GDB_WATCHPOINT_ACCESS    4

typedef struct GDBFeature {
    const char *xmlname;
    const char *xml;
} GDBFeature;


/* Get or set a register.  Returns the size of the register.  */
typedef int (*gdb_get_reg_cb)(CPUArchState *env, GByteArray *buf, int reg);
typedef int (*gdb_set_reg_cb)(CPUArchState *env, uint8_t *buf, int reg);

/**
 * gdb_register_coprocessor() - register a supplemental set of registers
 * @cpu - the CPU associated with registers
 * @get_reg - get function (gdb reading)
 * @set_reg - set function (gdb modifying)
 * @num_regs - number of registers in set
 * @xml - xml name of set
 * @gpos - non-zero to append to "general" register set at @gpos
 */
void gdb_register_coprocessor(CPUState *cpu,
                              gdb_get_reg_cb get_reg, gdb_set_reg_cb set_reg,
                              int num_regs, const char *xml, int g_pos);

/**
 * gdbserver_start: start the gdb server
 * @port_or_device: connection spec for gdb
 *
 * For CONFIG_USER this is either a tcp port or a path to a fifo. For
 * system emulation you can use a full chardev spec for your gdbserver
 * port.
 */
int gdbserver_start(const char *port_or_device);

void gdb_set_stop_cpu(CPUState *cpu);

/* in gdbstub-xml.c, generated by scripts/feature_to_c.py */
extern const GDBFeature gdb_static_features[];

#endif
