/* Timer group-hardware timer example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#define TIMER_DIVIDER         2  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (3.4179) // sample test interval for the first timer    // testing will be done without auto reload

/*
 * A sample structure to pass events
 * from the timer interrupt handler to the main program.
 */
typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;



/*
 * A simple helper function to print the raw timer counter value
 * and the counter value converted to seconds
 */
static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
    printf("Time   : %.8f s\n", (double) counter_value / TIMER_SCALE);
}

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 */

static void example_tg0_timer_init()
{
    /* Select and initialize basic parameters of the timer */
        timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_DOWN;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_DIS;
    //config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = TIMER_AUTORELOAD_DIS;
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x7270E00ULL);

    timer_start(TIMER_GROUP_0, TIMER_0);
}

void timer_control (void *pvParameter){
    uint64_t task_counter_value;
    while (1)
    {
        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &task_counter_value);
        if (task_counter_value > 0xE4E1C00ULL)
        {
            timer_pause(TIMER_GROUP_0, TIMER_0);

        }
 /*       else
        {
            timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x7270E00ULL);
            timer_start(TIMER_GROUP_0, TIMER_0);
        }*/
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    





}

/*
 * In this example, we will test hardware timer0 and timer1 of timer group0.
 */
void app_main(void)
{
    xTaskCreate(timer_control, "timer_control", 2048, NULL, 4, NULL);
    example_tg0_timer_init(TIMER_0);
    while(1)
    {       
    uint64_t task_counter_value;
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &task_counter_value); 
    print_timer_counter(task_counter_value);
    vTaskDelay(pdMS_TO_TICKS(100));
    }
}

