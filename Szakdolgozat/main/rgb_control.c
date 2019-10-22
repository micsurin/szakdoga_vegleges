#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"


#define LEDC_LS_TIMER          LEDC_TIMER_1
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_LS_CH2_GPIO       (18)                     //RED
#define LEDC_LS_CH2_CHANNEL    LEDC_CHANNEL_2
#define LEDC_LS_CH3_GPIO       (5)                      //GREEN
#define LEDC_LS_CH3_CHANNEL    LEDC_CHANNEL_3
#define LEDC_LS_CH4_GPIO       (17)                     //BLUE
#define LEDC_LS_CH4_CHANNEL    LEDC_CHANNEL_4

extern int watt;
extern int usb_bedugva;
extern int chargestate;
void rgb_control(void *pvParameter)
{
    ledc_timer_config_t rgb_timer = {
        .duty_resolution = LEDC_TIMER_5_BIT, // resolution of PWM duty
        .freq_hz = 500,                      // frequency of PWM signal
        .speed_mode = LEDC_LS_MODE,           // timer mode
        .timer_num = LEDC_LS_TIMER,            // timer index
    };

    ledc_timer_config(&rgb_timer);

    ledc_channel_config_t rgb_red_channel = 
        {
            .channel    = LEDC_LS_CH2_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_LS_CH2_GPIO,
            .speed_mode = LEDC_LS_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_LS_TIMER
        };
        ledc_channel_config_t rgb_green_channel = 
        {
            .channel    = LEDC_LS_CH3_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_LS_CH3_GPIO,
            .speed_mode = LEDC_LS_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_LS_TIMER
        };
        ledc_channel_config_t rgb_blue_channel = 
        {
            .channel    = LEDC_LS_CH4_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_LS_CH4_GPIO,
            .speed_mode = LEDC_LS_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_LS_TIMER
        };

    ledc_channel_config(&rgb_red_channel);
    ledc_channel_config(&rgb_green_channel);
    ledc_channel_config(&rgb_blue_channel);

while (1)
{   if(usb_bedugva == 0)
{
if (0 == watt)
    {
        ledc_set_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel, 0);
        ledc_set_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel, 0);
        ledc_set_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel, 31);
        ledc_update_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel);
        ledc_update_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel);
        ledc_update_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel);
    }
    if (1 <= watt && 20 > watt)
    {
        ledc_set_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel, 0);
        ledc_set_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel, (watt*(124.0/76.0)));
        ledc_set_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel, 31);
        ledc_update_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel);
        ledc_update_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel);
        ledc_update_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel);
    }
    if (20 <= watt && 39 > watt)
    {
        ledc_set_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel, 0);
        ledc_set_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel, 31);
        ledc_set_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel, (62.0-(watt*(124.0/76.0))));
        ledc_update_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel);
        ledc_update_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel);
        ledc_update_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel);
    }
    if (39 <= watt && 58 > watt)
    {
        ledc_set_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel, ((watt*(124.0/76.0))-62.0));
        ledc_set_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel, 31);
        ledc_set_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel, 0);
        ledc_update_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel);
        ledc_update_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel);
        ledc_update_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel);
    }
    if (58 <= watt && 75 > watt)
    {
        ledc_set_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel, 31);
        ledc_set_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel, (124.0-(watt*(124.0/76.0))));
        ledc_set_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel, 0);
        ledc_update_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel);
        ledc_update_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel);
        ledc_update_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel);
    }
    if (75 == watt)
    {
        ledc_set_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel, 31);
        ledc_set_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel, 0);
        ledc_set_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel, 0);
        ledc_update_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel);
        ledc_update_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel);
        ledc_update_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel);
    }
}
else
{
    if (chargestate == 1)
    {
        ledc_set_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel, 31);
        ledc_set_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel, 0);
        ledc_set_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel, 31);
        ledc_update_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel);
        ledc_update_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel);
        ledc_update_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel);
    }
    else
    {
        ledc_set_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel, 0);
        ledc_set_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel, 31);
        ledc_set_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel, 0);
        ledc_update_duty(rgb_red_channel.speed_mode, rgb_red_channel.channel);
        ledc_update_duty(rgb_green_channel.speed_mode, rgb_green_channel.channel);
        ledc_update_duty(rgb_blue_channel.speed_mode, rgb_blue_channel.channel);
    }
    
    
}
vTaskDelay(pdMS_TO_TICKS(100));


}

}