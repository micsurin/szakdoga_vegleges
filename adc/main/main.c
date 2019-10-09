#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_attr.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

//ADC paraméterek
#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

//PWM és timer paraméterek
#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       (18)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_1

//GPIO paraméterek
#define gomb1 15
#define GPIO_INPUT_PIN_SEL ((1ULL<<gomb1))

//PWM paraméterek
#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       (18)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_1



uint32_t szint;//GPIO szint

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;
uint32_t percent = 0;

static void check_efuse(void)
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}
//GPIO inicializálása
void gomb_init(void){
    gpio_config_t input_config = {
        .intr_type = GPIO_PIN_INTR_ANYEDGE,
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1
    };
    gpio_config(&input_config);
}
//GOMB kiolvasása
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
//ADC karakterizáció
void adc_charac (void){
        adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);
}
// ADC kiolvasása
void adc_read(void *pvParameter){
     while (1) {
        uint32_t adc_reading = 0;
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, ADC_WIDTH_BIT_12, &raw);
                adc_reading += raw;
            }
        }
        adc_reading /= NO_OF_SAMPLES;
        percent=adc_reading/40.95;
        //Convert adc_reading to voltage in mV
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
        printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
        printf("%d  \n", percent);
        vTaskDelay(pdMS_TO_TICKS(10));
}

//PWM konfigurálása
void pwm_config(void)

{

    ledc_timer_config_t ledc_timer = {

        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_HS_MODE,
        .timer_num = LEDC_HS_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    
    };

    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
            .channel    = LEDC_HS_CH0_CHANNEL,
            .duty       = 0,
            .gpio_num   = LEDC_HS_CH0_GPIO,
            .speed_mode = LEDC_HS_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_HS_TIMER
        };

    ledc_channel_config(&ledc_channel);
}

void pwm(void *pvParameter){
    while(1){
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, adc_reading);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
    vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void app_main(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();
    //Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(channel, atten);
    //Characterize ADC
    adc_charac();
    //configure GPIO
    gomb_init();
    //Configure PWM
    set_pwm();

    xTaskCreate(&adc_read, "read adc task", 2048, NULL, 5, NULL);
    xTaskCreate(&pwm, "set pwm", 2048, NULL, 5, NULL);

}


