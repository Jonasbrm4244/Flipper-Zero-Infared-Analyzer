#pragma once

#include <infrared.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char device_type[32];  // e.g. "Samsung Smart TV"
    char brand[20];        // e.g. "Samsung"
    char button_name[24];  // e.g. "Power (An/Aus)"
    char display_name[32]; // e.g. "Samsung_Power_02"
    uint32_t frequency;    // in Hz, e.g. 38000
    uint8_t duty_cycle;    // in percent, e.g. 33
} IrSignalAnalysis;

static inline void infrared_analyze_signal(
    InfraredProtocol protocol,
    uint32_t address,
    uint32_t command,
    IrSignalAnalysis* analysis) {
    if(!analysis) return;

    memset(analysis, 0, sizeof(IrSignalAnalysis));

    if(infrared_is_protocol_valid(protocol)) {
        analysis->frequency = infrared_get_protocol_frequency(protocol);
        analysis->duty_cycle = (uint8_t)(infrared_get_protocol_duty_cycle(protocol) * 100.0f);
    } else {
        analysis->frequency = 38000;
        analysis->duty_cycle = 33;
    }

    const char* p_name = infrared_is_protocol_valid(protocol) ? infrared_get_protocol_name(protocol) : "Unbekannt";
    if(!p_name) p_name = "Unbekannt";

    // 1. Samsung32
    if(protocol == InfraredProtocolSamsung32) {
        strncpy(analysis->brand, "Samsung", sizeof(analysis->brand) - 1);
        switch(address & 0xFF) {
        case 0x07:
            strncpy(analysis->device_type, "Samsung Smart TV", sizeof(analysis->device_type) - 1);
            break;
        case 0x01:
            strncpy(analysis->device_type, "Samsung DVD/BluRay", sizeof(analysis->device_type) - 1);
            break;
        case 0x02:
            strncpy(analysis->device_type, "Samsung Soundbar/Audio", sizeof(analysis->device_type) - 1);
            break;
        case 0x0E:
            strncpy(analysis->device_type, "Samsung Klimaanlage", sizeof(analysis->device_type) - 1);
            break;
        case 0x08:
            strncpy(analysis->device_type, "Samsung Set-Top-Box", sizeof(analysis->device_type) - 1);
            break;
        default:
            snprintf(analysis->device_type, sizeof(analysis->device_type), "Samsung 0x%02lX", (unsigned long)(address & 0xFF));
            break;
        }

        switch(command & 0xFF) {
        case 0x02: strncpy(analysis->button_name, "Power (An/Aus)", sizeof(analysis->button_name) - 1); break;
        case 0x07: strncpy(analysis->button_name, "Lautstaerke +", sizeof(analysis->button_name) - 1); break;
        case 0x0B: strncpy(analysis->button_name, "Lautstaerke -", sizeof(analysis->button_name) - 1); break;
        case 0x0F: strncpy(analysis->button_name, "Stumm (Mute)", sizeof(analysis->button_name) - 1); break;
        case 0x12: strncpy(analysis->button_name, "Kanal +", sizeof(analysis->button_name) - 1); break;
        case 0x10: strncpy(analysis->button_name, "Kanal -", sizeof(analysis->button_name) - 1); break;
        case 0x01: strncpy(analysis->button_name, "Quelle (Source)", sizeof(analysis->button_name) - 1); break;
        case 0x1A: strncpy(analysis->button_name, "Menue", sizeof(analysis->button_name) - 1); break;
        case 0x58: strncpy(analysis->button_name, "Exit / Zurueck", sizeof(analysis->button_name) - 1); break;
        case 0x68: strncpy(analysis->button_name, "OK / Enter", sizeof(analysis->button_name) - 1); break;
        case 0x60: strncpy(analysis->button_name, "Pfeil Hoch", sizeof(analysis->button_name) - 1); break;
        case 0x61: strncpy(analysis->button_name, "Pfeil Runter", sizeof(analysis->button_name) - 1); break;
        case 0x65: strncpy(analysis->button_name, "Pfeil Links", sizeof(analysis->button_name) - 1); break;
        case 0x62: strncpy(analysis->button_name, "Pfeil Rechts", sizeof(analysis->button_name) - 1); break;
        case 0x04: strncpy(analysis->button_name, "Taste 1", sizeof(analysis->button_name) - 1); break;
        case 0x05: strncpy(analysis->button_name, "Taste 2", sizeof(analysis->button_name) - 1); break;
        case 0x06: strncpy(analysis->button_name, "Taste 3", sizeof(analysis->button_name) - 1); break;
        case 0x08: strncpy(analysis->button_name, "Taste 4", sizeof(analysis->button_name) - 1); break;
        case 0x09: strncpy(analysis->button_name, "Taste 5", sizeof(analysis->button_name) - 1); break;
        case 0x0A: strncpy(analysis->button_name, "Taste 6", sizeof(analysis->button_name) - 1); break;
        case 0x0C: strncpy(analysis->button_name, "Taste 7", sizeof(analysis->button_name) - 1); break;
        case 0x0D: strncpy(analysis->button_name, "Taste 8", sizeof(analysis->button_name) - 1); break;
        case 0x0E: strncpy(analysis->button_name, "Taste 9", sizeof(analysis->button_name) - 1); break;
        case 0x11: strncpy(analysis->button_name, "Taste 0", sizeof(analysis->button_name) - 1); break;
        case 0x79: strncpy(analysis->button_name, "Smart Hub", sizeof(analysis->button_name) - 1); break;
        default:
            snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
            break;
        }
    }
    // 2. Sony SIRC (12, 15, 20 Bit)
    else if(protocol == InfraredProtocolSIRC || protocol == InfraredProtocolSIRC15 || protocol == InfraredProtocolSIRC20) {
        strncpy(analysis->brand, "Sony", sizeof(analysis->brand) - 1);
        switch(address & 0xFF) {
        case 0x01: strncpy(analysis->device_type, "Sony Bravia TV", sizeof(analysis->device_type) - 1); break;
        case 0x02: strncpy(analysis->device_type, "Sony VCR/Recorder", sizeof(analysis->device_type) - 1); break;
        case 0x04: strncpy(analysis->device_type, "Sony Laserdisc", sizeof(analysis->device_type) - 1); break;
        case 0x0B: strncpy(analysis->device_type, "Sony DVD/BluRay", sizeof(analysis->device_type) - 1); break;
        case 0x0C: strncpy(analysis->device_type, "Sony CD-Player", sizeof(analysis->device_type) - 1); break;
        case 0x10: strncpy(analysis->device_type, "Sony AV-Receiver", sizeof(analysis->device_type) - 1); break;
        case 0x11: strncpy(analysis->device_type, "Sony Tuner/Radio", sizeof(analysis->device_type) - 1); break;
        case 0x13: strncpy(analysis->device_type, "Sony MiniDisc", sizeof(analysis->device_type) - 1); break;
        case 0x1A: strncpy(analysis->device_type, "Sony PlayStation", sizeof(analysis->device_type) - 1); break;
        case 0x1E: strncpy(analysis->device_type, "Sony Projektor", sizeof(analysis->device_type) - 1); break;
        default:
            snprintf(analysis->device_type, sizeof(analysis->device_type), "Sony 0x%02lX", (unsigned long)(address & 0xFF));
            break;
        }

        switch(command & 0xFF) {
        case 0x15: strncpy(analysis->button_name, "Power (An/Aus)", sizeof(analysis->button_name) - 1); break;
        case 0x12: strncpy(analysis->button_name, "Lautstaerke +", sizeof(analysis->button_name) - 1); break;
        case 0x13: strncpy(analysis->button_name, "Lautstaerke -", sizeof(analysis->button_name) - 1); break;
        case 0x14: strncpy(analysis->button_name, "Stumm (Mute)", sizeof(analysis->button_name) - 1); break;
        case 0x10: strncpy(analysis->button_name, "Kanal +", sizeof(analysis->button_name) - 1); break;
        case 0x11: strncpy(analysis->button_name, "Kanal -", sizeof(analysis->button_name) - 1); break;
        case 0x00: strncpy(analysis->button_name, "Taste 1", sizeof(analysis->button_name) - 1); break;
        case 0x01: strncpy(analysis->button_name, "Taste 2", sizeof(analysis->button_name) - 1); break;
        case 0x02: strncpy(analysis->button_name, "Taste 3", sizeof(analysis->button_name) - 1); break;
        case 0x03: strncpy(analysis->button_name, "Taste 4", sizeof(analysis->button_name) - 1); break;
        case 0x04: strncpy(analysis->button_name, "Taste 5", sizeof(analysis->button_name) - 1); break;
        case 0x05: strncpy(analysis->button_name, "Taste 6", sizeof(analysis->button_name) - 1); break;
        case 0x06: strncpy(analysis->button_name, "Taste 7", sizeof(analysis->button_name) - 1); break;
        case 0x07: strncpy(analysis->button_name, "Taste 8", sizeof(analysis->button_name) - 1); break;
        case 0x08: strncpy(analysis->button_name, "Taste 9", sizeof(analysis->button_name) - 1); break;
        case 0x09: strncpy(analysis->button_name, "Taste 0", sizeof(analysis->button_name) - 1); break;
        case 0x25: strncpy(analysis->button_name, "Quelle / Input", sizeof(analysis->button_name) - 1); break;
        case 0x60: strncpy(analysis->button_name, "Home / Menue", sizeof(analysis->button_name) - 1); break;
        case 0x65: strncpy(analysis->button_name, "OK / Auswaehlen", sizeof(analysis->button_name) - 1); break;
        case 0x34: strncpy(analysis->button_name, "Play", sizeof(analysis->button_name) - 1); break;
        case 0x38: strncpy(analysis->button_name, "Stop", sizeof(analysis->button_name) - 1); break;
        case 0x39: strncpy(analysis->button_name, "Pause", sizeof(analysis->button_name) - 1); break;
        default:
            snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
            break;
        }
    }
    // 3. RC5 / RC5X
    else if(protocol == InfraredProtocolRC5 || protocol == InfraredProtocolRC5X) {
        strncpy(analysis->brand, "Philips", sizeof(analysis->brand) - 1);
        switch(address & 0x1F) {
        case 0x00:
        case 0x01: strncpy(analysis->device_type, "Philips TV", sizeof(analysis->device_type) - 1); break;
        case 0x05: strncpy(analysis->device_type, "Philips VCR", sizeof(analysis->device_type) - 1); break;
        case 0x10: strncpy(analysis->device_type, "Marantz/Philips Audio", sizeof(analysis->device_type) - 1); break;
        case 0x13: strncpy(analysis->device_type, "Philips/Marantz CD", sizeof(analysis->device_type) - 1); break;
        case 0x14: strncpy(analysis->device_type, "Philips Verstaerker", sizeof(analysis->device_type) - 1); break;
        case 0x15: strncpy(analysis->device_type, "Philips DVD", sizeof(analysis->device_type) - 1); break;
        case 0x17: strncpy(analysis->device_type, "Licht / Dimmer", sizeof(analysis->device_type) - 1); break;
        default:
            snprintf(analysis->device_type, sizeof(analysis->device_type), "RC5 0x%02lX", (unsigned long)(address & 0x1F));
            break;
        }

        switch(command & 0x3F) {
        case 0x0C: strncpy(analysis->button_name, "Power / Standby", sizeof(analysis->button_name) - 1); break;
        case 0x0D: strncpy(analysis->button_name, "Stumm (Mute)", sizeof(analysis->button_name) - 1); break;
        case 0x10: strncpy(analysis->button_name, "Lautstaerke +", sizeof(analysis->button_name) - 1); break;
        case 0x11: strncpy(analysis->button_name, "Lautstaerke -", sizeof(analysis->button_name) - 1); break;
        case 0x20: strncpy(analysis->button_name, "Kanal +", sizeof(analysis->button_name) - 1); break;
        case 0x21: strncpy(analysis->button_name, "Kanal -", sizeof(analysis->button_name) - 1); break;
        case 0x35: strncpy(analysis->button_name, "Play", sizeof(analysis->button_name) - 1); break;
        case 0x36: strncpy(analysis->button_name, "Stop", sizeof(analysis->button_name) - 1); break;
        case 0x30: strncpy(analysis->button_name, "Pause", sizeof(analysis->button_name) - 1); break;
        case 0x3F: strncpy(analysis->button_name, "Quelle (Source)", sizeof(analysis->button_name) - 1); break;
        default:
            if((command & 0x3F) <= 9) {
                snprintf(analysis->button_name, sizeof(analysis->button_name), "Taste %lu", (unsigned long)(command & 0x3F));
            } else {
                snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0x3F));
            }
            break;
        }
    }
    // 4. RC6
    else if(protocol == InfraredProtocolRC6) {
        strncpy(analysis->brand, "Philips", sizeof(analysis->brand) - 1);
        switch(address & 0xFF) {
        case 0x00: strncpy(analysis->device_type, "Philips TV", sizeof(analysis->device_type) - 1); break;
        case 0x80:
            strncpy(analysis->brand, "Microsoft", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Windows MCE / Xbox", sizeof(analysis->device_type) - 1);
            break;
        case 0x1B:
            strncpy(analysis->brand, "Roku", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Roku Streaming Box", sizeof(analysis->device_type) - 1);
            break;
        default:
            snprintf(analysis->device_type, sizeof(analysis->device_type), "RC6 0x%02lX", (unsigned long)(address & 0xFF));
            break;
        }

        switch(command & 0xFF) {
        case 0x0C: strncpy(analysis->button_name, "Power (An/Aus)", sizeof(analysis->button_name) - 1); break;
        case 0x0D: strncpy(analysis->button_name, "Stumm (Mute)", sizeof(analysis->button_name) - 1); break;
        case 0x10: strncpy(analysis->button_name, "Lautstaerke +", sizeof(analysis->button_name) - 1); break;
        case 0x11: strncpy(analysis->button_name, "Lautstaerke -", sizeof(analysis->button_name) - 1); break;
        case 0x20: strncpy(analysis->button_name, "Kanal +", sizeof(analysis->button_name) - 1); break;
        case 0x21: strncpy(analysis->button_name, "Kanal -", sizeof(analysis->button_name) - 1); break;
        case 0x5C: strncpy(analysis->button_name, "OK / Enter", sizeof(analysis->button_name) - 1); break;
        default:
            snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
            break;
        }
    }
    // 5. NEC / NECext
    else if(protocol == InfraredProtocolNEC || protocol == InfraredProtocolNECext) {
        uint8_t addr_low = (uint8_t)(address & 0xFF);
        if(addr_low == 0xEE || addr_low == 0x87) {
            strncpy(analysis->brand, "Apple", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Apple TV / Remote", sizeof(analysis->device_type) - 1);
            switch(command & 0xFF) {
            case 0x02: strncpy(analysis->button_name, "Menue", sizeof(analysis->button_name) - 1); break;
            case 0x04: strncpy(analysis->button_name, "Play / Pause", sizeof(analysis->button_name) - 1); break;
            case 0x07: strncpy(analysis->button_name, "Rechts / Vor", sizeof(analysis->button_name) - 1); break;
            case 0x08: strncpy(analysis->button_name, "Links / Zurueck", sizeof(analysis->button_name) - 1); break;
            case 0x0B: strncpy(analysis->button_name, "Hoch / Vol +", sizeof(analysis->button_name) - 1); break;
            case 0x0D: strncpy(analysis->button_name, "Runter / Vol -", sizeof(analysis->button_name) - 1); break;
            case 0x5D: strncpy(analysis->button_name, "Mitte / Select", sizeof(analysis->button_name) - 1); break;
            default:
                snprintf(analysis->button_name, sizeof(analysis->button_name), "Apple 0x%02lX", (unsigned long)(command & 0xFF));
                break;
            }
        } else if(addr_low == 0x04) {
            strncpy(analysis->brand, "LG", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "LG Smart TV", sizeof(analysis->device_type) - 1);
        } else if(addr_low == 0x20) {
            strncpy(analysis->brand, "Toshiba", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Toshiba TV", sizeof(analysis->device_type) - 1);
        } else if(addr_low == 0x61) {
            strncpy(analysis->brand, "Sharp", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Sharp Aquos TV", sizeof(analysis->device_type) - 1);
        } else if(addr_low == 0x7E || addr_low == 0x7F) {
            strncpy(analysis->brand, "Yamaha", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Yamaha AV-Receiver", sizeof(analysis->device_type) - 1);
        } else if(addr_low == 0x80) {
            strncpy(analysis->brand, "Pioneer/Onkyo", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "AV-Receiver", sizeof(analysis->device_type) - 1);
        } else if(addr_low == 0xFF || addr_low == 0x00) {
            strncpy(analysis->brand, "RGB LED", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "RGB LED Controller", sizeof(analysis->device_type) - 1);
            switch(command & 0xFF) {
            case 0x02: strncpy(analysis->button_name, "LED Power OFF", sizeof(analysis->button_name) - 1); break;
            case 0x03: strncpy(analysis->button_name, "LED Power ON", sizeof(analysis->button_name) - 1); break;
            case 0x04: strncpy(analysis->button_name, "Farbe Rot", sizeof(analysis->button_name) - 1); break;
            case 0x05: strncpy(analysis->button_name, "Farbe Gruen", sizeof(analysis->button_name) - 1); break;
            case 0x06: strncpy(analysis->button_name, "Farbe Blau", sizeof(analysis->button_name) - 1); break;
            case 0x07: strncpy(analysis->button_name, "Farbe Weiss", sizeof(analysis->button_name) - 1); break;
            case 0x08: strncpy(analysis->button_name, "Helligkeit +", sizeof(analysis->button_name) - 1); break;
            case 0x09: strncpy(analysis->button_name, "Helligkeit -", sizeof(analysis->button_name) - 1); break;
            default:
                snprintf(analysis->button_name, sizeof(analysis->button_name), "LED 0x%02lX", (unsigned long)(command & 0xFF));
                break;
            }
        } else {
            strncpy(analysis->brand, "NEC", sizeof(analysis->brand) - 1);
            snprintf(analysis->device_type, sizeof(analysis->device_type), "NEC Media (0x%02X)", addr_low);
        }

        if(analysis->button_name[0] == '\0') {
            switch(command & 0xFF) {
            case 0x08:
            case 0x12: strncpy(analysis->button_name, "Power (An/Aus)", sizeof(analysis->button_name) - 1); break;
            case 0x02:
            case 0x1A: strncpy(analysis->button_name, "Lautstaerke +", sizeof(analysis->button_name) - 1); break;
            case 0x03:
            case 0x1E: strncpy(analysis->button_name, "Lautstaerke -", sizeof(analysis->button_name) - 1); break;
            case 0x09:
            case 0x14: strncpy(analysis->button_name, "Stumm (Mute)", sizeof(analysis->button_name) - 1); break;
            case 0x00:
            case 0x10: strncpy(analysis->button_name, "Kanal +", sizeof(analysis->button_name) - 1); break;
            case 0x01:
            case 0x11: strncpy(analysis->button_name, "Kanal -", sizeof(analysis->button_name) - 1); break;
            case 0x44: strncpy(analysis->button_name, "OK / Enter", sizeof(analysis->button_name) - 1); break;
            case 0x43: strncpy(analysis->button_name, "Menue", sizeof(analysis->button_name) - 1); break;
            default:
                snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
                break;
            }
        }
    }
    // 6. Kaseikyo (Panasonic)
    else if(protocol == InfraredProtocolKaseikyo) {
        uint8_t vendor = (uint8_t)(address & 0xFF);
        switch(vendor) {
        case 0x01:
        case 0x02:
            strncpy(analysis->brand, "Panasonic", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Panasonic Viera TV", sizeof(analysis->device_type) - 1);
            break;
        case 0x03:
            strncpy(analysis->brand, "JVC", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "JVC AV Geraet", sizeof(analysis->device_type) - 1);
            break;
        case 0x08:
            strncpy(analysis->brand, "Sharp", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Sharp TV/Audio", sizeof(analysis->device_type) - 1);
            break;
        case 0x0A:
            strncpy(analysis->brand, "Denon", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Denon AV-Receiver", sizeof(analysis->device_type) - 1);
            break;
        case 0x24:
            strncpy(analysis->brand, "Mitsubishi", sizeof(analysis->brand) - 1);
            strncpy(analysis->device_type, "Mitsubishi TV/Beamer", sizeof(analysis->device_type) - 1);
            break;
        default:
            strncpy(analysis->brand, "Kaseikyo", sizeof(analysis->brand) - 1);
            snprintf(analysis->device_type, sizeof(analysis->device_type), "Japan AV (0x%02X)", vendor);
            break;
        }

        switch(command & 0x7F) {
        case 0x3D: strncpy(analysis->button_name, "Power (An/Aus)", sizeof(analysis->button_name) - 1); break;
        case 0x20: strncpy(analysis->button_name, "Lautstaerke +", sizeof(analysis->button_name) - 1); break;
        case 0x21: strncpy(analysis->button_name, "Lautstaerke -", sizeof(analysis->button_name) - 1); break;
        case 0x32: strncpy(analysis->button_name, "Stumm (Mute)", sizeof(analysis->button_name) - 1); break;
        case 0x34: strncpy(analysis->button_name, "Kanal +", sizeof(analysis->button_name) - 1); break;
        case 0x35: strncpy(analysis->button_name, "Kanal -", sizeof(analysis->button_name) - 1); break;
        default:
            snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
            break;
        }
    }
    // 7. Pioneer
    else if(protocol == InfraredProtocolPioneer) {
        strncpy(analysis->brand, "Pioneer", sizeof(analysis->brand) - 1);
        strncpy(analysis->device_type, "Pioneer AV-Receiver", sizeof(analysis->device_type) - 1);
        switch(command & 0xFF) {
        case 0x1C: strncpy(analysis->button_name, "Power (An/Aus)", sizeof(analysis->button_name) - 1); break;
        case 0x0A: strncpy(analysis->button_name, "Lautstaerke +", sizeof(analysis->button_name) - 1); break;
        case 0x0B: strncpy(analysis->button_name, "Lautstaerke -", sizeof(analysis->button_name) - 1); break;
        case 0x09: strncpy(analysis->button_name, "Stumm (Mute)", sizeof(analysis->button_name) - 1); break;
        default:
            snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
            break;
        }
    }
    // 8. RCA
    else if(protocol == InfraredProtocolRCA) {
        strncpy(analysis->brand, "RCA", sizeof(analysis->brand) - 1);
        strncpy(analysis->device_type, "RCA/Thomson TV", sizeof(analysis->device_type) - 1);
        switch(command & 0xFF) {
        case 0x08: strncpy(analysis->button_name, "Power (An/Aus)", sizeof(analysis->button_name) - 1); break;
        case 0x00: strncpy(analysis->button_name, "Lautstaerke +", sizeof(analysis->button_name) - 1); break;
        case 0x01: strncpy(analysis->button_name, "Lautstaerke -", sizeof(analysis->button_name) - 1); break;
        default:
            snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
            break;
        }
    }
    // 9. NEC42 / NEC42ext
    else if(protocol == InfraredProtocolNEC42 || protocol == InfraredProtocolNEC42ext) {
        strncpy(analysis->brand, "Panasonic", sizeof(analysis->brand) - 1);
        strncpy(analysis->device_type, "Panasonic AC / TV", sizeof(analysis->device_type) - 1);
        snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
    }
    // Fallback
    else {
        strncpy(analysis->brand, p_name, sizeof(analysis->brand) - 1);
        snprintf(analysis->device_type, sizeof(analysis->device_type), "%s Geraet", p_name);
        snprintf(analysis->button_name, sizeof(analysis->button_name), "Befehl 0x%02lX", (unsigned long)(command & 0xFF));
    }

    const char* short_btn = "Cmd";
    if(strstr(analysis->button_name, "Power")) short_btn = "Power";
    else if(strstr(analysis->button_name, "Lautstaerke +")) short_btn = "VolPlus";
    else if(strstr(analysis->button_name, "Lautstaerke -")) short_btn = "VolMinus";
    else if(strstr(analysis->button_name, "Stumm")) short_btn = "Mute";
    else if(strstr(analysis->button_name, "Kanal +")) short_btn = "ChPlus";
    else if(strstr(analysis->button_name, "Kanal -")) short_btn = "ChMinus";
    else if(strstr(analysis->button_name, "Play")) short_btn = "Play";
    else if(strstr(analysis->button_name, "Stop")) short_btn = "Stop";
    else if(strstr(analysis->button_name, "OK")) short_btn = "OK";
    else if(strstr(analysis->button_name, "Menue")) short_btn = "Menu";
    else if(strstr(analysis->button_name, "Quelle")) short_btn = "Source";
    else if(strstr(analysis->button_name, "Taste")) short_btn = "Key";

    snprintf(analysis->display_name, sizeof(analysis->display_name), "%s_%s_%02lX", 
        analysis->brand[0] ? analysis->brand : p_name, short_btn, (unsigned long)(command & 0xFF));
}
