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

#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_nvic.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"
#include "string.h"
#include "stdio.h"
#include <math.h>

#define PI 3.14159265358979

// Valores para el DAC
#define DMA_SIZE		     60
#define SAMPLES_DAC	         60
#define MIN_FREQUENCY_HZ     100      //Minima Frecuencia del DAC
#define MAX_FREQUENCY_HZ     1000    //Maxima Frecuencia del DAC
#define CLK_DAC_MHZ 	     25000000
uint32_t dac_sin[SAMPLES_DAC];

// Variables GLOBALES para DMA
GPDMA_Channel_CFG_Type GPDMACfg;
GPDMA_LLI_Type DMA_LLI_Struct;

// Valores TIMER0 y ADC
volatile uint32_t adc_value = 0;
volatile uint32_t temperatura = 0;
static const uint8_t CHANNEL_ADC = 7;
static const uint32_t RATE_ADC = 200000;
static const uint8_t MATCH_CHANNEL = 0;
static const uint32_t prescale_value = 24;
static const uint32_t match_value = 1000000;  // 1 segundo
static const uint32_t prescale_value1 = 24;
static const uint32_t match_value1 = 10000;   // ~20ms para multiplexación

// Variables para displays 7 segmentos
volatile uint8_t decena_temp = 0;
volatile uint8_t unidad_temp = 0;
volatile uint8_t display_actual = 0;

// Tabla BCD para display 7 segmentos (cátodo común)
const uint32_t number[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

// Prototipos
void uart3_SendADC(uint32_t value);
void update_dac_frequency(uint32_t adc_val);
void sin_Generate(void);
void config_DMA(void);
void config_Display(void);
void update_display(void);
void show_display(void);

void TIMER1_IRQHandler(void){
    if(TIM_GetIntStatus(LPC_TIM1, TIM_MR0_INT) == SET){
        show_display(); // Multiplexación de displays
        TIM_ClearIntPending(LPC_TIM1, TIM_MR0_INT);
    }
}

void TIMER0_IRQHandler(void){
    if(TIM_GetIntStatus(LPC_TIM0, TIM_MR0_INT) == SET){

        ADC_StartCmd(LPC_ADC, ADC_START_NOW);

        while (!(ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL_7, ADC_DATA_DONE)));

        adc_value = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_7);

        // Calcular temperatura (0-99°C)
        temperatura = (adc_value * 330) / 4095; // Conversión simplificada
        if(temperatura > 99) temperatura = 99;   // Limitar a 99°C

        update_display(); // Actualizar valores de displays
        uart3_SendADC(adc_value);
        update_dac_frequency(adc_value);

        // Control LED alarma
        if(temperatura > 55){
            LPC_GPIO0->FIOCLR = (1 << 22);
        } else {
            LPC_GPIO0->FIOSET = (1 << 22);
        }

        TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);
    }
}

void config_LED(void){
    PINSEL_CFG_Type pinsel_led;
    pinsel_led.Portnum = 0;
    pinsel_led.Pinnum = 22;
    pinsel_led.Funcnum = 0;
    pinsel_led.Pinmode = 1;
    pinsel_led.OpenDrain = 0;
    PINSEL_ConfigPin(&pinsel_led);

    LPC_GPIO0->FIODIR |= (1 << 22);
    LPC_GPIO0->FIOSET |= (1 << 22);
}

void config_ADC(void){
    PINSEL_CFG_Type pinsel_adc;
    pinsel_adc.Portnum = 0;
    pinsel_adc.Pinnum = 2;
    pinsel_adc.Funcnum = 2;
    pinsel_adc.Pinmode = 1;
    PINSEL_ConfigPin(&pinsel_adc);

    ADC_Init(LPC_ADC, RATE_ADC);
    ADC_ChannelCmd(LPC_ADC, CHANNEL_ADC, ENABLE);
    ADC_BurstCmd(LPC_ADC, DISABLE);
}

void config_TIMER(void){
    TIM_TIMERCFG_Type struct_timer;
    TIM_MATCHCFG_Type struct_match;
    struct_timer.PrescaleOption = TIM_PRESCALE_TICKVAL;
    struct_timer.PrescaleValue = prescale_value;

    TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &struct_timer);

    struct_match.MatchChannel = MATCH_CHANNEL;
    struct_match.IntOnMatch = ENABLE;
    struct_match.ResetOnMatch = ENABLE;
    struct_match.StopOnMatch = DISABLE;
    struct_match.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    struct_match.MatchValue = match_value;

    TIM_ConfigMatch(LPC_TIM0, &struct_match);
    TIM_ResetCounter(LPC_TIM0);
    TIM_Cmd(LPC_TIM0, ENABLE);

    TIM_ClearIntPending(LPC_TIM0, TIM_MR0_INT);
    NVIC_SetPriority(TIMER0_IRQn, 1);
    NVIC_EnableIRQ(TIMER0_IRQn);
}

void config_TIMER1(void){
    TIM_TIMERCFG_Type struct_timer1;
    TIM_MATCHCFG_Type struct_match1;
    struct_timer1.PrescaleOption = TIM_PRESCALE_TICKVAL;
    struct_timer1.PrescaleValue = prescale_value1;

    TIM_Init(LPC_TIM1, TIM_TIMER_MODE, &struct_timer1);

    struct_match1.MatchChannel = MATCH_CHANNEL;
    struct_match1.IntOnMatch = ENABLE;
    struct_match1.ResetOnMatch = ENABLE;
    struct_match1.StopOnMatch = DISABLE;
    struct_match1.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    struct_match1.MatchValue = match_value1;

    TIM_ConfigMatch(LPC_TIM1, &struct_match1);
    TIM_ResetCounter(LPC_TIM1);
    TIM_Cmd(LPC_TIM1, ENABLE);

    TIM_ClearIntPending(LPC_TIM1, TIM_MR0_INT);
    NVIC_SetPriority(TIMER1_IRQn, 0); // Mayor prioridad para multiplexación
    NVIC_EnableIRQ(TIMER1_IRQn);
}

void config_Uart(){
    PINSEL_CFG_Type uart_config;
    uart_config.Portnum = 0;
    uart_config.Pinnum = 0;
    uart_config.Funcnum = 2;
    uart_config.Pinmode = PINSEL_PINMODE_TRISTATE;
    uart_config.OpenDrain = 0;
    PINSEL_ConfigPin(&uart_config);

    UART_CFG_Type uart_cfg;
    UART_ConfigStructInit(&uart_cfg);
    uart_cfg.Baud_rate = 9600;
    UART_Init(LPC_UART3, &uart_cfg);

    UART_FIFO_CFG_Type uart_fifo;
    UART_FIFOConfigStructInit(&uart_fifo);
    UART_FIFOConfig(LPC_UART3, &uart_fifo);

    UART_TxCmd(LPC_UART3, ENABLE);
}

void sin_Generate(void){
    uint32_t i;
    for(i = 0; i < SAMPLES_DAC; i++) {
        float angle = 2 * PI * i / SAMPLES_DAC;
        float sine_value = sin(angle);
        uint32_t dac_value = (uint32_t)((sine_value + 1) * 511.5);
        dac_sin[i] = dac_value << 6;
    }
}

void config_DAC(void){
    PINSEL_CFG_Type pin_dac;
    pin_dac.Funcnum = 2;
    pin_dac.OpenDrain = 0;
    pin_dac.Pinmode = 0;
    pin_dac.Pinnum = 26;
    pin_dac.Portnum = 0;
    PINSEL_ConfigPin(&pin_dac);

    DAC_CONVERTER_CFG_Type dma_dac_struct;

    sin_Generate();
    config_DMA();

    dma_dac_struct.CNT_ENA = SET;
    dma_dac_struct.DMA_ENA = SET;
    DAC_Init(LPC_DAC);

    uint32_t tmp = (CLK_DAC_MHZ)/((MIN_FREQUENCY_HZ + MAX_FREQUENCY_HZ)/2 * SAMPLES_DAC);
    DAC_SetDMATimeOut(LPC_DAC, tmp);
    DAC_ConfigDAConverterControl(LPC_DAC, &dma_dac_struct);

    GPDMA_ChannelCmd(0, ENABLE);
}

void config_DMA(void){
    DMA_LLI_Struct.SrcAddr = (uint32_t)dac_sin;
    DMA_LLI_Struct.DstAddr = (uint32_t)&(LPC_DAC->DACR);
    DMA_LLI_Struct.NextLLI = (uint32_t)&DMA_LLI_Struct;
    DMA_LLI_Struct.Control = DMA_SIZE | (2<<18) | (2<<21) | (1<<26);

    GPDMA_Init();

    GPDMACfg.ChannelNum = 0;
    GPDMACfg.SrcMemAddr = (uint32_t)(dac_sin);
    GPDMACfg.DstMemAddr = 0;
    GPDMACfg.TransferSize = DMA_SIZE;
    GPDMACfg.TransferWidth = 0;
    GPDMACfg.TransferType = GPDMA_TRANSFERTYPE_M2P;
    GPDMACfg.SrcConn = 0;
    GPDMACfg.DstConn = GPDMA_CONN_DAC;
    GPDMACfg.DMALLI = (uint32_t)&DMA_LLI_Struct;
    GPDMA_Setup(&GPDMACfg);
}

void config_Display(void){
    PINSEL_CFG_Type pinCfg;

    // Configurar P2.0-P2.6 como GPIO (segmentos a-g)
    pinCfg.Portnum = 2;
    pinCfg.Funcnum = 0;
    pinCfg.Pinmode = PINSEL_PINMODE_TRISTATE;
    pinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;

    for(uint8_t i = 0; i <= 6; i++){
        pinCfg.Pinnum = i;
        PINSEL_ConfigPin(&pinCfg);
    }

    // Configurar P0.10 y P0.11 como GPIO (multiplexores)
    pinCfg.Portnum = 0;
    pinCfg.Pinnum = 10;
    PINSEL_ConfigPin(&pinCfg);

    pinCfg.Pinnum = 11;
    PINSEL_ConfigPin(&pinCfg);

    // Configurar como salidas
    GPIO_SetDir(2, 0x7F, 1); // P2.0-P2.6
    GPIO_SetDir(0, (1<<10) | (1<<11), 1); // P0.10-P0.11

    // Apagar displays y segmentos inicialmente
    GPIO_ClearValue(0, (1<<10) | (1<<11)); // Cátodo común: HIGH apaga
    GPIO_ClearValue(2, 0x7F);
}

void update_display(void){
    // Actualizar valores BCD de decenas y unidades
    decena_temp = number[(temperatura / 10) % 10];
    unidad_temp = number[temperatura % 10];
}

void show_display(void){
    // Apagar ambos displays
    GPIO_ClearValue(0, (1<<10) | (1<<11));
    GPIO_ClearValue(2, 0x7F);

    if(display_actual == 0){
        // Mostrar decenas en display conectado a P0.10
        GPIO_SetValue(2, decena_temp);
        GPIO_SetValue(0, (1<<10));
    } else {
        // Mostrar unidades en display conectado a P0.11
        GPIO_SetValue(2, unidad_temp);
        GPIO_SetValue(0, (1<<11));
    }

    display_actual = !display_actual; // Alternar displays
}

void update_dac_frequency(uint32_t adc_val){
    uint32_t frequency = MIN_FREQUENCY_HZ + ((adc_val * (MAX_FREQUENCY_HZ - MIN_FREQUENCY_HZ)) / 4095);
    uint32_t timeout = (CLK_DAC_MHZ) / (frequency * SAMPLES_DAC);
    DAC_SetDMATimeOut(LPC_DAC, timeout);
}

void uart3_SendADC(uint32_t value){
    uint32_t frequency = MIN_FREQUENCY_HZ + ((value * (MAX_FREQUENCY_HZ - MIN_FREQUENCY_HZ)) / 4095);

    char buf[64];
    int n = sprintf(buf, "ADC=%lu, Temp=%luC, Freq=%luHz\r\n",
                    (unsigned long)value, (unsigned long)temperatura, (unsigned long)frequency);
    UART_Send(LPC_UART3, (uint8_t*)buf, (uint32_t)n, BLOCKING);
}

int main(void){
    config_LED();
    config_ADC();
    config_Uart();
    config_Display(); // Configurar displays ANTES de los timers
    config_DAC();
    config_TIMER();   // Timer0: ADC cada 1 seg
    config_TIMER1();  // Timer1: Multiplexación cada ~20ms

    while(1){
        // El sistema funciona por interrupciones
    }

    return 0;
}