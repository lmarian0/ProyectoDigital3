/******************************************************************************
                           TRABAJO FINAL EDIII

                                INTEGRANTES:

                        - CASTILLA PABLO
                        - RIVERA LUIS MARIANO
                        - MOHAMMAD CABREJOS SAQIB DANIEL
*******************************************************************************/
#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif


#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_nvic.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"

#define RATE_ADC 200000
#define CHANNEL_ADC 0
#define PRESCALE_VALUE 1000000
#define TIME_MATCH 5
#define MATCH_CHANNEL 1
#define DONE 1
#define OVERRUN 0
#define PORT0 0
#define PIN0 (1<<0)
#define OUTPUT 1

volatile uint16_t adc_value; //Aqui se guardara el valor de la conversion
volatile uint32_t Channel0_error;


void config_ADC(void);
void config_TIMER(void);
void configLed(void);
void delay(void);

void configLed(void){
	PINSEL_CFG_Type led;
    led.Portnum = PINSEL_PORT_0;
    led.Pinnum = PINSEL_PIN_0;
    led.Funcnum = PINSEL_FUNC_0;
    led.Pinmode = PINSEL_PINMODE_TRISTATE;
    led.OpenDrain = PINSEL_PINMODE_NORMAL;
    PINSEL_ConfigPin(&led);
    GPIO_SetDir(PORT0, PIN0, OUTPUT);
}

void config_ADC(void){

    //Se configura el ADC0 para la entrada
    PINSEL_CFG_Type pinsel_adc;
    pinsel_adc.Portnum = PINSEL_PORT_0;
    pinsel_adc.Pinnum = PINSEL_PIN_23;
    pinsel_adc.Funcnum = PINSEL_FUNC_1;
    pinsel_adc.Pinmode = PINSEL_PINMODE_TRISTATE;
    pinsel_adc.OpenDrain = PINSEL_PINMODE_NORMAL;
    PINSEL_ConfigPin(&pinsel_adc);

    NVIC_DisableIRQ(ADC_IRQn);
    NVIC_SetPriority(ADC_IRQn, 0); //Seteamos la interrupcion del ADC con maxima prioridad
    //Usamos 200Khz para el ADC, que interrumpa cuando termine
    //Y que inicie la conversion cada que haya MATCH
    ADC_Init(LPC_ADC, RATE_ADC);
    ADC_IntConfig(LPC_ADC, ADC_ADINTEN0, ENABLE);
    ADC_ChannelCmd(LPC_ADC, CHANNEL_ADC, ENABLE);
    ADC_StartCmd(LPC_ADC, ADC_START_ON_MAT01); //Match 1 del Timer 0

    NVIC_EnableIRQ(ADC_IRQn);
}

void config_TIMER(void){
    //Configuramos el timer
    TIM_TIMERCFG_Type struct_timer;
    struct_timer.PrescaleOption = TIM_PRESCALE_USVAL; //Valor del prescaler en microsegundos
    struct_timer.PrescaleValue = PRESCALE_VALUE;

    //Inicializo el Timer 0
    TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &struct_timer);

    //Configuramos el match para que haga match cada 5 segundos
    TIM_MATCHCFG_Type struct_match;
    struct_match.MatchChannel = MATCH_CHANNEL;
    struct_match.IntOnMatch = DISABLE;
    struct_match.StopOnMatch = DISABLE;
    struct_match.ResetOnMatch = ENABLE;
    struct_match.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    struct_match.MatchValue = TIME_MATCH;

    //Inicializo el Match 1 del Timer
    TIM_ConfigMatch(LPC_TIM0, &struct_match);

    //Habilito el Timer0
    TIM_Cmd(LPC_TIM0, ENABLE);
}

void ADC_IRQHandler(void){
    //Primero verificamos que la conversion haya terminado
    if(ADC_ChannelGetStatus(LPC_ADC, CHANNEL_ADC, DONE)){
        volatile uint16_t aux = ADC_ChannelGetData(LPC_ADC, CHANNEL_ADC); //12 bits de 15:4 result segun LPC_ADC->ADDR0
        adc_value = (aux >> 4) & 0xFFF;
        //Desplazo el resultado 4 bits a la derecha-> adc_value = [xxxxadc____value] 11:0
        GPIO_SetValue(PORT0, PIN0);
        delay();
        GPIO_ClearValue(PORT0, PIN0);
    }
    if(ADC_ChannelGetStatus(LPC_ADC, CHANNEL_ADC, OVERRUN)){
        Channel0_error++; //Si hay Overrun que incremente la cantidad de conversiones fallidas
    }
}
void delay(void){
	for(volatile int i=0; i<1000000000;i++);
}

int main(void)
{
    config_TIMER();
    config_ADC();
    while(1){__WFI();}
    return 0;
}
