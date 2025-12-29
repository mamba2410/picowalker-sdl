#include <stdio.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#ifdef WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#else
#include <unistd.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "picowalker-defs.h"
#include "picowalker-text.h"

extern void pw_button_callback(uint8_t b);
extern void walker_setup();
extern void (*current_loop)();

#define PW_SCALE 10

SDL_Window *window;
SDL_Renderer *renderer;

SDL_AudioDeviceID audio_dev;

uint32_t uncounted_steps = 0;

uint32_t colours[] = {
    /* white */ 0xacaea4,
    /* lgrey */ 0x7c7e74,
    /* dgrey */ 0x5c5e54,
    /* black */ 0x2c2e24
};

void check_event() {
    SDL_Event event;

    while(SDL_PollEvent(&event)) {
        switch(event.type) {
        case SDL_QUIT: {
            exit(0);
            break;
        }
        case SDL_KEYDOWN: {
            switch(event.key.keysym.sym) {
            case SDLK_LEFT: {
                pw_button_callback(BUTTON_L);
                break;
            }
            case SDLK_UP: {
                pw_button_callback(BUTTON_M);
                break;
            }
            case SDLK_RIGHT: {
                pw_button_callback(BUTTON_R);
                break;
            }
            case SDLK_1: {
                uncounted_steps += 1;
                break;
            }
            case SDLK_2: {
                uncounted_steps += 10;
                break;
            }
            case SDLK_3: {
                uncounted_steps += 100;
                break;
            }
            case SDLK_4: {
                uncounted_steps += 1000;
                break;
            }
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }
}


/*
 * ==== REQUIRED FUNCTIONS
 *
 * ==== EEPROM
 * void pw_eeprom_init()
 * int pw_eeprom_read(eeprom_addr_t addr, uint8_t *buf, size_t sz)
 * int pw_eeprom_write(eeprom_addr_t addr, uint8_t *buf, size_t sz)
 * void pw_eeprom_set_area(eeprom_addr_t addr, uint8_t v, size_t sz)
 *
 * ==== SCREEN
 * void pw_screen_init()
 * void pw_screen_draw_img(pw_img_t *img, screen_pos_t x, screen_pos_t y)
 * void pw_screen_clear()
 * void pw_screen_fill_area()
 * void pw_screen_clear_area()
 * void pw_screen_draw_horiz_line()
 * void pw_screen_draw_text_box()
 *
 * ==== AUDIO
 * void pw_audio_init()
 * void pw_audio_play_sound(pw_sound_frame_t* sound_data, size_t sz)
 *
 * ==== FLASH
 * void pw_flash_read(pw_flash_img_t img_index, uint8_t *buf)
 *
 * ==== BUTTONS - IRQ
 * void pw_button_init()
 *
 * ==== IR
 * void pw_ir_init()
 * intpw_ir_read(uint8_t *buf, size_t len)
 * intpw_ir_write(uint8_t *buf, size_t len)
 *
 * ==== TIMER
 * void pw_timer_delay_ms(uint64_t ms)
 * uint64_t pw_now_us()
 *
 * ==== ACCEL
 * int8_t pw_accel_init()
 * uint32_t pw_accel_get_new_steps()
 */

void pw_button_init() {}
void pw_ir_init() {}
int pw_ir_read(uint8_t *buf, size_t len) {
    return 0;
}
int pw_ir_write(uint8_t *buf, size_t len) {
    return 0;
}

uint32_t pw_time_get_us() {
    struct timespec ts;
    (void)timespec_get(&ts, TIME_UTC);
    uint64_t us = ts.tv_sec*1000000 + ts.tv_nsec/1000;
    return (uint32_t)us;
}

uint32_t pw_time_get_ms() {
    struct timespec ts;
    (void)timespec_get(&ts, TIME_UTC);
    uint64_t ms = ts.tv_sec*1000 + ts.tv_nsec/1000000;
    return (uint32_t)ms;
}

pw_dhms_t pw_time_get_dhms() {
    struct timespec ts;
    (void)timespec_get(&ts, TIME_UTC);
    pw_dhms_t dhms = {0};
    dhms.seconds = ts.tv_sec % 60;
    ts.tv_sec /= 60;
    dhms.minutes = ts.tv_sec % 60;
    ts.tv_sec /= 60;
    dhms.hours = ts.tv_sec % 24;
    dhms.days = ts.tv_sec / 24;
    return dhms;
}

void pw_time_init_rtc(uint32_t rtc) {
    // pass
}

void pw_time_set_rtc(uint32_t rtc) { }

uint32_t pw_time_get_rtc() {
    struct timespec ts;
    (void)timespec_get(&ts, TIME_UTC);
    // Turn unix seconds to 1st jan 2000 seconds
    uint64_t seconds = ts.tv_sec - 946684800;
    return (uint32_t)seconds;

}

void pw_time_delay_ms(uint32_t ms) {
    usleep(ms*1000);
}

static uint32_t prev_time = 0;
pw_rtc_events_t pw_rtc_get_events() {
    pw_rtc_events_t events = 0;

    uint32_t time = pw_time_get_rtc();
    if(time != prev_time) {
        if(time%60 == 0) events |= RTC_EVENT_EVERY_MINUTE;
        if(time%3600 == 0) events |= RTC_EVENT_EVERY_HOUR;
        if(time%(24*3600) == 0) events |= RTC_EVENT_EVERY_DAY;
    }
    prev_time = time;
    return events;
}
void pw_accel_init() {}
uint32_t pw_accel_get_new_steps() {
    uint32_t tmp = uncounted_steps;
    uncounted_steps = 0;
    return tmp;
}
uint8_t *sad_pokewalker = 0;

void walker_entry();

static uint8_t *eeprom;

void pw_eeprom_init() {

    const char fname[] = "eeprom.bin";

    struct stat s;
    FILE *f = fopen(fname, "rb");
    if(!f) {
        fprintf(stderr, "Could not get file pointer for eeprom file %s\n", fname);
        exit(EXIT_FAILURE);
    }

    if(stat(fname, &s) == -1) {
        fprintf(stderr, "Could not stat file: %s\n", fname);
        exit(EXIT_FAILURE);
    }

    if(s.st_size != 64*1024) {
        fprintf(stderr, "eeprom file %s not the right size\n", fname);
        exit(EXIT_FAILURE);
    }

    eeprom = malloc(64*1024);
    if(!eeprom) {
        fprintf(stderr, "Could not alloc enough space when reading eeprom file %s\n", fname);
        exit(EXIT_FAILURE);
    }

    fread(eeprom, s.st_size, 1, f);

    fclose(f);
}


int pw_eeprom_read(eeprom_addr_t addr, uint8_t *buf, size_t len) {
    void *ret = memcpy(buf, &eeprom[addr], len);
    if(!ret) return -1;
    else return 0;
}

int pw_eeprom_write(eeprom_addr_t addr, uint8_t *buf, size_t len) {
    void *ret = memcpy(&eeprom[addr], buf, len);
    FILE *f = fopen("eeprom.bin", "wb");
    fwrite(eeprom, 64*1024, 1, f);
    fclose(f);
    if(!ret) return -1;
    else return 0;
}

void pw_eeprom_set_area(eeprom_addr_t addr, uint8_t v, size_t len) {
    memset(&eeprom[addr], v, len);
}


void pw_screen_init() {
    window = SDL_CreateWindow(
                 "picowalker-sdl",
                 SDL_WINDOWPOS_UNDEFINED,
                 SDL_WINDOWPOS_UNDEFINED,
                 96*PW_SCALE,
                 64*PW_SCALE,
                 0
             );
    if(!window) {
        fprintf(stderr, "Failed to open window: %s\n", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if(!window) {
        fprintf(stderr, "Failed to open renderer: %s\n", SDL_GetError());
    }
    SDL_RenderSetScale(renderer, 10.0, 10.0);
}

void pw_screen_clear() {
    // SDL render refresh or something
    SDL_SetRenderDrawColor(renderer, (colours[0]>>16)&0xff, (colours[0]>>8)&0xff, (colours[0])&0xff, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
}

void pw_screen_fill_area(screen_pos_t x, screen_pos_t y, screen_pos_t width, screen_pos_t height, screen_colour_t colour) {
    // idk draw a filled rectangle or something.
    SDL_Rect box = {.x=x, .y=y, .w=width, .h=height };
    SDL_SetRenderDrawColor(renderer, (colours[colour]>>16)&0xff, (colours[colour]>>8)&0xff, (colours[colour])&0xff, SDL_ALPHA_OPAQUE);
    SDL_RenderFillRect(renderer, &box);
    //SDL_RenderPresent(renderer);
}

void pw_screen_clear_area(screen_pos_t x, screen_pos_t y, screen_pos_t width, screen_pos_t height) {
    pw_screen_fill_area(x, y, width, height, 0);
}

void pw_screen_draw_text_box(screen_pos_t x1, screen_pos_t y1, screen_pos_t width, screen_pos_t height, screen_colour_t colour) {
    // draw rectangle, need to make it thicker but good start
    SDL_Rect box = {.x=x1, .y=y1, .w=width, .h=height };
    SDL_SetRenderDrawColor(renderer, 0x2c, 0x2e, 0x24, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawRect(renderer, &box);
    //SDL_RenderPresent(renderer);
}

void pw_screen_draw_horiz_line(screen_pos_t x, screen_pos_t y, screen_pos_t len, screen_colour_t colour) {
    SDL_SetRenderDrawColor(renderer, (colours[colour]>>16)&0xff, (colours[colour]>>8)&0xff, (colours[colour])&0xff, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawLine(renderer, x, y, (x+len), y);
    //SDL_RenderPresent(renderer);
}

void pw_screen_set_brightness(uint8_t level) { }

/*
 * Convert 2bpp Pokewalker image format to 4bpp OLED format
 * Need on-the-fly decoding because game sends images
 * Adapted from pw_lcd repo
 */
void decode_img(pw_img_t *pw_img, SDL_Texture **tex) {
    uint8_t pixel_value, bit_plane_upper, bit_plane_lower;
    size_t row, col;

    pw_img->size = pw_img->width * pw_img->height * 2/8; // 2 bytes = 8 pixels

    //*tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, pw_img->width, pw_img->height);
    *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, pw_img->width, pw_img->height);

    uint8_t *pixels;
    int pitch;  // aka stride
    int ret = SDL_LockTexture(*tex, NULL, (void**)(&pixels), &pitch); // NULL = lock everything
    if(ret) {
        fprintf(stderr, "SDL can't lock texture\n");
        exit(EXIT_FAILURE);
    }

    uint8_t *decode_buf = malloc(pw_img->width*pw_img->height);
    uint8_t *sdl_buf = pixels; // this is the texture pixel buf


    // i = number of bytes into pw_img
    for(size_t i = 0; i < pw_img->size; i += 2) {
        bit_plane_upper = pw_img->data[i];
        bit_plane_lower = pw_img->data[i+1];

        // j = index of pixel in chunk
        for(size_t j = 0; j < 8; j++) {
            // PW raw pixel value
            pixel_value  = ((bit_plane_upper>>j) & 1) << 1;
            pixel_value |= ((bit_plane_lower>>j) & 1);

            col = (i/2)%pw_img->width;
            row = 8*(i/(2*pw_img->width)) + j;

            // Convert pw 2bpp to oled 4bpp via map
            // assuming 1 byte per pixel
            decode_buf[ (row*pw_img->width)+col ] = pixel_value;
        }
    }

    for(size_t i = 0; i < pw_img->width*pw_img->height; i++) {
        uint32_t c = colours[decode_buf[i]]; // 2-bpp colour/index to RGB888 buffer
        sdl_buf[3*i+0] = (c & 0x00ff0000)>>16; // R
        sdl_buf[3*i+1] = (c & 0x0000ff00)>>8; // G
        sdl_buf[3*i+2] = c & 0x000000ff; // B
    }

    SDL_UnlockTexture(*tex);

    free(decode_buf);
}


void pw_screen_draw_img(pw_img_t *img, screen_pos_t x, screen_pos_t y) {
    // separate convert function and then upscales it, converts it to SDL texture or something and draws it

    SDL_Texture *tex = NULL;
    decode_img(img, &tex);

    SDL_Rect rect = {.x=x, .y=y, .w=img->width, .h=img->height};
    SDL_RenderCopy(renderer, tex, NULL, &rect); // NULL = use whole texture
    SDL_RenderPresent(renderer);
}

void pw_screen_sleep() { }
void pw_screen_wake() { }

#define AUDIO_SAMPLE_RATE 44100

const SDL_AudioSpec desired_audio_spec = (const SDL_AudioSpec) {
    .freq = AUDIO_SAMPLE_RATE,
    .format = AUDIO_S16SYS,
    .channels = 1,
    .samples = 1024,
    .callback = NULL,
};

struct audio_buffer_t {
    void* data;
    size_t size; // in bytes
};

void add_note(struct audio_buffer_t* audio_buffer, uint8_t period, uint16_t duration) {
    double period_samples = (AUDIO_SAMPLE_RATE / 65536.0) * period;
    size_t dur_samples = (AUDIO_SAMPLE_RATE / 32768.0) * duration;
    size_t old_size = audio_buffer->size;
    size_t new_size = old_size + sizeof(int16_t)*dur_samples;
    if (old_size > 0) {
        audio_buffer->data = realloc(audio_buffer->data, new_size);
    } else {
        audio_buffer->data = malloc(new_size);
    }
    audio_buffer->size = new_size;

    int16_t* audio_it = (int16_t*)((uint8_t*)audio_buffer->data + old_size);
    for (size_t i = 0; i < dur_samples; i++) {
         audio_it[i] = pw_audio_volume * pw_audio_volume * (8191 * ((int)(i / period_samples) % 2) - 8191); // square wave, quadratic volume
    }
}

void add_silence(struct audio_buffer_t* audio_buffer, uint16_t samples) {
    size_t old_size = audio_buffer->size;
    size_t new_size = old_size + sizeof(int16_t)*samples;
    if (old_size > 0) {
        audio_buffer->data = realloc(audio_buffer->data, new_size);
    } else {
        audio_buffer->data = malloc(new_size);
    }
    audio_buffer->size = new_size;

    int16_t* audio_it = (int16_t*)((uint8_t*)audio_buffer->data + old_size);
    for (size_t i = 0; i < samples; i++) {
         audio_it[i] = 0;
    }
}

void pw_audio_init() {
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &desired_audio_spec, NULL, 0); 
    if (audio_dev == 0)
        fprintf(stderr, "SDL audio device failed to initialise: %s\n", SDL_GetError());
}

void pw_audio_play_sound_data(const pw_sound_frame_t* sound_data, size_t sz) {
    if (pw_audio_volume == VOLUME_NONE) return;

    SDL_PauseAudioDevice(audio_dev, 0);
    struct audio_buffer_t audio_buffer;
    audio_buffer.size = 0;

    uint8_t SH;
    for (pw_sound_frame_t* sound_frame = sound_data; sound_frame < sound_data + sz; sound_frame++) {
        uint8_t info = sound_frame->info;
        uint8_t idx = sound_frame->period_idx;

        if (idx == 0x7b) {
            // Signifies start of sound data
            SH = info;
            continue;
	}
        if (idx == 0x7f) {
            // Signifies end of sound data
            break;
	}
        if (idx == 0x7d) {
            printf("WARN %x commands are not yet handled correctly\n", idx);
            continue;
	}

	bool negative = idx > 0x80;
	uint16_t dur;
        if (negative) {
            dur = ((0x14000 * info / SH));
	} else {
            dur = ((0x14000 * info / SH) - 0x140);
	}

	uint8_t period = PW_AUDIO_PERIODTAB[idx & 0x7f];

	add_note(&audio_buffer, period, dur);
	if (!negative) {
            add_silence(&audio_buffer, 300);
	}
    }

    SDL_QueueAudio(audio_dev, audio_buffer.data, audio_buffer.size);
    if (audio_buffer.size > 0)
        free(audio_buffer.data);
}


bool pw_audio_is_playing_sound() {
  return SDL_GetQueuedAudioSize(audio_dev) != 0;
}



void pw_flash_read(uint8_t *buf, size_t len) {
    // read file, fread into buf. easy.
    const char fname[] = "extraflash.bin";
    FILE *f = fopen(fname, "rb");
    if(!f) {
        fprintf(stderr, "Cannot open flash file: %s\n", fname);
    }
    fread(buf, len, 1, f);
    fclose(f);
}

static struct timespec last_power_meas;

pw_battery_status_t pw_power_get_battery_status() { return (pw_battery_status_t){0}; }

void pw_power_init() {
    // Nothing to do
    timespec_get(&last_power_meas, TIME_UTC);
    last_power_meas.tv_sec -= 60;
}

bool pw_power_should_sleep() {
    // TODO: keep a timer and check that
    return false;
}

void pw_power_enter_sleep() {
    // we don't sleep
    // change this if we do
}

pw_wake_reason_t pw_power_get_wake_reason() { return 0; }
void pw_power_clear_wake_reason(pw_wake_reason_t wake_reason) { }

void pw_ir_wake() {
    // TODO: we don't have IR, but if we did, we activate it here
}

void pw_ir_sleep() {
    // TODO: this shouldn't be here
}

void pw_battery_shutdown() {
    SDL_DestroyWindow(window);
    SDL_Quit();
    exit(0);
}

void pw_power_start_measurement() { }

pw_battery_status_t pw_power_get_status() {
    struct timespec now;
    timespec_get(&now, TIME_UTC);

    pw_battery_status_t bs = (pw_battery_status_t){
        .percent = 0,
        .flags = 0
    };

    if(now.tv_sec - last_power_meas.tv_sec > 60) {
        bs.percent = 100;
        bs.flags |= PW_BATTERY_STATUS_FLAGS_MEASUREMENT;
        last_power_meas = now;
    }

    return bs;
}

// ========================================================================================================

int main(int argc, char** argv) {


    SDL_SetMainReady();
    if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0 ) {
        fprintf(stderr, "SDL failed to initialise: %s\n", SDL_GetError());
        return -1;
    }

#ifdef WIN32
    // hide console for windows
    ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif

    /*
        pw_ir_init();
        pw_button_init();
        pw_screen_init();
        pw_eeprom_init();
        pw_accel_init();

        // set up frame
        SDL_SetRenderDrawColor(renderer, 0xac, 0xae, 0xa4, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        // draw frame
        SDL_RenderPresent(renderer);

        //SDL_Delay(3000);


        pw_screen_draw_horiz_line(0, 40, 96, 0);
        pw_screen_draw_horiz_line(0, 41, 96, 1);
        pw_screen_draw_text_box(0, 32, 96, 64, 0);

    */

    walker_setup();

    while(1) {
        //walker_loop();
        current_loop();
        check_event();
        SDL_RenderPresent(renderer);
        sleep(0);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
