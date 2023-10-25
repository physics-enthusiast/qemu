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
    int num_regs;
} GDBFeature;

typedef struct GDBFeatureBuilder {
    GDBFeature *feature;
    GPtrArray *xml;
    int base_reg;
} GDBFeatureBuilder;


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

/**
 * gdb_feature_builder_init() - Initialize GDBFeatureBuilder.
 * @builder: The builder to be initialized.
 * @feature: The feature to be filled.
 * @name: The name of the feature.
 * @xmlname: The name of the XML.
 * @base_reg: The base number of the register ID.
 */
void gdb_feature_builder_init(GDBFeatureBuilder *builder, GDBFeature *feature,
                              const char *name, const char *xmlname,
                              int base_reg);

/**
 * gdb_feature_builder_append_tag() - Append a tag.
 * @builder: The builder.
 * @format: The format of the tag.
 * @...: The values to be formatted.
 */
void G_GNUC_PRINTF(2, 3)
gdb_feature_builder_append_tag(const GDBFeatureBuilder *builder,
                               const char *format, ...);

/**
 * gdb_feature_builder_append_reg() - Append a register.
 * @builder: The builder.
 * @name: The register's name; it must be unique within a CPU.
 * @bitsize: The register's size, in bits.
 * @regnum: The offset of the register's number in the feature.
 * @type: The type of the register.
 * @group: The register group to which this register belongs; it can be NULL.
 */
void gdb_feature_builder_append_reg(const GDBFeatureBuilder *builder,
                                    const char *name,
                                    int bitsize,
                                    int regnum,
                                    const char *type,
                                    const char *group);

/**
 * gdb_feature_builder_end() - End building GDBFeature.
 * @builder: The builder.
 */
void gdb_feature_builder_end(const GDBFeatureBuilder *builder);

/**
 * gdb_find_static_feature() - Find a static feature.
 * @xmlname: The name of the XML.
 *
 * Return: The static feature.
 */
const GDBFeature *gdb_find_static_feature(const char *xmlname);

void gdb_set_stop_cpu(CPUState *cpu);

/* in gdbstub-xml.c, generated by scripts/feature_to_c.py */
extern const GDBFeature gdb_static_features[];

#endif
