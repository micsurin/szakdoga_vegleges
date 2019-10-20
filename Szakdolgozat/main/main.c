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
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
//ADC paraméterek
#define DEFAULT_VREF    1100        //Referencia feszültség
#define NO_OF_SAMPLES   64          //Többszörös mintavételezés

//PWM és PWM timer paraméterek
#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       (25)
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_HS_CH1_GPIO       (26)
#define LEDC_HS_CH1_CHANNEL    LEDC_CHANNEL_1

//GPIO paraméterek definiálása
#define USB_state 13
#define CHRGSTAT 4
#define en_driver 23
#define en_3v3 22
#define en_12v 21
#define tuzgomb 15
#define gomb1 12
#define gomb2 14
#define GPIO_INPUT_PIN_SEL ((1ULL<<gomb1)|(1ULL<<gomb2)|(1ULL<<tuzgomb)|(1ULL<<USB_state)|(1ULL<<CHRGSTAT))
#define GPIO_OUTPUT_PIN_SEL ((1ULL<<en_driver)|(1ULL<<en_3v3)|(1ULL<<en_12v))
//Interrupt paraméter
#define ESP_INTR_FLAG_DEFAULT 0
//Timer paraméterek
#define TIMER_DIVIDER         2 //80MHz órajel leosztása
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)

//flagek és számlálók az interrupt kezeléséhez
volatile int bekapcs = 0;
volatile int fire = 0;
volatile int fired = 0;
volatile int timer_run = 0;
volatile int btnpress = 1;
//
int usb_bedugva = 0;
int chargestate;

//külső hivatkozás az RGB vezérlő programra
extern void rgb_control(void *pvParameter);

//Timer struktúra
typedef struct {
    int type;
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;
//64 bites integer az időzítő értékének fenntartva
uint64_t task_counter_value;


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
//thermisztor mérés konfiguráció
static esp_adc_cal_characteristics_t *adc_therm;
static const adc_channel_t adc_therm_channel = ADC_CHANNEL_5;     //GPIO33
static const adc_atten_t adc_therm_atten = ADC_ATTEN_DB_0;
static const adc_unit_t adc_therm_unit = ADC_UNIT_1;

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
//időzítő inicializálása
static void timer_inicializalas()
{
    
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_DOWN;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_DIS;
    config.auto_reload = TIMER_AUTORELOAD_DIS;

    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x7270E00ULL);
}
//Debug, timer állapot kiíratása
static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
    printf("Time   : %.8f s\n", (double) counter_value / TIMER_SCALE);
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
    gpio_pullup_dis(USB_state);
    gpio_pulldown_en(USB_state);
    gpio_set_intr_type(tuzgomb, GPIO_INTR_ANYEDGE);
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
    //Thermisztor karakterizáció
    adc1_config_channel_atten(adc_therm_channel, adc_therm_atten);
	adc_therm = calloc(1, sizeof(esp_adc_cal_characteristics_t));
	esp_adc_cal_value_t therm_val_type = esp_adc_cal_characterize(adc_therm_unit, adc_therm_atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_therm);
	print_char_val_type(therm_val_type);
}

//tovabbi az input interrupthoz a gyorsabb reakcio erdekeben
static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    BaseType_t gpio_num = (BaseType_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}


uint64_t task_counter_value;

//időzítő számlálójának túlcsordulás lekezelése
void timer_control (void *pvParameter){
    while (1)
    {
        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &task_counter_value);
        if (task_counter_value > 0xE4E1C00ULL)
        {
            timer_pause(TIMER_GROUP_0, TIMER_0);
            timer_run=0;
        }
 /*       else
        {
            timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x7270E00ULL);
            timer_start(TIMER_GROUP_0, TIMER_0);
        }*/
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
//időzítő indítása függvény
void timer_inditas (){
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x7270E00ULL);
    timer_start(TIMER_GROUP_0, TIMER_0);
    timer_run=1;
    }
//Interrupt kiértékelése a tűzgombról
static void interrupt_kiertekeles(void* arg)
{
    int io_num;
    while(1) 
    {   timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &task_counter_value);
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) 
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
            
            if(io_num == tuzgomb)
            {
                    if(gpio_get_level(tuzgomb))
                        
                            fire = 0;
                        
                    else{
                        
                        fire = 1;
                        printf("\nallapotok: btnpress=%d, timer_run=%d, task_counter_value=%lld\n, ami nagyobb mint %lld",btnpress,timer_run,task_counter_value,0x7270E00ULL);
                        if (timer_run==0/* && task_counter_value == 0x7270E00ULL*/ ) //nem fut a timer, nincs bekapcsolva
                        {
                            timer_inditas();
                            btnpress++;
                            printf("timer_inditos_szakaszban vagyok\n");
                        }
                        if(timer_run==1 && bekapcs==0)
                        {
                            btnpress++;   //fut a timer de nem tudjuk be kell-e kapcsolni
                            if(btnpress == 5)
                            {
                                bekapcs=1;
                                timer_pause(TIMER_GROUP_0, TIMER_0);
                                timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x7270E00ULL);
                                btnpress = 1;
                            }
                            printf("a gomb leptetos szakaszban vagyok\n");
                            printf("a gombnyomasok erteke %d\n",btnpress);
                        } 
                        if(timer_run==1 && bekapcs==1)
                        {
                            btnpress++;   //fut a timer de nem tudjuk be kell-e kapcsolni
                            if(btnpress == 5)
                            {
                                bekapcs=0;
                                timer_pause(TIMER_GROUP_0, TIMER_0);
                                timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x7270E00ULL);
                                btnpress = 1;
                            }
                            printf("a gomb leptetos szakaszban vagyok\n");
                            printf("a gombnyomasok erteke %d\n",btnpress);
                        } 
                        if (timer_run==0 && task_counter_value != 0x7270E00ULL)  //lejárt a timer meglessük mizu
                        {
                            printf("tulcsordult a timer, a gombnyomasok erteke %d",btnpress);
                            timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x7270E00ULL);
                            btnpress = 1;
                            
                        }
                    }
            }
        }
    }
}    
// a teljesítményszabályozó gombok működéséhez szükséges változók
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
        }
        if(szint_minusz==0){
        printf("\n a gomb meg van nyomva");
        if(watt>0) watt--;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
vTaskDelete(NULL);
}

//pwm timer konfiguráció
 ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_7_BIT,
    .freq_hz = 500000,
    .speed_mode = LEDC_HS_MODE,
    .timer_num = LEDC_HS_TIMER
};
//interrupt letiltása flag
volatile int int_disable = 0;
int therm_raw;
//engedélyező kimenetek kezelése
void enable_outputs (void*pvParameter)
{
    while (1)
    {
        usb_bedugva = gpio_get_level(USB_state);
        chargestate = gpio_get_level(CHRGSTAT);
        for (int i = 0; i < NO_OF_SAMPLES; i++)
	   {
            therm_raw += adc1_get_raw((adc1_channel_t)adc_therm_channel);
       };
        therm_raw /= NO_OF_SAMPLES;
        if (therm_raw >= 3723)
        {
            gpio_set_level(en_driver, 0);
            gpio_set_level(en_3v3, 0);
            gpio_set_level(en_12v, 0);
            int_disable = 1;
        }
        else
        {
            if(bekapcs == 1)
            {
                gpio_set_level(en_driver, 1);
                gpio_set_level(en_3v3, 1);
                gpio_set_level(en_12v, 1);
                int_disable = 0;
            }
            if(bekapcs == 0)
            {
                if (gpio_get_level(USB_state) > 0)
                {
                    gpio_set_level(en_driver, 0);
                    gpio_set_level(en_3v3, 1);
                    gpio_set_level(en_12v, 0);
                    int_disable = 1;
                }
                else
                {
                    gpio_set_level(en_driver, 0);
                    gpio_set_level(en_3v3, 0);
                    gpio_set_level(en_12v, 0);
                    int_disable = 0;
                }
            

            }
        }   
    vTaskDelay(pdMS_TO_TICKS(10));
    }
    
}



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

// Teljesítményszabályozás megvalósítása
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
    //timer beállítása, timer túlcsordulásának lekezelése
    timer_inicializalas();
    xTaskCreate(timer_control, "timer_control", 2048, NULL, 4, NULL);
    //interrupt setup
    gpio_evt_queue = xQueueCreate(10, sizeof(BaseType_t));
    xTaskCreatePinnedToCore(interrupt_kiertekeles, "vEval_programme_task", 2048, NULL, 10, NULL, 1);
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(tuzgomb, gpio_isr_handler, (void*) tuzgomb); //interrupt csatolása a tűzgombhoz

    TaskHandle_t xHandle = NULL;
    xTaskCreate(telj_gomb, "gomb_kiolvasas", 2048, NULL, 4, NULL); 

    xTaskCreatePinnedToCore(enable_outputs, "kimenetek engedelyezese", 2048, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(rgb_control, "rgb vezerles task", 2048, NULL, 4, NULL, 0);

	while(1)
{
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &task_counter_value); 
    print_timer_counter(task_counter_value);
    //interrupt kikapcsolása
    if (int_disable)
    {
        gpio_intr_disable(tuzgomb);

    }
    else
    {
        gpio_intr_enable(tuzgomb);
    }
       
    //interrupt hatására a teljesítmény szabályozás task létrehozása, majd törlése
    if(fire){
    xTaskCreatePinnedToCore(telj_szabalyozas, "adc_read_task", 2048, NULL, 4, &xHandle, 1);
    fired=1;
    }
    if(!fire && fired==1)
    {
         vTaskDelete( xHandle );
         fired=0;
    }
    printf("%d\n", bekapcs);
    vTaskDelay(pdMS_TO_TICKS(100));

}
}


