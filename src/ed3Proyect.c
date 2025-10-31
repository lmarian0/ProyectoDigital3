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
#include "lpc17xx_gpdma.h"
#include "string.h"
#include "stdio.h"


volatile uint32_t adc_value = 0; //Aqui se guardara el valor de la conversion
static const uint8_t CHANNEL_ADC = 7;
static const uint32_t RATE_ADC = 200000; //200Khz
static const uint8_t MATCH_CHANNEL = 0;
static const uint32_t prescale_value = 24;
static const uint32_t match_value = 5000000;


void TIMER0_IRQHandler(void){
    if(TIM_GetIntStatus(LPC_TIM0, TIM_MR0_INT) == SET){

    	ADC_StartCmd(LPC_ADC, ADC_START_NOW);

    	while (!(ADC_ChannelGetStatus(LPC_ADC, ADC_CHANNEL_7, ADC_DATA_DONE)));

    	adc_value = ADC_ChannelGetData(LPC_ADC, ADC_CHANNEL_7);
    	// Ejemplo: LED encendido si potenciómetro > 50%
    	if(adc_value > 2048){
    	    LPC_GPIO0->FIOCLR = (1 << 22); // Enciende LED
    	} else {
    	    LPC_GPIO0->FIOSET = (1 << 22); // Apaga LED
    	}

    	//TIM_Cmd(LPC_TIM0, ENABLE); //inicia el timer
    	TIM_ClearIntPending(LPC_TIM0,TIM_MR0_INT); //Limpio bandera de interrupcion del timer0
    }
}


void config_LED(void){
    //Configuro el pin P0.22 como salida GPIO para el LED
    PINSEL_CFG_Type pinsel_led;
    pinsel_led.Portnum = 0;
    pinsel_led.Pinnum = 22;
    pinsel_led.Funcnum = 0; //GPIO
    pinsel_led.Pinmode = 1;
    pinsel_led.OpenDrain = 0;
    PINSEL_ConfigPin(&pinsel_led);

    //Configuro el pin como salida
    LPC_GPIO0->FIODIR |= (1 << 22);
    LPC_GPIO0->FIOSET |= (1 << 22);
}

void config_DMA(void) {

    GPDMA_Init(); // Inicializa el controlador de DMA

    // Configura el canal de DMA para transferir datos desde el buffer a UART
    GPDMA_Channel_CFG_Type dacuart;
    dacuart.ChannelNum = 0;
    dacuart.SrcMemAddr = 0;
    dacuart.DstMemAddr = 0;
    dacuart.TransferSize = 1;
    dacuart.TransferWidth = GPDMA_WIDTH_BYTE;
    dacuart.TransferType = GPDMA_TRANSFERTYPE_P2P;
    dacuart.SrcConn = GPDMA_CONN_ADC;
    dacuart.DstConn = GPDMA_CONN_UART3_Tx;
    dacuart.DMALLI = 0;

    // Configura
    GPDMA_Setup(&dacuart);
    GPDMA_ChannelCmd(0, ENABLE); // Activa el canal DMA 0
}


void config_Uart(void){
    PINSEL_CFG_Type uart_config;
    uart_config.Portnum = 0;
    uart_config.Pinnum = 0;
    uart_config.Funcnum = 2;
    uart_config.Pinmode = PINSEL_PINMODE_TRISTATE;
    uart_config.OpenDrain = 0;
    PINSEL_ConfigPin(&uart_config);

    UART_CFG_Type uart_cfg;
    UART_ConfigStructInit(&uart_cfg);
    UART_Init(LPC_UART3, &uart_cfg);

    UART_FIFO_CFG_Type UARTFIFOConfigStruct;
    UARTFIFOConfigStruct.FIFO_DMAMode = ENABLE; // Habilita el modo DMA
    UARTFIFOConfigStruct.FIFO_Level = UART_FIFO_TRGLEV0;
    UARTFIFOConfigStruct.FIFO_ResetRxBuf = ENABLE;
    UARTFIFOConfigStruct.FIFO_ResetTxBuf = ENABLE;
    UART_FIFOConfigStructInit(&UARTFIFOConfigStruct);

    UART_FIFOConfig(LPC_UART3, &UARTFIFOConfigStruct);
    UART_TxCmd(LPC_UART3, ENABLE);
}

void config_ADC(void){

    //Se configura el ADC7 para la entrada pin P0.2
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
    //Configuramos el timer
    TIM_TIMERCFG_Type struct_timer;
    TIM_MATCHCFG_Type struct_match;
    struct_timer.PrescaleOption = TIM_PRESCALE_TICKVAL;
    struct_timer.PrescaleValue = prescale_value;

    //Inicializo el Timer 0
    TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &struct_timer);

    //Configuramos el match para que haga match cada 5 segundos

    struct_match.MatchChannel = MATCH_CHANNEL;
    struct_match.IntOnMatch = ENABLE;
    struct_match.ResetOnMatch = ENABLE;
    struct_match.StopOnMatch = DISABLE;
    struct_match.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    struct_match.MatchValue = match_value; //5 segundos

    //Inicializo el Match 0 del Timer
    TIM_ConfigMatch(LPC_TIM0, &struct_match);
    TIM_ResetCounter(LPC_TIM0);
    //Habilito el Timer0
    TIM_Cmd(LPC_TIM0, ENABLE);

    TIM_ClearIntPending(LPC_TIM0,TIM_MR0_INT); //Limpio bandera de interrupcion del timer0
    NVIC_SetPriority(TIMER0_IRQn, 1);
    NVIC_EnableIRQ(TIMER0_IRQn);
}


int main(void)
{
    config_LED();
    config_ADC();
    config_Uart();
    config_DMA();
    config_TIMER();
    while(1){
    }

    return 0;
}