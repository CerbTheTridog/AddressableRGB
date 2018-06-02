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


uint8_t RED = 0;
uint8_t ORANGE = 1;
uint8_t YELLOW = 2;
uint8_t GREEN = 3;
uint8_t LIGHTBLUE = 4;
uint8_t BLUE = 5;
uint8_t PURPLE = 6;
uint8_t PINK = 7;

static ws2811_led_t dotcolors[] =
{
    0x00FF0000,  // red
    0x00FF8000,  // orange
    0x00FFFF00,  // yellow
    0x0000FF00,  // green
    0x0000FFFF,  // lightblue
    0x000000FF,  // blue
    0x00FF00FF,  // purple
    0x00FF0080,  // pink
};

void
move_lights(struct pattern *pattern) {
    ws2811_led_t *led_array = pattern->ledstring.channel[0].leds;

    // Shift everything in ledstring exactly one led forward
    int i = pattern->led_count - 1;
    while (i > 0) {
        led_array[i] = led_array[i -1];
        i--;
    }
}

/* Run the threaded loop */
void *
matrix_run2(void *vargp)
{
    log_matrix_trace("pulse_run()");

    ws2811_return_t ret;
    struct pattern *pattern = (struct pattern*)vargp;
    int len = pattern->width;
    int amp = pattern->height;
    uint32_t color = dotcolors[PURPLE];
    uint32_t r = (color & 0xFF0000);
    uint32_t g = (color & 0x00FF00);
    uint32_t b = (color & 0x0000FF);
    double slope = (double) (((double)amp / (double)len) / 256);
    pattern->ledstring.channel[0].brightness = 255;


    //int count = 0;
    /* This should never get called before load_rainbox_pattern initializes stuff.
     * Or ever be called after kill_pattern_rainbox */
    assert(pattern->running);
    uint32_t red, green, blue;
    double scalar;
    while (pattern->running)
    {
        /* If the pattern is paused, we won't update anything */
        if (!pattern->paused) {
            //double slope = (double) (((double)amp / (double)len) / 256);
            double prev_amp = slope;
            int i = 0;
            while (i < len-1) {
                move_lights(pattern);
                
                /*double */scalar = prev_amp;
                //printf("Scalar: %lf\n", scalar);
                prev_amp += slope;

                //auint32_t color = dotcolors[PURPLE];

                /*uint32_t*/ red = r; //(color & 0xFF0000);
                red = (r >> 16);
                red = ((uint32_t)(red * scalar) << 16);

                /*uint32_t*/ green = g; //(color & 0x00FF00);
                green = (green >> 8);
                green = ((uint32_t)(green * scalar) << 8);
                
                /*uint32_t*/ blue = b; //(color & 0x0000FF);
                blue = (blue * scalar);

                pattern->ledstring.channel[0].leds[0] = (red+green+blue);
                //printf("[%d] = %d %d %d\n", i, red, green, blue);
                i++;
                //count++;
                //printf("Count: %d\n", count);
                if ((ret = ws2811_render(&pattern->ledstring)) != WS2811_SUCCESS)
                {
                    printf("OHJESUSFUCK\n");
                    log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
                    // XXX: This should cause some sort of fatal error to propogate upwards
                    break;
                }

                //usleep(1000000 / pattern->movement_speed);
            }
            while (i >= 0) {
                move_lights(pattern);
                
                /*double*/ scalar = prev_amp;
                prev_amp -= slope;

                //uint32_t color = dotcolors[PURPLE];

                /*uint32_t*/ red = r; //(color & 0xFF0000);
                red = (red >> 16);
                red = ((uint32_t)(red * scalar) << 16);

                /*uint32_t*/ green = (color & 0x00FF00);
                green = (green >> 8);
                green = ((uint32_t)(green * scalar) << 8);
                
                /*uint32_t*/ blue = (color & 0x0000FF);
                blue = (blue * scalar);
                //printf("[%d] = %d %d %d\n", i, red, green, blue);
                pattern->ledstring.channel[0].leds[0] = (red+green+blue);

                i--;
                //count++;
                //printf("Count: %d\n", count);
                if ((ret = ws2811_render(&pattern->ledstring)) != WS2811_SUCCESS)
                {
                    printf("OHJESUSFUCK\n");
                    log_error("ws2811_render failed: %s", ws2811_get_return_t_str(ret));
                    // XXX: This should cause some sort of fatal error to propogate upwards
                    break;
                }

                //usleep(1000000 / pattern->movement_speed);
            }

            //pattern->ledstring.channel[0].leds[0] = 0xFF0000;
        }
        // 15 frames /sec
        //usleep(1000000 / pattern->refresh_rate);
    }

    return NULL;
}

/* Initialize everything, and begin the thread */
ws2811_return_t
pulse_load(struct pattern *pattern)
{
    log_trace("pulse_load()");

    /* Allocate memory */
    //pattern->matrix = calloc(pattern->width*pattern->height, sizeof(ws2811_led_t));

    /* A protection against matrix_run() being called in a bad order. */
    pattern->running = 1;

    pthread_create(&pattern->thread_id, NULL, matrix_run2, pattern);
    log_info("Rainbow Pattern Loop is now running.");
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

    if (pattern->clear_on_exit) {
        log_info("Pulse Pattern Loop: Clearing matrix");
        //matrix_clear(pattern);
        //matrix_render(pattern);
        ws2811_render(&pattern->ledstring);
    }
    log_debug("Rainbow Pattern Loop: Stopping run");
    pattern->running = 0;

    log_debug("Rainbow Pattern Loop: Waiting for thread %d to end", pattern->thread_id);
    pthread_join(pattern->thread_id, NULL);

    log_info("Rainbow Pattern Loop: now stopped");
    return WS2811_SUCCESS;
}

ws2811_return_t
pulse_create(struct pattern **pattern)
{
    log_trace("pulse_create()");
    *pattern = malloc(sizeof(struct pattern));
    if (*pattern == NULL) {
        log_error("Rainbow Pattern: Unable to allocate memory for pattern\n");
        return WS2811_ERROR_OUT_OF_MEMORY;
    }
    (*pattern)->func_load_pattern = &pulse_load;
    (*pattern)->func_start_pattern = &pulse_start;
    (*pattern)->func_kill_pattern = &pulse_kill;
    (*pattern)->func_pause_pattern = &pulse_pause;
    (*pattern)->running = true;
    (*pattern)->paused = true;
    return WS2811_SUCCESS;
}   

ws2811_return_t
pulse_delete(struct pattern *pattern)
{
    log_trace("pulse_delete()");
    log_debug("Pulse Pattern: Freeing objects");
    free(pattern->matrix);
    pattern->matrix = NULL;
    free(pattern);
    return WS2811_SUCCESS;
}
