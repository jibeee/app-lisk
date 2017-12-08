#include "os_io_seproxyhal.h"
#include "structs.h"


extern const ux_menu_entry_t menu_main[4];
extern const ux_menu_entry_t menu_about[3];

extern bagl_element_t bagl_ui_approval_nanos[5];
extern bagl_element_t bagl_ui_text_review_nanos[5];
extern bagl_element_t bagl_ui_approval_send_nanos[7];
extern char lineBuffer[50];
extern uint8_t lineBufferLength;

void satoshiToString(uint64_t amount, uint8_t *out);
void lineBufferSendTxProcessor(signContext_t *signContext, uint8_t step);