#ifndef __PATTERN_H
#define __PATTERN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <pthread.h>

/* The basic structure for all patterns */
struct pattern
{
    /* x */
    uint8_t width;
    /* y */
    uint8_t height;
    /* The total number of LEDs to consider as a part of the pattern */
    uint32_t led_count;
    /* Turn off the lights when exiting */
    bool clear_on_exit;
    /* Refresh rate - frames per second */
    uint16_t refresh_rate;
    /* Program is loaded into memory */
    bool running;
    /* Program is actively paused, but still loaded */
    bool paused;
    /* The thread id of the running loop */
    pthread_t thread_id;

    /* The actual led string */
    ws2811_t ledstring;
    /* The 2-dimensional representation of what lights are what color */
    ws2811_led_t *matrix;
    
    /* Load a given pattern and start its threaded loop */
    ws2811_return_t (*func_load_pattern)(struct pattern *pattern);
    /* Begin displaying a given pattern */
    ws2811_return_t (*func_start_pattern)(struct pattern *pattern);
    /* Pause or resume a given pattern, based upon argument "pause" */
    ws2811_return_t (*func_pause_pattern)(struct pattern *pattern);
    /* Kill whatever pattern is running */
    ws2811_return_t (*func_kill_pattern)(struct pattern *pattern);
    /* Free pattern form memory */
    ws2811_return_t (*func_delete)(struct pattern *pattern);
    /* Interupt whatever pattern is running - Specifically for control+c */
    ws2811_return_t (*func_interupt)(struct pattern *pattern);
};


#ifdef __cplusplus
}
#endif

#endif
