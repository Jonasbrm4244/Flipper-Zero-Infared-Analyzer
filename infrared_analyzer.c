#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_light.h>
#include <furi_hal_vibro.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <infrared_transmit.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "infrared_devices.h"

#define TAG "InfraredAnalyzer"
#define MAX_SAVED_CAPTURES 16
#define IR_STORAGE_FOLDER "/ext/infrared/analyzer"
#define MAX_RAW_TIMINGS 64

typedef enum {
    AppScreenSplash,
    AppScreenMain,
} AppScreen;

typedef enum {
    AppTabAnalyse = 0,
    AppTabSaved = 1,
} AppTab;

typedef enum {
    SavedSubScreenList,
    SavedSubScreenInfo,
} SavedSubScreen;

typedef enum {
    EventTypeTick,
    EventTypeKey,
    EventTypeIrSignal,
} EventType;

typedef struct {
    char name[24];
    char file_name[32];
    InfraredProtocol protocol_enum;
    char protocol[16];
    char device_desc[28];
    char button_name[24];
    uint32_t address;
    uint32_t command;
    uint32_t frequency;
    uint8_t duty_cycle;
    bool is_raw;
    uint32_t raw_count;
    uint32_t raw_timings[MAX_RAW_TIMINGS];
} SavedCapture;

typedef struct {
    EventType type;
    InputEvent input;
    bool is_decoded;
    InfraredMessage message;
    size_t raw_count;
    uint32_t raw_first_timing;
} AppEvent;

typedef struct {
    AppScreen screen;
    AppTab active_tab;
    SavedSubScreen saved_subscreen;
    uint32_t splash_ticks; // 100ms per tick, 30 ticks = 3.0s
    uint32_t anim_tick;

    // Analyzer state
    bool has_signal;
    bool is_raw;
    InfraredProtocol last_protocol;
    char protocol_name[16];
    uint32_t address;
    uint32_t command;
    bool repeat;
    uint32_t repeat_count;
    uint32_t raw_count;
    IrSignalAnalysis analysis;

    // Saved captures state
    size_t saved_count;
    size_t selected_saved_idx;
    SavedCapture current_cap_display;

    // Feedback
    uint8_t toast_ticks;
    uint8_t delete_toast_ticks;
    uint8_t tx_ticks;
    uint8_t rx_blink_ticks;
} InfraredAnalyzerModel;

typedef struct {
    FuriMutex* mutex;
    InfraredAnalyzerModel model;
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    Storage* storage;
    InfraredWorker* worker;
    bool worker_running;

    SavedCapture saved_captures[MAX_SAVED_CAPTURES];
} InfraredAnalyzerApp;

static void infrared_analyzer_rx_callback(void* context, InfraredWorkerSignal* received_signal) {
    InfraredAnalyzerApp* app = context;
    if(!app || !app->event_queue) return;

    AppEvent event;
    memset(&event, 0, sizeof(AppEvent));
    event.type = EventTypeIrSignal;

    if(infrared_worker_signal_is_decoded(received_signal)) {
        const InfraredMessage* msg = infrared_worker_get_decoded_signal(received_signal);
        if(msg) {
            event.is_decoded = true;
            event.message = *msg;
            furi_message_queue_put(app->event_queue, &event, 0);
        }
    } else {
        const uint32_t* timings = NULL;
        size_t count = 0;
        infrared_worker_get_raw_signal(received_signal, &timings, &count);
        event.is_decoded = false;
        event.raw_count = count;
        if(timings && count > 0) {
            event.raw_first_timing = timings[0];
        }
        furi_message_queue_put(app->event_queue, &event, 0);
    }
}

static void infrared_analyzer_load_saved(InfraredAnalyzerApp* app) {
    if(!app || !app->storage) return;

    File* dir = storage_file_alloc(app->storage);
    if(!dir) return;

    bool opened = storage_dir_open(dir, IR_STORAGE_FOLDER);
    if(opened) {
        FileInfo fileinfo;
        char filename[48];
        while(app->model.saved_count < MAX_SAVED_CAPTURES && 
              storage_dir_read(dir, &fileinfo, filename, sizeof(filename))) {
            if(!file_info_is_dir(&fileinfo) && strstr(filename, ".ir")) {
                char full_path[96];
                snprintf(full_path, sizeof(full_path), "%s/%s", IR_STORAGE_FOLDER, filename);

                FlipperFormat* ff = flipper_format_file_alloc(app->storage);
                if(ff) {
                    if(flipper_format_file_open_existing(ff, full_path)) {
                        FuriString* header_type = furi_string_alloc();
                        uint32_t version = 0;
                        if(flipper_format_read_header(ff, header_type, &version)) {
                            SavedCapture* cap = &app->saved_captures[app->model.saved_count];
                            memset(cap, 0, sizeof(SavedCapture));
                            strncpy(cap->file_name, filename, sizeof(cap->file_name) - 1);

                            FuriString* str_buf = furi_string_alloc();
                            if(flipper_format_read_string(ff, "name", str_buf)) {
                                strncpy(cap->name, furi_string_get_cstr(str_buf), sizeof(cap->name) - 1);
                            }

                            if(flipper_format_read_string(ff, "type", str_buf)) {
                                if(strcmp(furi_string_get_cstr(str_buf), "parsed") == 0) {
                                    cap->is_raw = false;
                                    if(flipper_format_read_string(ff, "protocol", str_buf)) {
                                        strncpy(cap->protocol, furi_string_get_cstr(str_buf), sizeof(cap->protocol) - 1);
                                        cap->protocol_enum = infrared_get_protocol_by_name(cap->protocol);
                                    }
                                    flipper_format_read_hex(ff, "address", (uint8_t*)&cap->address, 4);
                                    flipper_format_read_hex(ff, "command", (uint8_t*)&cap->command, 4);

                                    IrSignalAnalysis analysis;
                                    infrared_analyze_signal(cap->protocol_enum, cap->address, cap->command, &analysis);
                                    strncpy(cap->device_desc, analysis.device_type, sizeof(cap->device_desc) - 1);
                                    strncpy(cap->button_name, analysis.button_name, sizeof(cap->button_name) - 1);
                                    cap->frequency = analysis.frequency;
                                    cap->duty_cycle = analysis.duty_cycle;
                                    app->model.saved_count++;
                                } else if(strcmp(furi_string_get_cstr(str_buf), "raw") == 0) {
                                    cap->is_raw = true;
                                    strncpy(cap->protocol, "RAW", sizeof(cap->protocol) - 1);
                                    strncpy(cap->device_desc, "Raw Signal", sizeof(cap->device_desc) - 1);
                                    strncpy(cap->button_name, "Raw Daten", sizeof(cap->button_name) - 1);
                                    cap->frequency = 38000;
                                    cap->duty_cycle = 33;
                                    app->model.saved_count++;
                                }
                            }
                            furi_string_free(str_buf);
                        }
                        furi_string_free(header_type);
                        flipper_format_file_close(ff);
                    }
                    flipper_format_free(ff);
                }
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

static bool infrared_analyzer_save_current_capture(InfraredAnalyzerApp* app) {
    if(!app->model.has_signal || app->model.saved_count >= MAX_SAVED_CAPTURES) return false;

    char filename[48];
    snprintf(filename, sizeof(filename), "%s_%lu.ir", 
        app->model.analysis.display_name[0] ? app->model.analysis.display_name : "Capture",
        (unsigned long)(furi_get_tick() % 1000));

    char full_path[96];
    snprintf(full_path, sizeof(full_path), "%s/%s", IR_STORAGE_FOLDER, filename);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    bool saved = false;
    if(ff) {
        if(flipper_format_file_open_new(ff, full_path)) {
            if(flipper_format_write_header_cstr(ff, "Flipper IR file", 1)) {
                flipper_format_write_comment_cstr(ff, "Captured by Infrared Analyzer - made by Jonas.brm");
                flipper_format_write_string_cstr(ff, "name", app->model.analysis.display_name);

                if(!app->model.is_raw) {
                    flipper_format_write_string_cstr(ff, "type", "parsed");
                    flipper_format_write_string_cstr(ff, "protocol", app->model.protocol_name);
                    flipper_format_write_hex(ff, "address", (const uint8_t*)&app->model.address, 4);
                    flipper_format_write_hex(ff, "command", (const uint8_t*)&app->model.command, 4);
                    saved = true;
                } else {
                    flipper_format_write_string_cstr(ff, "type", "raw");
                    uint32_t freq = 38000;
                    float duty = 0.33f;
                    flipper_format_write_uint32(ff, "frequency", &freq, 1);
                    flipper_format_write_float(ff, "duty_cycle", &duty, 1);
                    saved = true;
                }
            }
            flipper_format_file_close(ff);
        }
        flipper_format_free(ff);
    }

    if(saved) {
        SavedCapture* cap = &app->saved_captures[app->model.saved_count++];
        memset(cap, 0, sizeof(SavedCapture));
        strncpy(cap->file_name, filename, sizeof(cap->file_name) - 1);
        strncpy(cap->name, app->model.analysis.display_name, sizeof(cap->name) - 1);
        cap->is_raw = app->model.is_raw;
        cap->protocol_enum = app->model.last_protocol;
        cap->address = app->model.address;
        cap->command = app->model.command;
        cap->frequency = app->model.analysis.frequency;
        cap->duty_cycle = app->model.analysis.duty_cycle;
        strncpy(cap->protocol, app->model.protocol_name, sizeof(cap->protocol) - 1);
        strncpy(cap->device_desc, app->model.analysis.device_type, sizeof(cap->device_desc) - 1);
        strncpy(cap->button_name, app->model.analysis.button_name, sizeof(cap->button_name) - 1);
    }

    return saved;
}

static void infrared_analyzer_delete_selected_capture(InfraredAnalyzerApp* app) {
    if(app->model.saved_count == 0 || app->model.selected_saved_idx >= app->model.saved_count) return;

    size_t idx = app->model.selected_saved_idx;
    SavedCapture* cap = &app->saved_captures[idx];

    if(cap->file_name[0] != '\0') {
        char full_path[96];
        snprintf(full_path, sizeof(full_path), "%s/%s", IR_STORAGE_FOLDER, cap->file_name);
        storage_simply_remove(app->storage, full_path);
    }

    for(size_t i = idx; i < app->model.saved_count - 1; i++) {
        app->saved_captures[i] = app->saved_captures[i + 1];
    }
    app->model.saved_count--;
    if(app->model.selected_saved_idx >= app->model.saved_count && app->model.saved_count > 0) {
        app->model.selected_saved_idx = app->model.saved_count - 1;
    }
}

static void infrared_analyzer_transmit_selected(InfraredAnalyzerApp* app) {
    if(app->model.saved_count == 0 || app->model.selected_saved_idx >= app->model.saved_count) return;

    SavedCapture* cap = &app->saved_captures[app->model.selected_saved_idx];

    if(app->worker_running) {
        infrared_worker_rx_stop(app->worker);
        app->worker_running = false;
    }

    app->model.tx_ticks = 4;
    furi_hal_light_set(LightRed, 255);
    furi_hal_vibro_on(true);

    if(!cap->is_raw && infrared_is_protocol_valid(cap->protocol_enum)) {
        InfraredMessage msg = {
            .protocol = cap->protocol_enum,
            .address = cap->address,
            .command = cap->command,
            .repeat = false,
        };
        infrared_send(&msg, 2);
    }

    furi_delay_ms(35);
    furi_hal_vibro_on(false);
}

static void draw_splash_screen(Canvas* canvas, const InfraredAnalyzerModel* model) {
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 3);
    canvas_draw_rframe(canvas, 2, 2, 124, 60, 2);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "INFRARED ANALYZER");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "made by Jonas.brm");

    // Remote control icon
    canvas_draw_rframe(canvas, 16, 29, 12, 17, 2);
    canvas_draw_box(canvas, 20, 27, 4, 2);
    canvas_draw_dot(canvas, 19, 32);
    canvas_draw_dot(canvas, 24, 32);
    canvas_draw_dot(canvas, 19, 36);
    canvas_draw_dot(canvas, 24, 36);
    canvas_draw_dot(canvas, 21, 41);

    // Animated radiating IR waves
    uint32_t wave = (model->splash_ticks / 3) % 4;
    if(wave >= 1) canvas_draw_circle(canvas, 22, 27, 4);
    if(wave >= 2) canvas_draw_circle(canvas, 22, 27, 7);
    if(wave >= 3) canvas_draw_circle(canvas, 22, 27, 10);

    const char* status = "Lade Decoder...";
    if(model->splash_ticks > 10 && model->splash_ticks <= 20) {
        status = "Scanne Sensoren...";
    } else if(model->splash_ticks > 20) {
        status = "Bereit fuer Analyse!";
    }
    canvas_draw_str(canvas, 42, 38, status);

    // Progress bar frame
    canvas_draw_rframe(canvas, 14, 49, 100, 7, 2);
    uint32_t progress = (model->splash_ticks * 96) / 30;
    if(progress > 96) progress = 96;
    if(progress > 0) {
        canvas_draw_box(canvas, 16, 51, progress, 3);
    }
}

static void draw_tab_bar(Canvas* canvas, AppTab active_tab) {
    if(active_tab == AppTabAnalyse) {
        canvas_draw_rbox(canvas, 1, 0, 56, 12, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 29, 6, AlignCenter, AlignCenter, "Analyse");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, 1, 0, 56, 12, 2);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 29, 6, AlignCenter, AlignCenter, "Analyse");
    }

    if(active_tab == AppTabSaved) {
        canvas_draw_rbox(canvas, 59, 0, 68, 12, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 93, 6, AlignCenter, AlignCenter, "Saved Captures");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, 59, 0, 68, 12, 2);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 93, 6, AlignCenter, AlignCenter, "Saved Captures");
    }

    canvas_draw_line(canvas, 0, 13, 127, 13);
}

static void draw_analyse_tab(Canvas* canvas, const InfraredAnalyzerModel* model) {
    if(!model->has_signal) {
        // Waiting state
        canvas_draw_rframe(canvas, 14, 23, 12, 17, 2);
        canvas_draw_box(canvas, 18, 21, 4, 2);
        canvas_draw_dot(canvas, 17, 26);
        canvas_draw_dot(canvas, 22, 26);
        canvas_draw_dot(canvas, 19, 32);

        uint32_t wave = (model->anim_tick / 2) % 4;
        if(wave >= 1) canvas_draw_circle(canvas, 20, 21, 4);
        if(wave >= 2) canvas_draw_circle(canvas, 20, 21, 7);
        if(wave >= 3) canvas_draw_circle(canvas, 20, 21, 10);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 38, 25, "Warte auf IR...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 38, 35, "Remote auf Flipper");
        canvas_draw_str(canvas, 38, 45, "richten & druecken");

        // Bottom prompt: Left exit, Right switch to Saved Captures
        canvas_draw_rframe(canvas, 1, 52, 54, 11, 2);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 28, 58, AlignCenter, AlignCenter, "Back Exit");

        canvas_draw_rframe(canvas, 59, 52, 68, 11, 2);
        canvas_draw_str_aligned(canvas, 93, 58, AlignCenter, AlignCenter, "Saved ►");
    } else {
        // Signal captured! Clean, structured presentation
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 23, model->analysis.device_type);

        canvas_set_font(canvas, FontSecondary);
        char line2[64];
        snprintf(line2, sizeof(line2), "Taste: %s", model->analysis.button_name);
        canvas_draw_str(canvas, 2, 33, line2);

        char line3[64];
        if(!model->is_raw) {
            snprintf(line3, sizeof(line3), "Addr: 0x%02lX (%lu)  Cmd: 0x%02lX", 
                (unsigned long)model->address, (unsigned long)model->address, 
                (unsigned long)model->command);
        } else {
            snprintf(line3, sizeof(line3), "Raw Signallaenge: %lu Timings", 
                (unsigned long)model->raw_count);
        }
        canvas_draw_str(canvas, 2, 42, line3);

        char line4[64];
        snprintf(line4, sizeof(line4), "%s | %lukHz | Rep: %s", 
            model->protocol_name, 
            (unsigned long)(model->analysis.frequency / 1000), 
            model->repeat ? "Ja" : "Nein");
        canvas_draw_str(canvas, 2, 50, line4);

        // 2 Buttons at bottom requested by user:
        // Left: Löschen (◄ Löschen)
        // Right: Speichern (Speichern ►)
        canvas_draw_rframe(canvas, 1, 52, 58, 11, 2);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 30, 58, AlignCenter, AlignCenter, "◄ Loeschen");

        canvas_draw_rbox(canvas, 66, 52, 61, 11, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 96, 58, AlignCenter, AlignCenter, "Speichern ►");
        canvas_set_color(canvas, ColorBlack);
    }

    if(model->toast_ticks > 0) {
        canvas_draw_rbox(canvas, 8, 18, 112, 30, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignCenter, "Signal Gespeichert!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "In 'Saved Captures' abgelegt");
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_saved_tab(Canvas* canvas, const InfraredAnalyzerModel* model) {
    if(model->saved_count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, "Keine Captures");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Im 'Analyse'-Tab Signal fangen");
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "und mit [Speichern ►] ablegen!");

        canvas_draw_rframe(canvas, 1, 52, 126, 11, 2);
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "◄ Zurueck zu Analyse");
        return;
    }

    if(model->saved_subscreen == SavedSubScreenInfo) {
        // Detailed Information Subscreen
        canvas_draw_rframe(canvas, 1, 14, 126, 37, 2);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 5, 24, model->current_cap_display.device_desc);

        canvas_set_font(canvas, FontSecondary);
        char l1[64];
        snprintf(l1, sizeof(l1), "Taste: %s", model->current_cap_display.button_name);
        canvas_draw_str(canvas, 5, 33, l1);

        char l2[64];
        snprintf(l2, sizeof(l2), "Proto: %s (%lukHz)", 
            model->current_cap_display.protocol,
            (unsigned long)(model->current_cap_display.frequency / 1000));
        canvas_draw_str(canvas, 5, 41, l2);

        char l3[64];
        snprintf(l3, sizeof(l3), "Addr: 0x%02lX | Cmd: 0x%02lX", 
            (unsigned long)model->current_cap_display.address,
            (unsigned long)model->current_cap_display.command);
        canvas_draw_str(canvas, 5, 49, l3);

        // Bottom bar in Info subscreen:
        // Left: ◄ Zurück, Right: OK: Senden
        canvas_draw_rframe(canvas, 1, 52, 54, 11, 2);
        canvas_draw_str_aligned(canvas, 28, 58, AlignCenter, AlignCenter, "◄ Zurueck");

        canvas_draw_rbox(canvas, 58, 52, 69, 11, 2);
        canvas_set_color(canvas, ColorWhite);
        if(model->tx_ticks > 0) {
            canvas_draw_str_aligned(canvas, 92, 58, AlignCenter, AlignCenter, ">>> Sende >>>");
        } else {
            canvas_draw_str_aligned(canvas, 92, 58, AlignCenter, AlignCenter, "OK: Senden");
        }
        canvas_set_color(canvas, ColorBlack);
        return;
    }

    // List view of Saved Captures
    char header[32];
    snprintf(header, sizeof(header), "Capture %u von %u", 
        (unsigned int)(model->selected_saved_idx + 1), 
        (unsigned int)model->saved_count);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 22, header);
    canvas_draw_str_aligned(canvas, 125, 22, AlignRight, AlignBottom, "▲ / ▼");

    // Selected capture card
    canvas_draw_rframe(canvas, 1, 24, 126, 27, 2);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 34, model->current_cap_display.device_desc);

    canvas_set_font(canvas, FontSecondary);
    char line2[64];
    if(!model->current_cap_display.is_raw) {
        snprintf(line2, sizeof(line2), "%s (0x%02lX/0x%02lX)", 
            model->current_cap_display.button_name, 
            (unsigned long)model->current_cap_display.address, 
            (unsigned long)model->current_cap_display.command);
    } else {
        snprintf(line2, sizeof(line2), "%s (%lu Timings)", 
            model->current_cap_display.button_name, 
            (unsigned long)model->current_cap_display.raw_count);
    }
    canvas_draw_str(canvas, 5, 45, line2);

    // 3 Buttons at bottom requested by user:
    // Links: Löschen (◄ Lösch)
    // Mitte: Senden (OK: Senden)
    // Rechts: Informationen (Info ►)
    canvas_draw_rframe(canvas, 1, 52, 36, 11, 2);
    canvas_draw_str_aligned(canvas, 19, 58, AlignCenter, AlignCenter, "◄ Loesch");

    canvas_draw_rbox(canvas, 39, 52, 50, 11, 2);
    canvas_set_color(canvas, ColorWhite);
    if(model->tx_ticks > 0) {
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, ">>> Sende >>>");
    } else {
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "OK: Senden");
    }
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_rframe(canvas, 91, 52, 36, 11, 2);
    canvas_draw_str_aligned(canvas, 109, 58, AlignCenter, AlignCenter, "Info ►");

    if(model->delete_toast_ticks > 0) {
        canvas_draw_rbox(canvas, 18, 22, 92, 22, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignCenter, "Geloescht!");
        canvas_set_color(canvas, ColorBlack);
    }
}

static void infrared_analyzer_render_callback(Canvas* canvas, void* context) {
    InfraredAnalyzerApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);

    if(app->model.screen == AppScreenSplash) {
        draw_splash_screen(canvas, &app->model);
    } else {
        draw_tab_bar(canvas, app->model.active_tab);
        if(app->model.active_tab == AppTabAnalyse) {
            draw_analyse_tab(canvas, &app->model);
        } else {
            draw_saved_tab(canvas, &app->model);
        }
    }

    furi_mutex_release(app->mutex);
}

static void infrared_analyzer_input_callback(InputEvent* input_event, void* context) {
    InfraredAnalyzerApp* app = context;
    AppEvent event;
    memset(&event, 0, sizeof(AppEvent));
    event.type = EventTypeKey;
    event.input = *input_event;
    furi_message_queue_put(app->event_queue, &event, 0);
}

static void infrared_analyzer_timer_callback(void* context) {
    InfraredAnalyzerApp* app = context;
    AppEvent event;
    memset(&event, 0, sizeof(AppEvent));
    event.type = EventTypeTick;
    furi_message_queue_put(app->event_queue, &event, 0);
}

int32_t infrared_analyzer_app(void* p) {
    UNUSED(p);

    InfraredAnalyzerApp* app = malloc(sizeof(InfraredAnalyzerApp));
    furi_check(app);
    memset(app, 0, sizeof(InfraredAnalyzerApp));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_check(app->mutex);

    app->event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    furi_check(app->event_queue);

    app->storage = furi_record_open(RECORD_STORAGE);

    // Initial model state
    app->model.screen = AppScreenSplash;
    app->model.active_tab = AppTabAnalyse;
    app->model.saved_subscreen = SavedSubScreenList;
    app->model.splash_ticks = 0;
    app->model.saved_count = 0;
    app->model.selected_saved_idx = 0;

    // Create folders and load existing captures if storage is available
    if(app->storage) {
        storage_simply_mkdir(app->storage, "/ext/infrared");
        storage_simply_mkdir(app->storage, IR_STORAGE_FOLDER);
        infrared_analyzer_load_saved(app);
        if(app->model.saved_count > 0) {
            app->model.current_cap_display = app->saved_captures[0];
        }
    }

    // Allocate InfraredWorker for receiving signals
    app->worker = infrared_worker_alloc();
    furi_check(app->worker);
    infrared_worker_rx_enable_blink_on_receiving(app->worker, true);
    infrared_worker_rx_enable_signal_decoding(app->worker, true);
    infrared_worker_rx_set_received_signal_callback(
        app->worker, infrared_analyzer_rx_callback, app);

    // View port & GUI
    app->view_port = view_port_alloc();
    furi_check(app->view_port);
    view_port_draw_callback_set(app->view_port, infrared_analyzer_render_callback, app);
    view_port_input_callback_set(app->view_port, infrared_analyzer_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    furi_check(app->gui);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // 100ms periodic timer
    FuriTimer* timer = furi_timer_alloc(
        infrared_analyzer_timer_callback, FuriTimerTypePeriodic, app);
    furi_check(timer);
    furi_timer_start(timer, furi_ms_to_ticks(100));

    bool running = true;
    AppEvent event;

    while(running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, FuriWaitForever);
        if(status != FuriStatusOk) continue;

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if(event.type == EventTypeTick) {
            app->model.anim_tick++;

            if(app->model.screen == AppScreenSplash) {
                app->model.splash_ticks++;
                if(app->model.splash_ticks >= 30) { // 3.0 seconds
                    app->model.screen = AppScreenMain;
                    // Start IR receiver!
                    infrared_worker_rx_start(app->worker);
                    app->worker_running = true;
                    furi_hal_vibro_on(true);
                    furi_delay_ms(30);
                    furi_hal_vibro_on(false);
                }
            } else {
                if(app->model.toast_ticks > 0) {
                    app->model.toast_ticks--;
                }
                if(app->model.delete_toast_ticks > 0) {
                    app->model.delete_toast_ticks--;
                }
                if(app->model.tx_ticks > 0) {
                    app->model.tx_ticks--;
                    if(app->model.tx_ticks == 0) {
                        furi_hal_light_set(LightRed, 0);
                        if(app->model.active_tab == AppTabAnalyse && !app->worker_running) {
                            infrared_worker_rx_start(app->worker);
                            app->worker_running = true;
                        }
                    }
                }
                if(app->model.rx_blink_ticks > 0) {
                    app->model.rx_blink_ticks--;
                    if(app->model.rx_blink_ticks == 0) {
                        furi_hal_light_set(LightGreen, 0);
                        furi_hal_light_set(LightBlue, 0);
                    }
                }
            }
        } else if(event.type == EventTypeIrSignal) {
            if(app->model.screen == AppScreenMain && app->model.active_tab == AppTabAnalyse) {
                if(event.is_decoded && infrared_is_protocol_valid(event.message.protocol)) {
                    app->model.has_signal = true;
                    app->model.is_raw = false;
                    app->model.last_protocol = event.message.protocol;
                    const char* pname = infrared_get_protocol_name(event.message.protocol);
                    strncpy(app->model.protocol_name, pname ? pname : "Unbekannt", sizeof(app->model.protocol_name) - 1);
                    app->model.address = event.message.address;
                    app->model.command = event.message.command;
                    app->model.repeat = event.message.repeat;
                    if(event.message.repeat) {
                        app->model.repeat_count++;
                    } else {
                        app->model.repeat_count = 0;
                    }

                    infrared_analyze_signal(
                        event.message.protocol, 
                        event.message.address, 
                        event.message.command, 
                        &app->model.analysis);

                    // Blink blue LED to show reception
                    app->model.rx_blink_ticks = 2;
                    furi_hal_light_set(LightBlue, 255);
                } else if(!event.is_decoded && event.raw_count > 0) {
                    app->model.has_signal = true;
                    app->model.is_raw = true;
                    app->model.last_protocol = InfraredProtocolUnknown;
                    strncpy(app->model.protocol_name, "RAW Signal", sizeof(app->model.protocol_name) - 1);
                    app->model.address = 0;
                    app->model.command = 0;
                    app->model.repeat = false;
                    app->model.raw_count = event.raw_count;

                    strncpy(app->model.analysis.device_type, "Raw Infrarot", sizeof(app->model.analysis.device_type) - 1);
                    strncpy(app->model.analysis.button_name, "Unbekanntes Signal", sizeof(app->model.analysis.button_name) - 1);
                    snprintf(app->model.analysis.display_name, sizeof(app->model.analysis.display_name), "Raw_%lu_Samples", (unsigned long)event.raw_count);
                    app->model.analysis.frequency = 38000;
                    app->model.analysis.duty_cycle = 33;

                    app->model.rx_blink_ticks = 2;
                    furi_hal_light_set(LightBlue, 255);
                }
            }
        } else if(event.type == EventTypeKey) {
            if(app->model.screen == AppScreenSplash) {
                if(event.input.type == InputTypeShort) {
                    if(event.input.key == InputKeyBack) {
                        running = false;
                    } else if(event.input.key == InputKeyOk) {
                        // Skip splash
                        app->model.screen = AppScreenMain;
                        infrared_worker_rx_start(app->worker);
                        app->worker_running = true;
                    }
                }
            } else {
                // Main screen navigation
                if(event.input.type == InputTypeShort) {
                    // TAB 1: ANALYSE
                    if(app->model.active_tab == AppTabAnalyse) {
                        if(app->model.has_signal) {
                            // When signal is captured:
                            // Links [◄]: Löschen / Reset
                            // Rechts [►]: Speichern
                            if(event.input.key == InputKeyLeft || event.input.key == InputKeyBack) {
                                // Löschen / Zurücksetzen
                                app->model.has_signal = false;
                                furi_hal_vibro_on(true);
                                furi_delay_ms(25);
                                furi_hal_vibro_on(false);
                            } else if(event.input.key == InputKeyRight || event.input.key == InputKeyOk) {
                                // Speichern
                                if(infrared_analyzer_save_current_capture(app)) {
                                    app->model.toast_ticks = 15;
                                    furi_hal_light_set(LightGreen, 255);
                                    app->model.rx_blink_ticks = 3;
                                    furi_hal_vibro_on(true);
                                    furi_delay_ms(40);
                                    furi_hal_vibro_on(false);
                                }
                            }
                        } else {
                            // Waiting for signal:
                            if(event.input.key == InputKeyRight) {
                                // Switch to Saved Captures tab
                                app->model.active_tab = AppTabSaved;
                                app->model.saved_subscreen = SavedSubScreenList;
                                if(app->worker_running) {
                                    infrared_worker_rx_stop(app->worker);
                                    app->worker_running = false;
                                }
                                if(app->model.saved_count > 0 && app->model.selected_saved_idx < app->model.saved_count) {
                                    app->model.current_cap_display = app->saved_captures[app->model.selected_saved_idx];
                                }
                            } else if(event.input.key == InputKeyBack) {
                                running = false;
                            }
                        }
                    }
                    // TAB 2: SAVED CAPTURES
                    else if(app->model.active_tab == AppTabSaved) {
                        if(app->model.saved_subscreen == SavedSubScreenInfo) {
                            // In Info view:
                            if(event.input.key == InputKeyLeft || event.input.key == InputKeyBack) {
                                // Return to List view
                                app->model.saved_subscreen = SavedSubScreenList;
                            } else if(event.input.key == InputKeyOk) {
                                // Transmit signal
                                infrared_analyzer_transmit_selected(app);
                            }
                        } else {
                            // In List view:
                            // Oben [▲] & Unten [▼]: Scrollen
                            // [OK]: Senden
                            // Rechts [►]: Informationen
                            // Links [◄]: Löschen
                            if(event.input.key == InputKeyUp) {
                                if(app->model.saved_count > 0) {
                                    if(app->model.selected_saved_idx > 0) {
                                        app->model.selected_saved_idx--;
                                    } else {
                                        app->model.selected_saved_idx = app->model.saved_count - 1;
                                    }
                                    app->model.current_cap_display = app->saved_captures[app->model.selected_saved_idx];
                                }
                            } else if(event.input.key == InputKeyDown) {
                                if(app->model.saved_count > 0) {
                                    if(app->model.selected_saved_idx + 1 < app->model.saved_count) {
                                        app->model.selected_saved_idx++;
                                    } else {
                                        app->model.selected_saved_idx = 0;
                                    }
                                    app->model.current_cap_display = app->saved_captures[app->model.selected_saved_idx];
                                }
                            } else if(event.input.key == InputKeyOk) {
                                // Senden
                                infrared_analyzer_transmit_selected(app);
                            } else if(event.input.key == InputKeyRight) {
                                // Informationen öffnen
                                if(app->model.saved_count > 0) {
                                    app->model.saved_subscreen = SavedSubScreenInfo;
                                }
                            } else if(event.input.key == InputKeyLeft) {
                                // Löschen
                                if(app->model.saved_count > 0) {
                                    infrared_analyzer_delete_selected_capture(app);
                                    app->model.delete_toast_ticks = 12;
                                    if(app->model.saved_count > 0) {
                                        app->model.current_cap_display = app->saved_captures[app->model.selected_saved_idx];
                                    }
                                    furi_hal_vibro_on(true);
                                    furi_delay_ms(40);
                                    furi_hal_vibro_on(false);
                                } else {
                                    // Switch back to Analyse tab
                                    app->model.active_tab = AppTabAnalyse;
                                    if(!app->worker_running) {
                                        infrared_worker_rx_start(app->worker);
                                        app->worker_running = true;
                                    }
                                }
                            } else if(event.input.key == InputKeyBack) {
                                // Return to Analyse tab
                                app->model.active_tab = AppTabAnalyse;
                                if(!app->worker_running) {
                                    infrared_worker_rx_start(app->worker);
                                    app->worker_running = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }

    // Cleanup
    furi_timer_stop(timer);
    furi_timer_free(timer);

    if(app->worker_running) {
        infrared_worker_rx_stop(app->worker);
    }
    infrared_worker_free(app->worker);

    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);
    furi_hal_vibro_on(false);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
    }
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
