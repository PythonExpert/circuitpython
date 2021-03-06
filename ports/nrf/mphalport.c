/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Glenn Ruben Bakke
 * Copyright (c) 2018 Artur Pacholec
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>

#include "py/mphal.h"
#include "py/mpstate.h"
#include "py/gc.h"

#if (MICROPY_PY_BLE_NUS == 0)

#if !defined( NRF52840_XXAA)
int mp_hal_stdin_rx_chr(void) {
    uint8_t data;
    nrfx_uarte_rx(&serial_instance, &data, 1);
    return data;
}

bool mp_hal_stdin_any(void) {
    return nrf_uarte_event_check(serial_instance.p_reg, NRF_UARTE_EVENT_RXDRDY);
}

void mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    if (len == 0) {
        return;
    }

    // EasyDMA can only access SRAM
    uint8_t * tx_buf = (uint8_t*) str;
    if ( !nrfx_is_in_ram(str) ) {
        tx_buf = (uint8_t *) gc_alloc(len, false, false);
        memcpy(tx_buf, str, len);
    }

    nrfx_uarte_tx(&serial_instance, tx_buf, len);

    if ( !nrfx_is_in_ram(str) ) {
        gc_free(tx_buf);
    }
}

#else

#include "tusb.h"

int mp_hal_stdin_rx_chr(void) {
    for (;;) {
        #ifdef MICROPY_VM_HOOK_LOOP
            MICROPY_VM_HOOK_LOOP
        #endif
        // if (reload_requested) {
        //     return CHAR_CTRL_D;
        // }

        if (tud_cdc_available()) {
            #ifdef MICROPY_HW_LED_RX
            gpio_toggle_pin_level(MICROPY_HW_LED_RX);
            #endif
            return tud_cdc_read_char();
        }
    }

    return 0;
}

bool mp_hal_stdin_any(void) {
    return tud_cdc_available() > 0;
}

void mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {

    #ifdef MICROPY_HW_LED_TX
    gpio_toggle_pin_level(MICROPY_HW_LED_TX);
    #endif

    #ifdef CIRCUITPY_BOOT_OUTPUT_FILE
    if (boot_output_file != NULL) {
        UINT bytes_written = 0;
        f_write(boot_output_file, str, len, &bytes_written);
    }
    #endif

    tud_cdc_write(str, len);
}

#endif // USB

#endif // NUS


/*------------------------------------------------------------------*/
/* delay
 *------------------------------------------------------------------*/
void mp_hal_delay_ms(mp_uint_t delay) {
    uint64_t start_tick = ticks_ms;
    uint64_t duration = 0;
    while (duration < delay) {
        #ifdef MICROPY_VM_HOOK_LOOP
            MICROPY_VM_HOOK_LOOP
        #endif
        // Check to see if we've been CTRL-Ced by autoreload or the user.
        if(MP_STATE_VM(mp_pending_exception) == MP_OBJ_FROM_PTR(&MP_STATE_VM(mp_kbd_exception))) {
            break;
        }
        duration = (ticks_ms - start_tick);
        // TODO(tannewt): Go to sleep for a little while while we wait.
    }
}

