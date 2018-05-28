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
#include "pattern_rainbow.h"

#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

//static uint8_t running;
bool debug = 1;

#define DEBUG_LOG(x)\
    if (debug) {\
        printf(x);\
    }

uint16_t dotspos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

ws2811_led_t dotcolors[] =
{
    0x00200000,  // red
    0x00201000,  // orange
    0x00202000,  // yellow
    0x00002000,  // green
    0x00002020,  // lightblue
    0x00000020,  // blue
    0x00100010,  // purple
    0x00200010,  // pink
};

ws2811_led_t dotcolors_rgbw[] =
{
    0x00200000,  // red
    0x10200000,  // red + W
    0x00002000,  // green
    0x10002000,  // green + W
    0x00000020,  // blue
    0x10000020,  // blue + W
    0x00101010,  // white
    0x10101010,  // white + W

};


/* Stamp onto the string */
void matrix_render(struct pattern *pattern)
{
    DEBUG_LOG("matrix_renderer() {\n");
    int x, y;
    int width = pattern->width;
    int height = pattern->height;
    for (x = 0; x < width; x++)
    {
        for (y = 0; y < height; y++)
        {   
            pattern->ledstring.channel[0].leds[(y * width) + x] = pattern->matrix[y * width + x];
        }
    }
    DEBUG_LOG("} matrix_renderer()\n");
}

/* Not used in a 1-dimensional string */
void matrix_raise(struct pattern *pattern)
{
    DEBUG_LOG("matrix_raise() {\n");
    int x, y;
    int height = pattern->height;
    int width = pattern->width;
    /* See if height is 1, then this is one dimensional */
    for (y = 0; y < (height - 1); y++)
    {
        for (x = 0; x < width; x++)
        {
            // This is for the 8x8 Pimoroni Unicorn-HAT where the LEDS in subsequent
            // rows are arranged in opposite directions
            pattern->matrix[y * width + x] = pattern->matrix[(y + 1)*width + width - x - 1];
        }
    }
    DEBUG_LOG("} matrix_raise()\n");
}

void matrix_clear(struct pattern *pattern)
{
    DEBUG_LOG("matrix_clear() {\n");
    int x, y;
    int height = pattern->height;
    int width = pattern->width;

    for (y = 0; y < (height ); y++)
    {
        for (x = 0; x < width; x++)
        {
            pattern->matrix[y * width + x] = 0;
        }
    }
    DEBUG_LOG("} matrix_clear()\n");
}

void matrix_bottom(struct pattern *pattern)
{
    DEBUG_LOG("matrix_bottom()\n");

    int i;
    for (i = 0; i < (int)(ARRAY_SIZE(dotspos)); i++)
    {
        dotspos[i]++;
        
        /* Loop back to beginning of string */
        if (dotspos[i] > (pattern->width - 1))
        {
            dotspos[i] = 0;
        }

        /* Not mine */
        if (pattern->ledstring.channel[0].strip_type == SK6812_STRIP_RGBW) {
            pattern->matrix[dotspos[i] + (pattern->height - 1) * pattern->width] = dotcolors_rgbw[i];
        }
        /* Mine */
        else {
            pattern->matrix[dotspos[i] + (pattern->height - 1) * pattern->width] = dotcolors[i];
        }
    }
    /* This clears all lights that are not currently in the array */
    i = 0;
    while (i < dotspos[0]) {
        pattern->matrix[i] = 0;
        i++;
    }
    if (dotspos[7] == pattern->led_count-1) {
        pattern->matrix[dotspos[7]] = 0;
    }
}

/* Run the threaded loop */
void *
matrix_run(void *vargp)
{
    DEBUG_LOG("rainbow_run()\n");

    ws2811_return_t ret;
    struct pattern *pattern = (struct pattern*)vargp;

    /* This should never get called before load_rainbox_pattern initializes stuff.
     * Or ever be called after kill_pattern_rainbox */
    assert(pattern->running);

    while (pattern->running)
    {
        /* If the pattern is paused, we won't update anything */
        if (!pattern->paused) {
            matrix_raise(pattern);
            matrix_bottom(pattern);
            matrix_render(pattern);
            if ((ret = ws2811_render(&pattern->ledstring)) != WS2811_SUCCESS)
            {
                fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
                break;
            }
        }
        // 15 frames /sec
        usleep(1000000 / pattern->refresh_rate);
    }

    return NULL;
}

/* Initialize everything, and begin the thread */
ws2811_return_t
rainbow_load(struct pattern *pattern)
{
    DEBUG_LOG("rainbow_load()\n");
    /* Allocate memory */
    pattern->matrix = calloc(pattern->width*pattern->height, sizeof(ws2811_led_t));
    pattern->running = 1;
    DEBUG_LOG("Before thread\n");
    pthread_create(&pattern->thread_id, NULL, matrix_run, pattern);
    DEBUG_LOG("Loop is now running\n");
    return WS2811_SUCCESS;
}

ws2811_return_t
rainbow_start(struct pattern *pattern)
{
    pattern->paused = false;
    return WS2811_SUCCESS;
}

ws2811_return_t
rainbow_stop(struct pattern *pattern)
{
    pattern->paused = true;
    matrix_clear(pattern);
    return WS2811_SUCCESS;
}

ws2811_return_t
rainbow_pause(struct pattern *pattern)
{
    pattern->paused = true;
    return WS2811_SUCCESS;
}

/* Turn off strip, kill second process */
ws2811_return_t
rainbow_kill(struct pattern *pattern)
{
    DEBUG_LOG("rainbow_kill()\n");
    pthread_join(pattern->thread_id, NULL);
    DEBUG_LOG("Loop has successfully terminated\n");
    if (pattern->clear_on_exit) {
        matrix_clear(pattern);
        matrix_render(pattern);
        ws2811_render(&pattern->ledstring);
    }
    pattern->running = 0;
    return WS2811_SUCCESS;
}

ws2811_return_t
rainbow_interupt(struct pattern *pattern)
{
    pattern->running = 0;
    return WS2811_SUCCESS;
}


ws2811_return_t
rainbow_create(struct pattern **pattern)
{
    *pattern = malloc(sizeof(struct pattern));
    if (*pattern == NULL) {
        return WS2811_ERROR_OUT_OF_MEMORY;
    }
    (*pattern)->func_load_pattern = &rainbow_load;
    (*pattern)->func_start_pattern = &rainbow_start;
    (*pattern)->func_kill_pattern = &rainbow_kill;
    (*pattern)->func_interupt = &rainbow_interupt;
    (*pattern)->func_pause_pattern = &rainbow_pause;
    (*pattern)->running = true;
    (*pattern)->paused = true;
    return WS2811_SUCCESS;
}   

ws2811_return_t
rainbow_delete(struct pattern *pattern)
{
    free(pattern->matrix);
    pattern->matrix = NULL;
    free(pattern);
    return WS2811_SUCCESS;
}
