#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"


#define gomb1 15
#define GPIO_INPUT_PIN_SEL ((1ULL<<gomb1))

uint32_t szint;
void gomb_init(void){
    gpio_config_t input_config = {
        .intr_type = GPIO_PIN_INTR_ANYEDGE,
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1
    };
    gpio_config(&input_config);
}

void gomb(void){
    while(1){
        gpio_get_level(szint);
        if(szint==0){
            printf("\n a gomb meg van nyomva");
        }
        else
        {
            printf("\n a gomb nincs megnyomva");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }



}