#include <stdio.h>
#include <math.h>
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
#define en_driver 23
#define en_3v3 22
#define en_12v 21
#define tuzgomb 15
#define gomb1 13
#define gomb2 14
#define GPIO_INPUT_PIN_SEL ((1ULL<<gomb1)|(1ULL<<gomb2)|(1ULL<<tuzgomb))
#define GPIO_OUTPUT_PIN_SEL ((1ULL<<en_driver)|(1ULL<<en_3v3)|(1ULL<<en_12v))
//Interrupt paraméter
#define ESP_INTR_FLAG_DEFAULT 0


volatile int bekapcs = 0;
volatile int fire = 0;
volatile int fired = 0;


extern void rgb_control(void *pvParameter);




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
static volatile int varakoz = 500;
static volatile int folyamatos = 0;

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
//engedélyező jelek inicializálása
void en_init(void){
    gpio_config_t en_config = {
        .intr_type = GPIO_PIN_INTR_DISABLE,
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0
    };
    gpio_config(&en_config);

}
//Gombok inicializálása
void gomb_init(void){
  	 gpio_config_t gomb_config = {
        .intr_type = GPIO_PIN_INTR_DISABLE,
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1
    };
    gpio_config(&gomb_config);
    gpio_set_intr_type(tuzgomb, GPIO_INTR_ANYEDGE);
}

//tovabbi az input interrupthoz a gyorsabb reakcio erdekeben
static xQueueHandle gpio_evt_queue = NULL;                                  //ez kell

static void IRAM_ATTR gpio_isr_handler(void* arg)                           //ez kell innentől
{
    BaseType_t gpio_num = (BaseType_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}                                                           

static void interrupt_kiertekeles(void* arg)
{
    int io_num;
    while(1) 
    {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) 
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
            
            if(io_num == tuzgomb)
            {
                    if(gpio_get_level(tuzgomb))
                        fire = 0;
                    else
                        fire = 1;

            }
        }
    }
}    
// a teljesítményszabályozó gombok működése
uint32_t watt = 16;
uint32_t szint_plusz;
uint32_t szint_minusz;
void telj_gomb(void *pvParameter)
{

    while(1){
        szint_plusz = gpio_get_level(gomb1);
        szint_minusz = gpio_get_level(gomb2);
        if(szint_plusz==0){
            printf("\n a gomb meg van nyomva");
            if(watt<75) watt++;
//            if(folyamatos>=0 && folyamatos < 10) folyamatos++;
        }
        if(szint_minusz==0){
        printf("\n a gomb meg van nyomva");
        if(watt>0) watt--;
//        if(folyamatos>=0 && folyamatos < 10) folyamatos++;
        }
//        if(szint_minusz && szint_plusz) folyamatos--;
 //       if(folyamatos == 10 ) vTaskDelay(pdMS_TO_TICKS(20));
 //       if(folyamatos <= 0 ) vTaskDelay(pdMS_TO_TICKS(500));
        vTaskDelay(pdMS_TO_TICKS(100));
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
void telj_szabalyozas(void *pvParameter)
{
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t buck_pwm = {
        .channel    = LEDC_HS_CH0_CHANNEL,
        .duty       = 42, //kezdeti kitöltés, legyen alacsony fesz, hogy ne robbanjon fel ~1,4V
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
        if (vbat >= Voutcalc )
        {
        dutypercent = Voutcalc/vbat;
        ledc_set_duty(buck_pwm.speed_mode, buck_pwm.channel,dutypercent*127.0);
        ledc_update_duty(buck_pwm.speed_mode, buck_pwm.channel);
        ledc_set_duty(boost_pwm.speed_mode, boost_pwm.channel,0);
        ledc_update_duty(boost_pwm.speed_mode, boost_pwm.channel); 
        }
        else
        {
        dutypercent = 1-(vbat/Voutcalc);
        ledc_set_duty(boost_pwm.speed_mode, boost_pwm.channel,dutypercent*127.0);
        ledc_update_duty(boost_pwm.speed_mode, boost_pwm.channel);
        ledc_set_duty(buck_pwm.speed_mode, buck_pwm.channel,127);
        ledc_update_duty(buck_pwm.speed_mode, buck_pwm.channel); 
        }
        vTaskDelay(pdMS_TO_TICKS(10));
	
}
        //vTaskDelete(NULL);
}


void app_main(void)
{
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();
    //Characterize ADC
    adc_charac();
    //configure GPIO
    gomb_init();
    en_init();
    //interrupt setup
    gpio_evt_queue = xQueueCreate(10, sizeof(BaseType_t));
    xTaskCreate(interrupt_kiertekeles, "vEval_programme_task", 2048, NULL, 10, NULL);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(tuzgomb, gpio_isr_handler, (void*) tuzgomb);

    TaskHandle_t xHandle = NULL;
    xTaskCreate(telj_gomb, "gomb_kiolvasas", 2048, NULL, 4, NULL); 
    
    xTaskCreate(rgb_control, "rgb vezerles task", 2048, NULL, 4, NULL);
    bekapcs = 1;
   // vTaskStartScheduler();
	while(1)
{
    if(fire){
    xTaskCreate(telj_szabalyozas, "adc_read_task", 2048, NULL, 4, &xHandle);
    fired=1;
    }
    if(!fire && fired==1)
    {
         vTaskDelete( xHandle );
         fired=0;
    }
    printf("%d\n", fired);
    vTaskDelay(pdMS_TO_TICKS(100));

}
}


