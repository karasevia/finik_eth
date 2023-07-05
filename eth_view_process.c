#include "eth_view_process.h"
#include "eth_worker.h"
#include "eth_worker_i.h"
#include "finik_eth_icons.h"

#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/canvas.h>
#include <string.h>

#include "u8g2.h"

#define TAG "EthView"

EthViewProcess* ethernet_view_process_malloc(EthWorkerProcess type) {
    EthViewProcess* evp = malloc(sizeof(EthViewProcess));
    evp->type = type;
    evp->autofill = 1;
    evp->carriage = 0;
    evp->position = 0;
    evp->x = 27;
    evp->y = 6;

    if(type == EthWorkerProcessInit) {
        evp->y += 22;
        EthViewDrawInit* init = malloc(sizeof(EthViewDrawInit));
        memset(init, 0, sizeof(EthViewDrawInit));
        init->mac[0] = 0x10;
        init->mac[1] = 0x08;
        init->mac[2] = 0xDC;
        init->mac[3] = 0x47;
        init->mac[4] = 0x47;
        init->mac[5] = 0x54;
        evp->draw_struct = init;
    } else if(type == EthWorkerProcessStatic) {
        evp->y += 22;
        EthViewDrawStatic* stat = malloc(sizeof(EthViewDrawStatic));
        memset(stat, 0, sizeof(EthViewDrawStatic));
        stat->ip[0] = 192;
        stat->ip[1] = 168;
        stat->ip[2] = 0;
        stat->ip[3] = 101;
        stat->mask[0] = 255;
        stat->mask[1] = 255;
        stat->mask[2] = 255;
        stat->mask[3] = 0;
        stat->gateway[0] = 192;
        stat->gateway[1] = 168;
        stat->gateway[2] = 0;
        stat->gateway[3] = 1;
        stat->dns[0] = 192;
        stat->dns[1] = 168;
        stat->dns[2] = 0;
        stat->dns[3] = 1;
        evp->draw_struct = stat;
    } else if(type == EthWorkerProcessPing) {
        evp->y += 11;
        EthViewDrawPing* ping = malloc(sizeof(EthViewDrawPing));
        memset(ping, 0, sizeof(EthViewDrawPing));
        ping->ip[0] = 8;
        ping->ip[1] = 8;
        ping->ip[2] = 8;
        ping->ip[3] = 8;
        evp->draw_struct = ping;
    }
    return evp;
}

void ethernet_view_process_free(EthViewProcess* evp) {
    if(evp->type == EthWorkerProcessInit || evp->type == EthWorkerProcessStatic ||
       evp->type == EthWorkerProcessPing) {
        free(evp->draw_struct);
    }
    free(evp);
}

static void draw_hex_digit(Canvas* canvas, uint8_t x, uint8_t y, uint8_t digit) {
    char digit_str[] = "0";
    if(digit < 0xA) {
        digit_str[0] += digit;
    } else if(digit < 0x10) {
        digit_str[0] = 'A';
        digit_str[0] += digit - 0xA;
    } else {
        return;
    }

    canvas_draw_str(canvas, x, y, digit_str);
}

static void draw_dec_number(Canvas* canvas, uint8_t x, uint8_t y, uint8_t num) {
    char num_str[] = "0";
    {
        num_str[0] = '0' + num % 10;
        canvas_draw_str(canvas, x + 6 + 6, y, num_str);
    }
    if(num >= 10) {
        num_str[0] = '0' + num / 10 - (num / 100) * 10;
        canvas_draw_str(canvas, x + 6, y, num_str);
    }
    if(num >= 100) {
        num_str[0] = '0' + num / 100;
        canvas_draw_str(canvas, x, y, num_str);
    }
}

static void draw_static_mode(Canvas* canvas, uint8_t mode) {
    const uint8_t s1 = 13;
    const uint8_t s2 = 31;
    const uint8_t s3 = 19;
    const uint8_t s4 = 21;
    const uint8_t s = 35;
    const uint8_t h = 7;
    const uint8_t y = 10;
    const uint8_t y1 = 15;

    if(mode == EthViewDrawStaticModeIp) {
        canvas_invert_color(canvas);
        canvas_draw_box(canvas, s, y, s1, h);
        canvas_invert_color(canvas);
        canvas_draw_str(canvas, 38, y1, "ip");
    }
    if(mode == EthViewDrawStaticModeMask) {
        canvas_invert_color(canvas);
        canvas_draw_box(canvas, s + s1, y, s2, h);
        canvas_invert_color(canvas);
        canvas_draw_str(canvas, 53, y1, "mask");
    }
    if(mode == EthViewDrawStaticModeGateway) {
        canvas_invert_color(canvas);
        canvas_draw_box(canvas, s + s1 + s2, y, s3, h);
        canvas_invert_color(canvas);
        canvas_draw_str(canvas, 82, y1, "gw");
    }
    if(mode == EthViewDrawStaticModeDNS) {
        canvas_invert_color(canvas);
        canvas_draw_box(canvas, s + s1 + s2 + s3, y, s4, h);
        canvas_invert_color(canvas);
        canvas_draw_str(canvas, 102, y1, "dns");
    }
}

static uint8_t* draw_static_get_current_adress(EthViewDrawStatic* evds) {
    furi_assert(evds);
    if(evds->current_mode == EthViewDrawStaticModeIp) return evds->ip;
    if(evds->current_mode == EthViewDrawStaticModeMask) return evds->mask;
    if(evds->current_mode == EthViewDrawStaticModeGateway) return evds->gateway;
    if(evds->current_mode == EthViewDrawStaticModeDNS) return evds->dns;
    return evds->ip;
}

void ethernet_view_process_draw(EthViewProcess* process, Canvas* canvas) {
    furi_assert(canvas);
    furi_assert(process);
    canvas_set_font(canvas, FontSecondary);

    const uint8_t x = process->x;
    const uint8_t y = process->y;
    const uint8_t str_height = 11;
    const uint8_t str_count = (64 - y) / str_height;
    uint8_t carriage = process->carriage;
    uint8_t position = process->position;

    if(process->autofill) {
        position = (carriage + SCREEN_STRINGS_COUNT - str_count) % SCREEN_STRINGS_COUNT;
        process->position = position;
    }

    for(uint8_t i = 0; i < str_count; ++i) {
        uint8_t y1 = y + (i + 1) * str_height;
        canvas_draw_str(canvas, x, y1, process->fifo[(position + i) % SCREEN_STRINGS_COUNT]);
    }

    if(process->type == EthWorkerProcessInit) {
        uint8_t editing = process->editing;
        canvas_draw_icon(canvas, 27, 10, &I_init_100x19px);
        uint8_t octet = ((EthViewDrawInit*)process->draw_struct)->current_octet;
        uint8_t* mac = ((EthViewDrawInit*)process->draw_struct)->mac;
        for(uint8_t i = 0; i < 6; ++i) {
            uint8_t x1 = 29 + i * 17;
            uint8_t x2 = x1 + 6;
            draw_hex_digit(canvas, x1, 25, (mac[i] & 0x0F));
            draw_hex_digit(canvas, x2, 25, (mac[i] & 0xF0) >> 4);
            if(editing && (octet / 2 == i)) {
                uint8_t x = octet & 1 ? x2 : x1;
                canvas_draw_line(canvas, x, 26, x + 4, 26);
                canvas_draw_line(canvas, x, 27, x + 4, 27);
            }
        }
    } else if(process->type == EthWorkerProcessStatic) {
        canvas_draw_frame(canvas, 31, 18, 21, 13);
        canvas_draw_frame(canvas, 55, 18, 21, 13);
        canvas_draw_frame(canvas, 79, 18, 21, 13);
        canvas_draw_frame(canvas, 103, 18, 21, 13);
        canvas_draw_box(canvas, 29, 10, 97, 7);
        uint8_t mode = ((EthViewDrawStatic*)process->draw_struct)->current_mode;
        uint8_t current_digit = ((EthViewDrawStatic*)process->draw_struct)->current_digit;
        uint8_t* adress = draw_static_get_current_adress((EthViewDrawStatic*)process->draw_struct);
        uint8_t editing = ((EthViewDrawStatic*)process->draw_struct)->editing;
        for(uint8_t i = 0; i < 4; ++i) {
            if(i == mode && process->editing) {
                draw_static_mode(canvas, mode);
            } else {
                canvas_invert_color(canvas);
                draw_static_mode(canvas, i);
                canvas_invert_color(canvas);
            }
        }
        for(uint8_t i = 0; i < 4; ++i) {
            uint8_t x = 33 + i * 24;
            draw_dec_number(canvas, x, 27, adress[i]);
            if(editing && (current_digit / 3 == i)) {
                uint8_t x1 = x + 6 * (current_digit % 3);
                canvas_draw_line(canvas, x1, 28, x1 + 4, 28);
                canvas_draw_line(canvas, x1, 29, x1 + 4, 29);
            }
        }
    } else if(process->type == EthWorkerProcessPing) {
        canvas_draw_frame(canvas, 31, 8, 21, 13);
        canvas_draw_frame(canvas, 55, 8, 21, 13);
        canvas_draw_frame(canvas, 79, 8, 21, 13);
        canvas_draw_frame(canvas, 103, 8, 21, 13);
        uint8_t current_digit = ((EthViewDrawPing*)process->draw_struct)->current_digit;
        uint8_t* adress = ((EthViewDrawPing*)process->draw_struct)->ip;
        for(uint8_t i = 0; i < 4; ++i) {
            uint8_t x = 33 + i * 24;
            draw_dec_number(canvas, x, 17, adress[i]);
            if(process->editing && (current_digit / 3 == i)) {
                uint8_t x1 = x + 6 * (current_digit % 3);
                canvas_draw_line(canvas, x1, 18, x1 + 4, 18);
                canvas_draw_line(canvas, x1, 19, x1 + 4, 19);
            }
        }
    }
}

static void mac_change_hex_digit(uint8_t* mac, uint8_t octet, int8_t diff) {
    uint8_t digit = (octet & 1) ? (mac[octet / 2] >> 4) : (mac[octet / 2]);
    digit = (digit + diff) & 0xF;
    mac[octet / 2] = (mac[octet / 2] & ((octet & 1) ? 0x0F : 0xF0)) |
                     (digit << ((octet & 1) ? 4 : 0));
}

static void adress_change_dec_digit(uint8_t* ip, uint8_t digit, int8_t diff) {
    {
        uint8_t k = 0;
        k = (digit % 3 == 0) ? 100 : k;
        k = (digit % 3 == 1) ? 10 : k;
        k = (digit % 3 == 2) ? 1 : k;
        diff *= k;
    }
    {
        int16_t ip1 = ip[digit / 3];
        if(diff > 0 && ((0x100 - ip1) > diff)) ip1 += diff;
        if(diff < 0 && (ip1 + diff >= 0)) ip1 += diff;
        ip[digit / 3] = ip1;
    }
}

void ethernet_view_process_keyevent(EthViewProcess* process, InputKey key) {
    furi_assert(process);
    if(process->type == EthWorkerProcessInit) {
        uint8_t octet = ((EthViewDrawInit*)process->draw_struct)->current_octet;
        uint8_t* mac = ((EthViewDrawInit*)process->draw_struct)->mac;
        if(key == InputKeyLeft) {
            if(octet > 0) {
                octet -= 1;
            } else {
                process->editing = 0;
            }
        } else if(key == InputKeyRight) {
            if(octet < 11) octet += 1;
        } else if(key == InputKeyUp) {
            mac_change_hex_digit(mac, octet, 1);
        } else if(key == InputKeyDown) {
            mac_change_hex_digit(mac, octet, -1);
        } else if(key == InputKeyOk) {
            process->editing = 0;
        }
        ((EthViewDrawInit*)process->draw_struct)->current_octet = octet;
    } else if(process->type == EthWorkerProcessStatic) {
        uint8_t digit = ((EthViewDrawStatic*)process->draw_struct)->current_digit;
        uint8_t mode = ((EthViewDrawStatic*)process->draw_struct)->current_mode;
        uint8_t* adress = draw_static_get_current_adress((EthViewDrawStatic*)process->draw_struct);
        uint8_t editing = ((EthViewDrawStatic*)process->draw_struct)->editing;
        if(editing) {
            if(key == InputKeyLeft) {
                if(digit > 0) {
                    digit -= 1;
                } else {
                    ((EthViewDrawStatic*)process->draw_struct)->editing = 0;
                }
            } else if(key == InputKeyRight) {
                if(digit < 11) digit += 1;
            } else if(key == InputKeyUp) {
                adress_change_dec_digit(adress, digit, 1);
            } else if(key == InputKeyDown) {
                adress_change_dec_digit(adress, digit, -1);
            } else if(key == InputKeyOk || key == InputKeyBack) {
                ((EthViewDrawStatic*)process->draw_struct)->editing = 0;
            }
        } else {
            if(key == InputKeyLeft) {
                if(mode > 0) {
                    mode -= 1;
                } else {
                    process->editing = 0;
                }
            } else if(key == InputKeyRight) {
                if(mode < 3) {
                    mode += 1;
                }
            } else if(key == InputKeyDown || key == InputKeyOk) {
                ((EthViewDrawStatic*)process->draw_struct)->editing = 1;
            } else if(key == InputKeyBack || key == InputKeyUp) {
                mode = 0;
                process->editing = 0;
            }
        }
        ((EthViewDrawStatic*)process->draw_struct)->current_mode = mode;
        ((EthViewDrawStatic*)process->draw_struct)->current_digit = digit;
    } else if(process->type == EthWorkerProcessPing) {
        uint8_t digit = ((EthViewDrawPing*)process->draw_struct)->current_digit;
        uint8_t* adress = ((EthViewDrawPing*)process->draw_struct)->ip;
        if(key == InputKeyLeft) {
            if(digit > 0) {
                digit -= 1;
            } else {
                process->editing = 0;
            }
        } else if(key == InputKeyRight) {
            if(digit < 11) digit += 1;
        } else if(key == InputKeyUp) {
            adress_change_dec_digit(adress, digit, 1);
        } else if(key == InputKeyDown) {
            adress_change_dec_digit(adress, digit, -1);
        } else if(key == InputKeyOk || key == InputKeyBack) {
            process->editing = 0;
        }
        ((EthViewDrawPing*)process->draw_struct)->current_digit = digit;
    } else {
        if(key == InputKeyBack || key == InputKeyLeft) {
            process->editing = 0;
        }
    }
}

void ethernet_view_process_move(EthViewProcess* process, int8_t shift) {
    furi_assert(process);
    uint8_t position = process->position;
    if(shift <= -SCREEN_STRINGS_COUNT) {
        position = 0;
    } else if(shift >= SCREEN_STRINGS_COUNT) {
        position = process->carriage - 1;
    } else {
        position = (position + (SCREEN_STRINGS_COUNT + shift)) % SCREEN_STRINGS_COUNT;
    }
    process->position = position;
    process->autofill = !shift;
}

void ethernet_view_process_autofill(EthViewProcess* process, uint8_t state) {
    furi_assert(process);
    process->autofill = state;
}

static uint16_t get_string_with_width(const char* str, uint16_t width) {
    u8g2_t canvas_memory;
    Canvas* canvas = (Canvas*)&canvas_memory; // grazniy hack
    canvas_set_font(canvas, FontSecondary);

    uint8_t end = 0;
    char copy[SCREEN_SYMBOLS_WIDTH + 1] = {0};

    for(;;) {
        if(str[end] == '\0') {
            break;
        }
        if(end == SCREEN_SYMBOLS_WIDTH) {
            break;
        }
        copy[end] = str[end];
        if(canvas_string_width(canvas, copy) > width) {
            end -= 1;
            break;
        }
        end += 1;
    }

    return end;
}

void ethernet_view_process_print(EthViewProcess* process, const char* str) {
    furi_assert(process);

    uint16_t max_width = 126 - process->x;
    uint16_t ptr = 0;
    uint16_t len = strlen(str);

    while(ptr < len) {
        uint16_t start = ptr;
        ptr += get_string_with_width(str + ptr, max_width);
        uint8_t carriage = process->carriage;
        uint8_t carriage1 = (carriage + 1) % SCREEN_STRINGS_COUNT;
        uint8_t carriage2 = (carriage + 2) % SCREEN_STRINGS_COUNT;
        FURI_LOG_I(TAG, "print %d %d %d %d %d", max_width, len, start, carriage, carriage1);
        memset(process->fifo[carriage], 0, SCREEN_SYMBOLS_WIDTH);
        memset(process->fifo[carriage1], 0, SCREEN_SYMBOLS_WIDTH);
        memset(process->fifo[carriage2], 0, SCREEN_SYMBOLS_WIDTH);
        memcpy(process->fifo[carriage], str + start, ptr - start);
        process->carriage = carriage1;
    }
}
