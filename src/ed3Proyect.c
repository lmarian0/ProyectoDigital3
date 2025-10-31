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

volatile uint32_t adc_value = 0;
static volatile uint8_t adc_ready = 0;   // FLAG para enviar desde main
static char uart_buf[32];

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

        if(adc_value > 2048){
            LPC_GPIO0->FIOCLR = (1 << 22); // LED ON
        } else {
            LPC_GPIO0->FIOSET = (1 << 22); // LED OFF
        }

        adc_ready = 1;
        TIM_ClearIntPending(LPC_TIM0,TIM_MR0_INT);
    }
}

void config_LED(void){
    PINSEL_CFG_Type p;
    p.Portnum = 0; p.Pinnum = 22; p.Funcnum = 0; p.Pinmode = 1; p.OpenDrain = 0;
    PINSEL_ConfigPin(&p);
    LPC_GPIO0->FIODIR |= (1 << 22);
    LPC_GPIO0->FIOSET |= (1 << 22);
}

void config_ADC(void){
    PINSEL_CFG_Type p;
    p.Portnum = 0; p.Pinnum = 2; p.Funcnum = 2; p.Pinmode = 1; p.OpenDrain = 0;
    PINSEL_ConfigPin(&p);

    ADC_Init(LPC_ADC, RATE_ADC);
    ADC_ChannelCmd(LPC_ADC, CHANNEL_ADC, ENABLE);
    ADC_BurstCmd(LPC_ADC, DISABLE);
}

static void uart3_send_dma(const char* buf, uint32_t len){
    // DMA: Memoria -> UART3 TX (ASCII)
    GPDMA_Channel_CFG_Type cfg;
    cfg.ChannelNum    = 0;
    cfg.TransferSize  = len;
    cfg.TransferWidth = GPDMA_WIDTH_BYTE;
    cfg.SrcMemAddr    = (uint32_t)buf;
    cfg.DstMemAddr    = 0;                      // no usado en M2P
    cfg.TransferType  = GPDMA_TRANSFERTYPE_M2P; // Mem -> Perif
    cfg.SrcConn       = 0;
    cfg.DstConn       = GPDMA_CONN_UART3_Tx;    // handshake UART3 TX
    cfg.DMALLI        = 0;

    GPDMA_Setup(&cfg);
    GPDMA_ChannelCmd(0, ENABLE);

    // Esperar fin (simple)
    while (!GPDMA_IntGetStatus(GPDMA_STAT_INTTC, 0) && !GPDMA_IntGetStatus(GPDMA_STAT_INTERR, 0));
    GPDMA_ClearIntPending(GPDMA_STATCLR_INTTC, 0);
    GPDMA_ClearIntPending(GPDMA_STATCLR_INTERR, 0);
}

void config_Uart(void){
    PINSEL_CFG_Type p;
    // TXD3 P0.0
    p.Portnum = 0; p.Pinnum = 0; p.Funcnum = 2; p.Pinmode = PINSEL_PINMODE_TRISTATE; p.OpenDrain = 0;
    PINSEL_ConfigPin(&p);
    // RXD3 P0.1 (opcional)
    p.Pinnum = 1; p.Funcnum = 2;
    PINSEL_ConfigPin(&p);

    UART_CFG_Type uc;
    UART_ConfigStructInit(&uc);
    uc.Baud_rate = 9600; // Asegura 9600
    UART_Init(LPC_UART3, &uc);

    UART_FIFO_CFG_Type fifo;
    UART_FIFOConfigStructInit(&fifo);
    fifo.FIFO_DMAMode = ENABLE; // habilita DMA en FIFO
    UART_FIFOConfig(LPC_UART3, &fifo);

    UART_IrDACmd(LPC_UART3, ENABLE); // habilita solicitud DMA TX
    UART_TxCmd(LPC_UART3, ENABLE);
}

void config_DMA(void){
    GPDMA_Init(); // solo una vez
}

void config_TIMER(void){
    TIM_TIMERCFG_Type t;
    TIM_MATCHCFG_Type m;
    t.PrescaleOption = TIM_PRESCALE_TICKVAL;
    t.PrescaleValue  = prescale_value;
    TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &t);

    m.MatchChannel = MATCH_CHANNEL;
    m.IntOnMatch = ENABLE;
    m.ResetOnMatch = ENABLE;
    m.StopOnMatch = DISABLE;
    m.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    m.MatchValue = match_value; // 5s
    TIM_ConfigMatch(LPC_TIM0, &m);

    TIM_ResetCounter(LPC_TIM0);
    TIM_Cmd(LPC_TIM0, ENABLE);
    TIM_ClearIntPending(LPC_TIM0,TIM_MR0_INT);
    NVIC_SetPriority(TIMER0_IRQn, 1);
    NVIC_EnableIRQ(TIMER0_IRQn);
}

int main(void){
    config_LED();
    config_ADC();
    config_Uart();
    config_DMA();
    config_TIMER();

    while(1){
        if (adc_ready){
            adc_ready = 0;
            int n = sprintf(uart_buf, "ADC=%lu\r\n", (unsigned long)adc_value);
            uart3_send_dma(uart_buf, (uint32_t)n);
        }
    }
    return 0;
}