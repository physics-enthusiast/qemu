/*
 *this file is shared between the mcd dll and the mcd stub.
 *it has to be kept exectly the same!
 */

#ifndef MCD_SHARED_DEFINES
#define MCD_SHARED_DEFINES

/* tcp data characters */
#define TCP_CHAR_OPEN_SERVER 'I'
#define TCP_CHAR_OPEN_CORE 'i'
#define TCP_CHAR_GO 'C'
#define TCP_CHAR_STEP 'c'
#define TCP_CHAR_BREAK 'b'
#define TCP_CHAR_QUERY 'q'
#define TCP_CHAR_CLOSE_SERVER 'D'
#define TCP_CHAR_CLOSE_CORE 'd'
#define TCP_CHAR_KILLQEMU 'k'
#define TCP_CHAR_RESET 'r'
#define TCP_CHAR_READ_REGISTER 'p'
#define TCP_CHAR_WRITE_REGISTER 'P'
#define TCP_CHAR_READ_MEMORY 'm'
#define TCP_CHAR_WRITE_MEMORY 'M'
#define TCP_CHAR_BREAKPOINT_INSERT 't'
#define TCP_CHAR_BREAKPOINT_REMOVE 'T'

/* tcp protocol chars */
#define TCP_ACKNOWLEDGED '+'
#define TCP_NOT_ACKNOWLEDGED '-'
#define TCP_COMMAND_START '$'
#define TCP_COMMAND_END '#'
#define TCP_WAS_LAST '|'
#define TCP_WAS_NOT_LAST '~'
#define TCP_HANDSHAKE_SUCCESS "shaking your hand"
#define TCP_EXECUTION_SUCCESS "success"
#define TCP_EXECUTION_ERROR "error"

/* tcp query arguments */
#define QUERY_FIRST "f"
#define QUERY_CONSEQUTIVE "c"

#define QUERY_ARG_SYSTEM "system"
#define QUERY_ARG_CORES "cores"
#define QUERY_ARG_RESET "reset"
#define QUERY_ARG_TRIGGER "trigger"
#define QUERY_ARG_MEMORY "memory"
#define QUERY_ARG_REGGROUP "reggroup"
#define QUERY_ARG_REG "reg"
#define QUERY_ARG_STATE "state"

/* tcp query packet argument list */
#define TCP_ARGUMENT_NAME "name"
#define TCP_ARGUMENT_DATA "data"
#define TCP_ARGUMENT_ID "id"
#define TCP_ARGUMENT_TYPE "type"
#define TCP_ARGUMENT_BITS_PER_MAU "bpm"
#define TCP_ARGUMENT_INVARIANCE "i"
#define TCP_ARGUMENT_ENDIAN "e"
#define TCP_ARGUMENT_MIN "min"
#define TCP_ARGUMENT_MAX "max"
#define TCP_ARGUMENT_SUPPORTED_ACCESS_OPTIONS "sao"
#define TCP_ARGUMENT_REGGROUPID "reggroupid"
#define TCP_ARGUMENT_MEMSPACEID "memspaceid"
#define TCP_ARGUMENT_SIZE "size"
#define TCP_ARGUMENT_THREAD "thread"
#define TCP_ARGUMENT_TRIGGER_ID "trig_id"
#define TCP_ARGUMENT_STOP_STRING "stop_str"
#define TCP_ARGUMENT_INFO_STRING "info_str"
#define TCP_ARGUMENT_STATE "state"
#define TCP_ARGUMENT_EVENT "event"
#define TCP_ARGUMENT_DEVICE "device"
#define TCP_ARGUMENT_CORE "core"
#define TCP_ARGUMENT_AMOUNT_CORE "nr_cores"
#define TCP_ARGUMENT_AMOUNT_TRIGGER "nr_trigger"
#define TCP_ARGUMENT_OPTION "option"
#define TCP_ARGUMENT_ACTION "action"

/* for packets sent to qemu */
#define ARGUMENT_SEPARATOR ';'

/* core states */
#define CORE_STATE_RUNNING "running"
#define CORE_STATE_HALTED "halted"
#define CORE_STATE_DEBUG "debug"
#define CORE_STATE_UNKNOWN "unknown"

/* breakpoint types */
#define MCD_BREAKPOINT_HW 1
#define MCD_BREAKPOINT_READ 2
#define MCD_BREAKPOINT_WRITE 3
#define MCD_BREAKPOINT_RW 4

#endif
