/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "os.h"
#include "cx.h"

#include "os_io_seproxyhal.h"
#include "register.h"
#include "start_check.h"
#include "parse_tx.h"

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

static unsigned int current_text_pos; // parsing cursor in the text to display
static unsigned int text_y;           // current location of the displayed text
static unsigned char hashTainted;     // notification to restart the hash

WIDE DMS_parameters_t N_storage_DMS_parameter;
#define N_storage (*(WIDE DMS_parameters_t *)PIC(&N_storage_DMS_parameter))

// UI currently displayed
enum UI_STATE { UI_IDLE, UI_TEXT, UI_APPROVAL };

enum UI_STATE uiState;

ux_state_t ux;

static const bagl_element_t *io_seproxyhal_touch_exit(const bagl_element_t *e);
static const bagl_element_t*
io_seproxyhal_touch_approve(const bagl_element_t *e);
static const bagl_element_t *io_seproxyhal_touch_deny(const bagl_element_t *e);

static void ui_idle(void);
static unsigned char display_text_part(void);
static void ui_text(void);
static void ui_approval(void);

#define MAX_CHARS_PER_LINE 49
#define DEFAULT_FONT BAGL_FONT_OPEN_SANS_LIGHT_16px | BAGL_FONT_ALIGNMENT_LEFT
#define TEXT_HEIGHT 15
#define TEXT_SPACE 4

#define CLA 0x80
#define INS_REGISTER 0x01
#define INS_START_CHECK 0x02
#define INS_STREAM_BLOCK 0x03
#define INS_STREAM_TX 0x04

#define OFFSET_CLA 0
#define OFFSET_INS 1
#define OFFSET_P1 2
#define OFFSET_P2 3
#define OFFSET_LC 4
#define OFFSET_CDATA 5

// private key in flash. const and N_ variable name are mandatory here
static const cx_ecfp_private_key_t N_privateKey;
// initialization marker in flash. const and N_ variable name are mandatory here
static const unsigned char N_initialized;

static char lineBuffer[50];

check_ctx_t check_ctx;

static const bagl_element_t bagl_ui_idle_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 0, 12, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        "Waiting for message",
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CROSS},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
};

static unsigned int
bagl_ui_idle_nanos_button(unsigned int button_mask,
                          unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
    case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT:
        io_seproxyhal_touch_exit(NULL);
        break;
    }

    return 0;
}

static const bagl_element_t bagl_ui_approval_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 0, 12, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        "Sign message",
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CROSS},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CHECK},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
};

static unsigned int
bagl_ui_approval_nanos_button(unsigned int button_mask,
                              unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        io_seproxyhal_touch_approve(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_deny(NULL);
        break;
    }
    return 0;
}

static const bagl_element_t bagl_ui_text_review_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 0, 12, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        "Verify text",
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 23, 26, 82, 11, 0x80 | 10, 0, 0, 0xFFFFFF,
         0x000000, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px | BAGL_FONT_ALIGNMENT_CENTER, 26},
        lineBuffer,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CROSS},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CHECK},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
};

static unsigned int
bagl_ui_text_review_nanos_button(unsigned int button_mask,
                                 unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        if (!display_text_part()) {
            ui_approval();
        } else {
            UX_REDISPLAY();
        }
        break;

    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_deny(NULL);
        break;
    }
    return 0;
}


static const bagl_element_t *io_seproxyhal_touch_exit(const bagl_element_t *e) {
    // Go back to the dashboard
    os_sched_exit(0);
    return NULL; // do not redraw the widget
}

static const bagl_element_t*
io_seproxyhal_touch_approve(const bagl_element_t *e) {
    unsigned int tx = 0;
    
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

static const bagl_element_t *io_seproxyhal_touch_deny(const bagl_element_t *e) {
    hashTainted = 1;
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

static void sample_main(void) {
    volatile unsigned int rx = 0;
    volatile unsigned int tx = 0;
    volatile unsigned int flags = 0;


    // next timer callback in 500 ms
    UX_CALLBACK_SET_INTERVAL(500);

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the apdu is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        volatile unsigned short sw = 0;

        BEGIN_TRY {
            TRY {
                rx = tx;
                tx = 0; // ensure no race in catch_other if io_exchange throws
                        // an error
                rx = io_exchange(CHANNEL_APDU | flags, rx);
                flags = 0;

                // no apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx == 0) {
                    THROW(0x6982);
                }

                if (G_io_apdu_buffer[0] != CLA) {
                    THROW(0x6E00);
                }

                switch (G_io_apdu_buffer[1]) 
                {

                    case INS_REGISTER: 
                    {
                        if(register_param(&G_io_apdu_buffer[OFFSET_CDATA])) THROW(ERR_ALREADY_REGISTERED);

                        PRINTF("Registered pub_key: %.*H\n", 32, N_storage.pub_key);
                        PRINTF("Registered number of block before secret release: %d\n", N_storage.release_blocks_num);
                        PRINTF("First block hash: %.*H\n", 32, N_storage.first_block_hash);
                        PRINTF("Registered secret: %.*H\n", 64, N_storage.secret);
                        
                        THROW(0x9000);
                    }
                
                    case INS_START_CHECK:
                    {
                        if(N_storage.initialized != 1) 
                        {
                            check_ctx.machine_state=UNINITIALIZED;
                            THROW(ERR_NVRAM_NOT_INITIALIZED);
                        }

                        init_check_ctx();
                        os_memcpy(G_io_apdu_buffer, check_ctx.current_block_hash, 32);
                        tx += 32;
                        THROW(0x9000);
                    }

                    case INS_STREAM_BLOCK:
                    {
                        if(check_ctx.machine_state != WAITING_FOR_BLOCK)
                        {
                            check_ctx.machine_state=UNINITIALIZED;
                             THROW(ERR_WRONG_STATE_TRANSITION);
                        }

                        parse_block_header(&G_io_apdu_buffer[OFFSET_CDATA]);

                        THROW(0x9000);
                    }

                    case INS_STREAM_TX:
                    {
                        if(check_ctx.machine_state != WAITING_FOR_TX)
                        {
                            check_ctx.machine_state=UNINITIALIZED;
                            THROW(ERR_WRONG_STATE_TRANSITION);
                        } 
                        if((G_io_apdu_buffer[OFFSET_P1] != P1_LAST && G_io_apdu_buffer[OFFSET_P1] != P1_MORE) 
                          || (G_io_apdu_buffer[OFFSET_P2] != P2_SEGWIT && G_io_apdu_buffer[OFFSET_P2] != P2_NO_SEGWIT))
                        {
                            check_ctx.machine_state=UNINITIALIZED;
                            THROW(ERR_WRONG_TX_FLAGS);
                        }

                        tx += parse_tx(&G_io_apdu_buffer[OFFSET_CDATA], G_io_apdu_buffer[OFFSET_P1], G_io_apdu_buffer[OFFSET_P2]);

                        if(check_ctx.tx_remaining_in_block == 0)
                        {
                            // TODO: build Merkle tree to validate block

                            check_ctx.blocks_elapsed += 1;

                            if(check_ctx.if_block_valid_reset_counter == 1)
                            {
                                check_ctx.blocks_elapsed = 0;
                            }
                            if(check_ctx.blocks_elapsed >= N_storage.release_blocks_num)
                            {
                                PRINTF("KRAKEN RELEAAAASED\n")
                                os_memcpy(G_io_apdu_buffer, N_storage.secret, 64);
                                tx = 64;
                            }
                        }
                        
                        THROW(0x9000);
                    }


                    case 0xFF: // return to dashboard
                        goto return_to_dashboard;

                    default:
                        THROW(0x6D00);
                        break;
                }
            }
            CATCH_OTHER(e) {
                switch (e & 0xF000) {
                case 0x6000:
                case 0x9000:
                    sw = e;
                    break;
                default:
                    sw = 0x6800 | (e & 0x7FF);
                    break;
                }
                // Unexpected exception => report
                G_io_apdu_buffer[tx] = sw >> 8;
                G_io_apdu_buffer[tx + 1] = sw;
                tx += 2;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

return_to_dashboard:
    return;
}

void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *)element);
}

// Pick the text elements to display
static unsigned char display_text_part() {
    unsigned int i;
    WIDE char *text = (char*) G_io_apdu_buffer + 5;
    if (text[current_text_pos] == '\0') {
        return 0;
    }
    i = 0;
    while ((text[current_text_pos] != 0) && (text[current_text_pos] != '\n') &&
           (i < MAX_CHARS_PER_LINE)) {
        lineBuffer[i++] = text[current_text_pos];
        current_text_pos++;
    }
    if (text[current_text_pos] == '\n') {
        current_text_pos++;
    }
    lineBuffer[i] = '\0';
#ifdef TARGET_BLUE
    os_memset(bagl_ui_text, 0, sizeof(bagl_ui_text));
    bagl_ui_text[0].component.type = BAGL_LABEL;
    bagl_ui_text[0].component.x = 4;
    bagl_ui_text[0].component.y = text_y;
    bagl_ui_text[0].component.width = 320;
    bagl_ui_text[0].component.height = TEXT_HEIGHT;
    // element.component.fill = BAGL_FILL;
    bagl_ui_text[0].component.fgcolor = 0x000000;
    bagl_ui_text[0].component.bgcolor = 0xf9f9f9;
    bagl_ui_text[0].component.font_id = DEFAULT_FONT;
    bagl_ui_text[0].text = lineBuffer;
    text_y += TEXT_HEIGHT + TEXT_SPACE;
#endif
    return 1;
}

static void ui_idle(void) {
    uiState = UI_IDLE;
#ifdef TARGET_BLUE
    UX_DISPLAY(bagl_ui_idle_blue, NULL);
#else
    UX_DISPLAY(bagl_ui_idle_nanos, NULL);
#endif
}

static void ui_text(void) {
    uiState = UI_TEXT;
#ifdef TARGET_BLUE
    UX_DISPLAY(bagl_ui_text, NULL);
#else
    UX_DISPLAY(bagl_ui_text_review_nanos, NULL);
#endif
}

static void ui_approval(void) {
    uiState = UI_APPROVAL;
#ifdef TARGET_BLUE
    UX_DISPLAY(bagl_ui_approval_blue, NULL);
#else
    UX_DISPLAY(bagl_ui_approval_nanos, NULL);
#endif
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT: // for Nano S
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        if ((uiState == UI_TEXT) &&
            (os_seph_features() &
             SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_SCREEN_BIG)) {
            if (!display_text_part()) {
                ui_approval();
            } else {
                UX_REDISPLAY();
            }
        } else {
            UX_DISPLAYED_EVENT();
        }
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        #ifdef TARGET_NANOS
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {
                // defaulty retrig very soon (will be overriden during
                // stepper_prepro)
                UX_CALLBACK_SET_INTERVAL(500);
                UX_REDISPLAY();
            });
        #endif 
        break;

    // unknown events are acknowledged
    default:
        UX_DEFAULT_EVENT();
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    current_text_pos = 0;
    text_y = 60;
    hashTainted = 1;
    uiState = UI_IDLE;

    // ensure exception will work as planned
    os_boot();

    UX_INIT();

    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();

            check_ctx.machine_state = UNINITIALIZED;

#ifdef LISTEN_BLE
            if (os_seph_features() &
                SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_BLE) {
                BLE_power(0, NULL);
                // restart IOs
                BLE_power(1, NULL);
            }
#endif

            USB_power(0);
            USB_power(1);

            ui_idle();

            sample_main();
        }
        CATCH_OTHER(e) {
        }
        FINALLY {
        }
    }
    END_TRY;
}
