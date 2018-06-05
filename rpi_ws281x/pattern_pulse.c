/*
 * newtest.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>
#include <pthread.h>
#include <assert.h>

#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"

#include "ws2811.h"
#include "pattern_pulse.h"
#include "log.h"

#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

void
move_lights(struct pattern *pattern) {
    ws2811_led_t *led_array = pattern->ledstring.channel[0].leds;

    // Shift everything in ledstring exactly one led forward
    int i = pattern->led_count - 1;
    while (i > 0) {
        memmove(&led_array[i], &led_array[i-1], sizeof(ws2811_led_t));
        i--;
    }
}

static ws2811_led_t color = 0;
static bool newColor = false;
static uint32_t intensity = 0;
/* A new color has been injected. What happens to old color? */
ws2811_return_t
pulse_inject(ws2811_led_t in_color, uint32_t in_intensity)
{
    log_trace("pulse_inject(): %d, %d\n", in_color, in_intensity);
    ws2811_return_t ret = WS2811_SUCCESS;
    color = in_color;
    newColor = true;
    intensity = in_intensity;
    return ret;
}

/* Run the threaded loop */
void *
matrix_run2(void *vargp)
{
    log_matrix_trace("pulse_run()");

    ws2811_return_t ret = WS2811_SUCCESS;
    struct pattern *pattern = (struct pattern*)vargp;
    assert(pattern->running);
    pattern->ledstring.channel[0].brightness = 255;
    #if 0
    while (pattern->running)
    {
        int len = pattern->width;
        int amp = pattern->height;

        /* If the pattern is paused, we won't update anything */
        if (!pattern->paused) {
            if (!newColor) {
                move_lights(pattern);
                pattern->ledstring.channel[0].leds[0] = 0;
                log_matrix_trace("Injecting 0");

                if ((ret = ws2811_render(&pattern->ledstring)) != WS2811_SUCCESS) {
                    log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
                    // XXX: This should cause some sort of fatal error to propogate upwards
                    break;
                }

                usleep(1000000 / pattern->movement_rate);
                continue;
            }
            // XXX: This should be based on intensity, not amp and length
            double slope = (double) (((double)amp / (double)len) / 256);
            slope = slope * (double)((double)intensity/(double)100);

            int i = 0;
            double prev_amp = slope;
            bool rampUp = true;
            bool colorFinished = false;
            newColor = false;
            int loopIteration = 0;
            while (!colorFinished && pattern->running ) {
                uint32_t r_shift = (color & 0xFF0000) >> 16;
                uint32_t g_shift = (color & 0x00FF00) >> 8;
                uint32_t b_shift = (color & 0x0000FF);
                uint32_t red, green, blue;
                double scalar;
        
                if (newColor) {

                    prev_amp = slope;
                    i = 0;
                    newColor = false;
                    rampUp = true;
                    colorFinished = false;
                }   
                move_lights(pattern);
                
                if (rampUp && (i == len-1)) {
                    rampUp = false;
                }
                else if (!rampUp && (i == 0)) {
                    rampUp = true;
                    colorFinished = true;
                    break;
                }
                
                scalar = prev_amp;
                prev_amp = (rampUp) ? (prev_amp + slope) : (prev_amp - slope);
                
                red = ((uint32_t)(r_shift * scalar) << 16);
                green = ((uint32_t)(g_shift * scalar) << 8);
                blue = ((uint32_t)(b_shift * scalar));

                pattern->ledstring.channel[0].leds[0] = (red+green+blue);
                log_matrix_trace("Injecting %d %d %d\n", i, red, green, blue);

                if ((ret = ws2811_render(&pattern->ledstring)) != WS2811_SUCCESS) {
                    log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
                    // XXX: This should cause some sort of fatal error to propogate upwards
                    break;
                }
                i = (rampUp) ? (i + 1) : (i - 1);
                loopIteration++;
                usleep(1000000 / pattern->movement_rate);
            }
        }
    }
    #else
    int i = 0;
    bool rampUp = true;
    bool colorFinished = false;
    newColor = false;
    double prev_amp;
    while (pattern->running)
    {
        int pulseWidth = pattern->pulseWidth;
        int pulseBrightness = pattern->ledstring.channel[0].brightness;
        double slope = (double)(((double)pulseBrightness/(double)pulseWidth) / 256);
        slope = slope * (double)((double)intensity/(double)100);
        prev_amp = slope;

        if (!pattern->paused) {
            move_lights(pattern);
            if (!newColor && colorFinished) {
                pattern->ledstring.channel[0].leds[0] = 0;
            } else if (newColor && !colorFinished) { 
                uint32_t r_shift = (color & 0xFF0000) >> 16;
                uint32_t g_shift = (color & 0x00FF00) >> 8;
                uint32_t b_shift = (color & 0x0000FF);
                uint32_t red, green, blue;
                double scalar;
                
                if (rampUp && (i == pulseWidth-1)) {
                    rampUp = false;
                }
                else if (!rampUp && (i == 0)) {
                    rampUp = true;
                    colorFinished = true;
                    break;
                }
                
                scalar = prev_amp;
                prev_amp = (rampUp) ? (prev_amp + slope) : (prev_amp - slope);
                
                red = ((uint32_t)(r_shift * scalar) << 16);
                green = ((uint32_t)(g_shift * scalar) << 8);
                blue = ((uint32_t)(b_shift * scalar));

                pattern->ledstring.channel[0].leds[0] = (red+green+blue);
                log_matrix_trace("Injecting %d %d %d\n", i, red, green, blue);
                i = (rampUp) ? (i + 1) : (i - 1);
                if (i == 0) {
                    colorFinished = true;
                }
            }
            else if (newColor && colorFinished) {
                log_matrix_trace("Injecting 0");

                prev_amp = slope;
                i = 0;
                newColor = false;
                rampUp = true;
                colorFinished = false;
            }

            if ((ret = ws2811_render(&pattern->ledstring)) != WS2811_SUCCESS) {
                log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
                // xxx: this should cause some sort of fatal error to propogate upwards
                break;
            }
        }
        usleep(1000000 / pattern->movement_rate);
    }
#endif
    return NULL;
}

/* Initialize everything, and begin the thread */
ws2811_return_t
pulse_load(struct pattern *pattern)
{
    log_trace("pulse_load()");

    /* A protection against matrix_run() being called in a bad order. */
    pattern->running = 1;

    pthread_create(&pattern->thread_id, NULL, matrix_run2, pattern);
    log_info("Pattern Pulse: Loop is now running.");
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_start(struct pattern *pattern)
{
    log_trace("pulse_start()");
    pattern->paused = false;
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_stop(struct pattern *pattern)
{
    log_trace("pulse_stop()");
    pattern->paused = true;
    //matrix_clear(pattern);
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_clear(struct pattern *pattern)
{
    log_trace("pulse_clear()\n");
    log_info("Pattern Pulse: Clearing Pattern");
    ws2811_return_t ret = WS2811_SUCCESS;
    uint32_t i;

    for (i = 0; i < pattern->led_count; i++) {
        pattern->ledstring.channel[0].leds[i] = 0;
    }

    if ((ret = ws2811_render(&pattern->ledstring)) != WS2811_SUCCESS) {
        log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
        // xxx: this should cause some sort of fatal error to propogate upwards
    }
    return ret;
}

ws2811_return_t
pulse_pause(struct pattern *pattern)
{
    log_trace("pulse_pause()");
    pattern->paused = true;
    return WS2811_SUCCESS;
}

/* Turn off strip, kill second process */
ws2811_return_t
pulse_kill(struct pattern *pattern)
{
    log_trace("pulse_kill()");
    log_debug("Pattern Pulse: Stopping run");
    pattern->running = 0;

    log_debug("Pattern Pulse: Loop Waiting for thread %d to end", pattern->thread_id);
    pthread_join(pattern->thread_id, NULL);

    if (pattern->clear_on_exit) {
        pulse_clear(pattern);
    }

    log_info("Pattern Pulse: Loop now stopped");
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_create(struct pattern **pattern)
{
    log_trace("pulse_create()");
    *pattern = malloc(sizeof(struct pattern));
    if (*pattern == NULL) {
        log_error("Pattern Pulse: Unable to allocate memory for pattern\n");
        return WS2811_ERROR_OUT_OF_MEMORY;
    }
    (*pattern)->func_load_pattern = &pulse_load;
    (*pattern)->func_start_pattern = &pulse_start;
    (*pattern)->func_kill_pattern = &pulse_kill;
    (*pattern)->func_pause_pattern = &pulse_pause;
    (*pattern)->func_inject = &pulse_inject;
    (*pattern)->running = true;
    (*pattern)->paused = true;
    return WS2811_SUCCESS;
}   

ws2811_return_t
pulse_delete(struct pattern *pattern)
{
    log_trace("pulse_delete()");
    log_debug("Pattern Pulse: Freeing objects");
    free(pattern->matrix);
    pattern->matrix = NULL;
    free(pattern);
    return WS2811_SUCCESS;
}
