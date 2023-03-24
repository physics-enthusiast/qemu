/*
 * IPMI base class
 *
 * Copyright (c) 2015 Corey Minyard, MontaVista Software, LLC
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

#ifndef HW_IPMI_H
#define HW_IPMI_H

#include "exec/memory.h"
#include "hw/qdev-core.h"
#include "qom/object.h"

#define MAX_IPMI_MSG_SIZE 300

enum ipmi_op {
    IPMI_RESET_CHASSIS,
    IPMI_POWEROFF_CHASSIS,
    IPMI_POWERON_CHASSIS,
    IPMI_POWERCYCLE_CHASSIS,
    IPMI_PULSE_DIAG_IRQ,
    IPMI_SHUTDOWN_VIA_ACPI_OVERTEMP,
    IPMI_SEND_NMI
};

#define IPMI_CC_INVALID_CMD                              0xc1
#define IPMI_CC_COMMAND_INVALID_FOR_LUN                  0xc2
#define IPMI_CC_TIMEOUT                                  0xc3
#define IPMI_CC_OUT_OF_SPACE                             0xc4
#define IPMI_CC_INVALID_RESERVATION                      0xc5
#define IPMI_CC_REQUEST_DATA_TRUNCATED                   0xc6
#define IPMI_CC_REQUEST_DATA_LENGTH_INVALID              0xc7
#define IPMI_CC_PARM_OUT_OF_RANGE                        0xc9
#define IPMI_CC_CANNOT_RETURN_REQ_NUM_BYTES              0xca
#define IPMI_CC_REQ_ENTRY_NOT_PRESENT                    0xcb
#define IPMI_CC_INVALID_DATA_FIELD                       0xcc
#define IPMI_CC_BMC_INIT_IN_PROGRESS                     0xd2
#define IPMI_CC_COMMAND_NOT_SUPPORTED                    0xd5
#define IPMI_CC_UNSPECIFIED                              0xff

#define IPMI_NETFN_APP                0x06
#define IPMI_NETFN_OEM                0x3a

#define IPMI_DEBUG 1

/* Specified in the SMBIOS spec. */
#define IPMI_SMBIOS_KCS         0x01
#define IPMI_SMBIOS_SMIC        0x02
#define IPMI_SMBIOS_BT          0x03
#define IPMI_SMBIOS_SSIF        0x04

/*
 * Used for transferring information to interfaces that add
 * entries to firmware tables.
 */
typedef struct IPMIFwInfo {
    const char *interface_name;
    int interface_type;
    uint8_t ipmi_spec_major_revision;
    uint8_t ipmi_spec_minor_revision;
    uint8_t i2c_slave_address;
    uint32_t uuid;

    uint64_t base_address;
    uint64_t register_length;
    uint8_t register_spacing;
    enum {
        IPMI_MEMSPACE_IO,
        IPMI_MEMSPACE_MEM32,
        IPMI_MEMSPACE_MEM64,
        IPMI_MEMSPACE_SMBUS
    } memspace;

    int interrupt_number;
    enum {
        IPMI_LEVEL_IRQ,
        IPMI_EDGE_IRQ
    } irq_type;
} IPMIFwInfo;

/*
 * Called by each instantiated IPMI interface device to get it's uuid.
 */
uint32_t ipmi_next_uuid(void);

/* IPMI Interface types (KCS, SMIC, BT) are prefixed with this */
#define TYPE_IPMI_INTERFACE_PREFIX "ipmi-interface-"

/*
 * An IPMI Interface, the interface for talking between the target
 * and the BMC.
 */
#define TYPE_IPMI_INTERFACE "ipmi-interface"
OBJECT_DECLARE_TYPE(IPMIInterface, IPMIInterfaceClass, IPMI_INTERFACE)

typedef struct IPMIInterfaceClass IPMIInterfaceClass;
typedef struct IPMIInterface IPMIInterface;

struct IPMIInterfaceClass {
    InterfaceClass parent;

    /*
     * The interfaces use this to perform certain ops
     */
    void (*set_atn)(struct IPMIInterface *s, int val, int irq);

    /*
     * Set by the owner to hold the backend data for the interface.
     */
    void *(*get_backend_data)(struct IPMIInterface *s);

    /*
     * Handle a message between the host and the BMC.
     */
    void (*handle_msg)(struct IPMIInterface *s, uint8_t msg_id,
                       unsigned char *msg, unsigned int msg_len);
};

/*
 * An IPMI Interface representing host side communication to a
 * remote BMC, either simulated or an IPMI BMC client.
 */
#define TYPE_IPMI_INTERFACE_HOST "ipmi-interface-host"
OBJECT_DECLARE_TYPE(IPMIInterfaceHost, IPMIInterfaceHostClass, \
                    IPMI_INTERFACE_HOST)

typedef struct IPMIInterfaceHostClass IPMIInterfaceHostClass;
typedef struct IPMIInterfaceHost IPMIInterfaceHost;

struct IPMIInterfaceHostClass {
    IPMIInterfaceClass parent;

    /*
     * min_size is the requested I/O size and must be a power of 2.
     * This is so PCI (or other busses) can request a bigger range.
     * Use 0 for the default.
     */
    void (*init)(struct IPMIInterfaceHost *s, unsigned int min_size,
                 Error **errp);

    /*
     * Perform various operations on the hardware.  If checkonly is
     * true, it will return if the operation can be performed, but it
     * will not do the operation.
     */
    int (*do_hw_op)(struct IPMIInterfaceHost *s, enum ipmi_op op,
                    int checkonly);

    /*
     * Enable/disable irqs on the interface when the BMC requests this.
     */
    void (*set_irq_enable)(struct IPMIInterfaceHost *s, int val);

    /*
     * Handle an event that occurred on the interface, generally the.
     * target writing to a register.
     */
    void (*handle_if_event)(struct IPMIInterfaceHost *s);

    /*
     * Got an IPMI warm/cold reset.
     */
    void (*reset)(struct IPMIInterfaceHost *s, bool is_cold);

    /*
     * Return the firmware info for a device.
     */
    void (*get_fwinfo)(struct IPMIInterfaceHost *s, IPMIFwInfo *info);
};

/*
 * An IPMI Interface representing BMC side communication to a
 * remote host running `ipmi-bmc-extern`.
 */
#define TYPE_IPMI_INTERFACE_CLIENT "ipmi-interface-client"
OBJECT_DECLARE_TYPE(IPMIInterfaceClient, IPMIInterfaceClientClass,
                    IPMI_INTERFACE_CLIENT)

typedef struct IPMIInterfaceClientClass IPMIInterfaceClientClass;
typedef struct IPMIInterfaceClient IPMIInterfaceClient;

struct IPMIInterfaceClientClass {
    IPMIInterfaceClass parent;
};

/*
 * Define an IPMI core (Either BMC or Host simulator.)
 */
#define TYPE_IPMI_CORE "ipmi-core"
OBJECT_DECLARE_TYPE(IPMICore, IPMICoreClass, IPMI_CORE)

struct IPMICore {
    DeviceState parent;

    IPMIInterface *intf;
};

struct IPMICoreClass {
    DeviceClass parent;

    /*
     * Handle a hardware command.
     */
    void (*handle_hw_op)(struct IPMICore *s, uint8_t hw_op, uint8_t operand);

    /*
     * Handle a command to the bmc.
     */
    void (*handle_command)(struct IPMICore *s,
                           uint8_t *cmd, unsigned int cmd_len,
                           unsigned int max_cmd_len,
                           uint8_t msg_id);
};

/*
 * Define a BMC simulator (or perhaps a connection to a real BMC)
 */
#define TYPE_IPMI_BMC_HOST "ipmi-bmc-host"
OBJECT_DECLARE_TYPE(IPMIBmcHost, IPMIBmcHostClass, IPMI_BMC_HOST)

struct IPMIBmcHost {
    IPMICore parent;

    uint8_t slave_addr;
};

struct IPMIBmcHostClass {
    IPMICoreClass parent;

    /* Called when the system resets to report to the bmc. */
    void (*handle_reset)(struct IPMIBmcHost *s);

};

/*
 * Define a BMC side client that responds to an `ipmi-bmc-extern`.
 */
#define TYPE_IPMI_BMC_CLIENT "ipmi-bmc-client"
OBJECT_DECLARE_SIMPLE_TYPE(IPMIBmcClient, IPMI_BMC_CLIENT)
struct IPMIBmcClient {
    IPMICore parent;
};

/*
 * Add a link property to obj that points to a BMC.
 */
void ipmi_bmc_find_and_link(Object *obj, Object **bmc);

#ifdef IPMI_DEBUG
#define ipmi_debug(fs, ...) \
    fprintf(stderr, "IPMI (%s): " fs, __func__, ##__VA_ARGS__)
#else
#define ipmi_debug(fs, ...)
#endif

struct ipmi_sdr_header {
    uint8_t  rec_id[2];
    uint8_t  sdr_version;               /* 0x51 */
    uint8_t  rec_type;
    uint8_t  rec_length;
};
#define IPMI_SDR_HEADER_SIZE     sizeof(struct ipmi_sdr_header)

#define ipmi_sdr_recid(sdr) ((sdr)->rec_id[0] | ((sdr)->rec_id[1] << 8))
#define ipmi_sdr_length(sdr) ((sdr)->rec_length + IPMI_SDR_HEADER_SIZE)

/*
 * 43.2 SDR Type 02h. Compact Sensor Record
 */
#define IPMI_SDR_COMPACT_TYPE    2

struct ipmi_sdr_compact {
    struct ipmi_sdr_header header;

    uint8_t  sensor_owner_id;
    uint8_t  sensor_owner_lun;
    uint8_t  sensor_owner_number;       /* byte 8 */
    uint8_t  entity_id;
    uint8_t  entity_instance;
    uint8_t  sensor_init;
    uint8_t  sensor_caps;
    uint8_t  sensor_type;
    uint8_t  reading_type;
    uint8_t  assert_mask[2];            /* byte 16 */
    uint8_t  deassert_mask[2];
    uint8_t  discrete_mask[2];
    uint8_t  sensor_unit1;
    uint8_t  sensor_unit2;
    uint8_t  sensor_unit3;
    uint8_t  sensor_direction[2];       /* byte 24 */
    uint8_t  positive_threshold;
    uint8_t  negative_threshold;
    uint8_t  reserved[3];
    uint8_t  oem;
    uint8_t  id_str_len;                /* byte 32 */
    uint8_t  id_string[16];
};

typedef uint8_t ipmi_sdr_compact_buffer[sizeof(struct ipmi_sdr_compact)];

int ipmi_bmc_sdr_find(IPMIBmcHost *b, uint16_t recid,
                      const struct ipmi_sdr_compact **sdr, uint16_t *nextrec);
void ipmi_bmc_gen_event(IPMIBmcHost *b, uint8_t *evt, bool log);

#define TYPE_IPMI_BMC_SIMULATOR "ipmi-bmc-sim"
OBJECT_DECLARE_SIMPLE_TYPE(IPMIBmcSim, IPMI_BMC_SIMULATOR)


typedef struct RspBuffer {
    uint8_t buffer[MAX_IPMI_MSG_SIZE];
    unsigned int len;
} RspBuffer;

static inline void rsp_buffer_set_error(RspBuffer *rsp, uint8_t byte)
{
    rsp->buffer[2] = byte;
}

/* Add a byte to the response. */
static inline void rsp_buffer_push(RspBuffer *rsp, uint8_t byte)
{
    if (rsp->len >= sizeof(rsp->buffer)) {
        rsp_buffer_set_error(rsp, IPMI_CC_REQUEST_DATA_TRUNCATED);
        return;
    }
    rsp->buffer[rsp->len++] = byte;
}

typedef struct IPMICmdHandler {
    void (*cmd_handler)(IPMIBmcSim *s,
                        uint8_t *cmd, unsigned int cmd_len,
                        RspBuffer *rsp);
    unsigned int cmd_len_min;
} IPMICmdHandler;

typedef struct IPMINetfn {
    unsigned int cmd_nums;
    const IPMICmdHandler *cmd_handlers;
} IPMINetfn;

int ipmi_sim_register_netfn(IPMIBmcSim *s, unsigned int netfn,
                            const IPMINetfn *netfnd);

#endif
