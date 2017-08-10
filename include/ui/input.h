#ifndef INPUT_H
#define INPUT_H

#include "qapi-types.h"

#define INPUT_EVENT_MASK_KEY   (1<<INPUT_EVENT_KIND_KEY)
#define INPUT_EVENT_MASK_BTN   (1<<INPUT_EVENT_KIND_BTN)
#define INPUT_EVENT_MASK_REL   (1<<INPUT_EVENT_KIND_REL)
#define INPUT_EVENT_MASK_ABS   (1<<INPUT_EVENT_KIND_ABS)

#define INPUT_EVENT_ABS_MIN    0x0000
#define INPUT_EVENT_ABS_MAX    0x7FFF

typedef struct QemuInputHandler QemuInputHandler;
typedef struct QemuInputHandlerState QemuInputHandlerState;

typedef void (*QemuInputHandlerEvent)(DeviceState *dev, QemuConsole *src,
                                      InputEvent *evt);
typedef void (*QemuInputHandlerSync)(DeviceState *dev);

struct QemuInputHandler {
    const char             *name;
    uint32_t               mask;
    QemuInputHandlerEvent  event;
    QemuInputHandlerSync   sync;
};

QemuInputHandlerState *qemu_input_handler_register(DeviceState *dev,
                                                   QemuInputHandler *handler);
void qemu_input_handler_activate(QemuInputHandlerState *s);
void qemu_input_handler_deactivate(QemuInputHandlerState *s);
void qemu_input_handler_unregister(QemuInputHandlerState *s);
void qemu_input_handler_bind(QemuInputHandlerState *s,
                             const char *device_id, int head,
                             Error **errp);
void qemu_input_event_send(QemuConsole *src, InputEvent *evt);
void qemu_input_event_send_impl(QemuConsole *src, InputEvent *evt);
void qemu_input_event_sync(void);
void qemu_input_event_sync_impl(void);

void qemu_input_event_send_key_number(QemuConsole *src, int num, bool down);
void qemu_input_event_send_key_qcode(QemuConsole *src, QKeyCode q, bool down);
void qemu_input_event_send_key_delay(uint32_t delay_ms);
int qemu_input_key_number_to_qcode(unsigned int nr);
int qemu_input_qcode_to_scancode(QKeyCode qcode, bool down, int *codes);
int qemu_input_linux_to_qcode(unsigned int lnx);

InputEvent *qemu_input_event_new_btn(InputButton btn, bool down);
void qemu_input_queue_btn(QemuConsole *src, InputButton btn, bool down);
void qemu_input_update_buttons(QemuConsole *src, uint32_t *button_map,
                               uint32_t button_old, uint32_t button_new);

bool qemu_input_is_absolute(void);
int qemu_input_scale_axis(int value,
                          int min_in, int max_in,
                          int min_out, int max_out);
InputEvent *qemu_input_event_new_move(InputEventKind kind,
                                      InputAxis axis, int value);
void qemu_input_queue_rel(QemuConsole *src, InputAxis axis, int value);
void qemu_input_queue_abs(QemuConsole *src, InputAxis axis, int value,
                          int min_in, int max_in);

void qemu_input_check_mode_change(void);
void qemu_add_mouse_mode_change_notifier(Notifier *notify);
void qemu_remove_mouse_mode_change_notifier(Notifier *notify);

extern const guint qemu_input_map_atset12qcode_len;
extern const guint16 qemu_input_map_atset12qcode[];

extern const guint qemu_input_map_linux2qcode_len;
extern const guint16 qemu_input_map_linux2qcode[];

extern const guint qemu_input_map_osx2qcode_len;
extern const guint16 qemu_input_map_osx2qcode[];

extern const guint qemu_input_map_qcode2adb_len;
extern const guint16 qemu_input_map_qcode2adb[];

extern const guint qemu_input_map_qcode2atset1_len;
extern const guint16 qemu_input_map_qcode2atset1[];

extern const guint qemu_input_map_qcode2atset2_len;
extern const guint16 qemu_input_map_qcode2atset2[];

extern const guint qemu_input_map_qcode2atset3_len;
extern const guint16 qemu_input_map_qcode2atset3[];

extern const guint qemu_input_map_qcode2linux_len;
extern const guint16 qemu_input_map_qcode2linux[];

extern const guint qemu_input_map_qcode2qnum_len;
extern const guint16 qemu_input_map_qcode2qnum[];

extern const guint qemu_input_map_qcode2sun_len;
extern const guint16 qemu_input_map_qcode2sun[];

extern const guint qemu_input_map_qnum2qcode_len;
extern const guint16 qemu_input_map_qnum2qcode[];

extern const guint qemu_input_map_usb2qcode_len;
extern const guint16 qemu_input_map_usb2qcode[];

extern const guint qemu_input_map_win322qcode_len;
extern const guint16 qemu_input_map_win322qcode[];

extern const guint qemu_input_map_x112qcode_len;
extern const guint16 qemu_input_map_x112qcode[];

extern const guint qemu_input_map_xorgevdev2qcode_len;
extern const guint16 qemu_input_map_xorgevdev2qcode[];

extern const guint qemu_input_map_xorgkbd2qcode_len;
extern const guint16 qemu_input_map_xorgkbd2qcode[];

extern const guint qemu_input_map_xorgxquartz2qcode_len;
extern const guint16 qemu_input_map_xorgxquartz2qcode[];

extern const guint qemu_input_map_xorgxwin2qcode_len;
extern const guint16 qemu_input_map_xorgxwin2qcode[];

#endif /* INPUT_H */
