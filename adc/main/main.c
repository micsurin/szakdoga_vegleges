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
#include "driver/ledc.h"
#include "soc/mcpwm_reg.h"
#include "soc/mcpwm_struct.h"

//ADC paraméterek
#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

//PWM és timer paraméterek
#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       (25)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_HS_CH1_GPIO       (26)
#define LEDC_HS_CH1_CHANNEL    LEDC_CHANNEL_1

//GPIO paraméterek
#define gomb1 15
#define GPIO_INPUT_PIN_SEL ((1ULL<<gomb1))

#define watt 16

uint32_t szint;




//kimeneti áram mérés konfiguráció
static esp_adc_cal_characteristics_t *adc_iout;
static const adc_channel_t adc_iout_channel = ADC_CHANNEL_6;     //GPIO34
static const adc_atten_t adc_iout_atten = ADC_ATTEN_DB_0;
static const adc_unit_t adc_iout_unit = ADC_UNIT_1;
//kimeneti fesz mérés konfiguráció
static esp_adc_cal_characteristics_t *adc_vout;
static const adc_channel_t adc_vout_channel = ADC_CHANNEL_7;     //GPIO35
static const adc_atten_t adc_vout_atten = ADC_ATTEN_DB_0;
static const adc_unit_t adc_vout_unit = ADC_UNIT_1;
//akksifesz mérés konfiguráció
static esp_adc_cal_characteristics_t *adc_vbat;
static const adc_channel_t adc_vbat_channel = ADC_CHANNEL_4;     //GPIO32
static const adc_atten_t adc_vbat_atten = ADC_ATTEN_DB_0;
static const adc_unit_t adc_vbat_unit = ADC_UNIT_1;


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

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
	if(val_type == ESP_ADC_CAL_VAL_EFUSE_TP){
		printf("Characterised using two point value \n");}
	else if(val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
	printf("Characterised using eFuse Vref\n");}
	else {printf("Characterized using default vref\n");}

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
static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    BaseType_t gpio_num = (BaseType_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

//GOMB kiolvasása
void gomb(void *pvParameter)
{
    while(1){
        szint = gpio_get_level(gomb1);
        if(szint==0){
            printf("\n a gomb meg van nyomva");
        }
        else
        {
            printf("\n a gomb nincs megnyomva");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }


vTaskDelete(NULL);
}
//ADC karakterizáció
void adc_charac ()
{
    //Iout karakterizáció
   	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(adc_iout_channel, adc_iout_atten);
	adc_iout = calloc(1, sizeof(esp_adc_cal_characteristics_t));
	esp_adc_cal_value_t iout_val_type = esp_adc_cal_characterize(adc_iout_unit, adc_iout_atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_iout);
	print_char_val_type(iout_val_type);
    //Vout karakterizáció
	adc1_config_channel_atten(adc_vout_channel, adc_vout_atten);
	adc_vout = calloc(1, sizeof(esp_adc_cal_characteristics_t));
	esp_adc_cal_value_t vout_val_type = esp_adc_cal_characterize(adc_vout_unit, adc_vout_atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_vout);
	print_char_val_type(vout_val_type);  
    //Vbat karakterizáció
	adc1_config_channel_atten(adc_vbat_channel, adc_vbat_atten);
	adc_vbat = calloc(1, sizeof(esp_adc_cal_characteristics_t));
	esp_adc_cal_value_t vbat_val_type = esp_adc_cal_characterize(adc_vbat_unit, adc_vbat_atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_vbat);
	print_char_val_type(vbat_val_type);    
  
}

//pwm timer konfiguráció
 ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_7_BIT,
    .freq_hz = 500000,
    .speed_mode = LEDC_HS_MODE,
    .timer_num = LEDC_HS_TIMER
};
//változók deklarálása a méréshez/szabályozáshoz
uint32_t iout_raw;
uint32_t vout_raw;
uint32_t vbat_raw;
float iout;
float vout;
float vbat;
float Rload;
float Voutcalc;
float dutypercent;

// ADC kiolvasása task
void adc_read_task(void *pvParameter)
{
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t buck_pwm = {
        .channel    = LEDC_HS_CH0_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_CH0_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    };
    ledc_channel_config_t boost_pwm = {
        .channel    = LEDC_HS_CH1_CHANNEL,
        .duty       = 0,
        .gpio_num   = LEDC_HS_CH1_GPIO,
        .speed_mode = LEDC_HS_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_HS_TIMER
    };

        ledc_channel_config(&buck_pwm);
        ledc_channel_config(&boost_pwm);


     while (1) {
        //Multisampling
        for (int i = 0; i < NO_OF_SAMPLES; i++)
	   {
            iout_raw += adc1_get_raw((adc1_channel_t)adc_iout_channel);
            vout_raw += adc1_get_raw((adc1_channel_t)adc_vout_channel);
            vbat_raw += adc1_get_raw((adc1_channel_t)adc_vbat_channel);
       };
        iout_raw /= NO_OF_SAMPLES;
        vout_raw /= NO_OF_SAMPLES;
        vbat_raw /= NO_OF_SAMPLES;

        iout = (22/4095)*iout_raw;
        vout = (13.322/4095)*vout_raw;
        vbat = (4.2/4095)*vbat_raw;
        Rload = vout/iout;
        Voutcalc = sqrt(watt)*Rload;
        if (vbat <= Voutcalc )
        {
        dutypercent = vbat/Voutcalc;
        ledc_set_duty(buck_pwm.speed_mode, buck_pwm.channel,dutypercent*127);
        ledc_update_duty(buck_pwm.speed_mode, buck_pwm.channel);
        ledc_set_duty(boost_pwm.speed_mode, boost_pwm.channel,0);
        ledc_update_duty(boost_pwm.speed_mode, boost_pwm.channel); 
        }
        else
        {
        dutypercent = 1-(vbat/Voutcalc);
        ledc_set_duty(boost_pwm.speed_mode, boost_pwm.channel,dutypercent*127);
        ledc_update_duty(boost_pwm.speed_mode, boost_pwm.channel);
        ledc_set_duty(buck_pwm.speed_mode, buck_pwm.channel,127);
        ledc_update_duty(buck_pwm.speed_mode, buck_pwm.channel); 
        }

        vTaskDelay(pdMS_TO_TICKS(10));
	
}
        vTaskDelete(NULL);
}


void app_main(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();
    //Characterize ADC
    adc_charac();
    //configure GPIO
    gomb_init();

    xTaskCreate(gomb, "gomb_kiolvasas", 2048, NULL, 4, NULL);    
    xTaskCreate(adc_read_task, "adc_read_task", 2048, NULL, 4, NULL);
   // vTaskStartScheduler();
	while(1)
{
    vTaskDelay(pdMS_TO_TICKS(10));
}
}


