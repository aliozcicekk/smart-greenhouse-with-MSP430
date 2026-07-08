#include <msp430.h>
#include <stdio.h>
#include <string.h>

#include "ssd1306.h"
#include "i2c.h"

#define DHT11_PIN BIT4
#define PUMP_PIN  BIT0     // P2.0

// ---------------- GLOBAL ----------------

volatile unsigned int ldrValue = 0;
volatile unsigned int soilValue = 0;

volatile unsigned char RH1, RH2, TEMP1, TEMP2, SUM;

char line[32];
char uartBuffer[128];

// 7 GUNLUK SKOR

unsigned char dailyScores[7] = {0};

unsigned int measurementCount = 0;
unsigned int dailyTotal = 0;

unsigned char dailyIndex = 0;

// ---------------- UART ----------------

void UART_Init(void)
{
    P1SEL  |= BIT1 + BIT2;
    P1SEL2 |= BIT1 + BIT2;

    UCA0CTL1 |= UCSWRST;
    UCA0CTL1 |= UCSSEL_2;

    UCA0BR0 = 104;
    UCA0BR1 = 0;

    UCA0MCTL = UCBRS0;

    UCA0CTL1 &= ~UCSWRST;
}

void UART_SendChar(char c)
{
    while(!(IFG2 & UCA0TXIFG));

    UCA0TXBUF = c;
}

void UART_SendString(char *str)
{
    while(*str)
    {
        UART_SendChar(*str++);
    }
}

// ---------------- ADC ----------------

void ADC_Init(void)
{
    ADC10CTL0 = ADC10SHT_2 + ADC10ON;

    ADC10AE0 |= BIT3;
    ADC10AE0 |= BIT4;
}

// -------- LDR --------

unsigned int Read_LDR()
{
    unsigned long total = 0;

    int i;

    ADC10CTL0 &= ~ENC;

    ADC10CTL1 = INCH_3;

    __delay_cycles(5000);

    ADC10CTL0 |= ENC + ADC10SC;

    while(ADC10CTL1 & ADC10BUSY);

    for(i = 0; i < 5; i++)
    {
        ADC10CTL0 &= ~ENC;

        ADC10CTL0 |= ENC + ADC10SC;

        while(ADC10CTL1 & ADC10BUSY);

        total += ADC10MEM;

        __delay_cycles(1000);
    }

    return (unsigned int)(total / 5);
}

// -------- SOIL --------

unsigned int Read_Soil()
{
    unsigned long total = 0;

    int i;

    ADC10CTL0 &= ~ENC;

    ADC10CTL1 = INCH_4;

    __delay_cycles(5000);

    ADC10CTL0 |= ENC + ADC10SC;

    while(ADC10CTL1 & ADC10BUSY);

    for(i = 0; i < 5; i++)
    {
        ADC10CTL0 &= ~ENC;

        ADC10CTL0 |= ENC + ADC10SC;

        while(ADC10CTL1 & ADC10BUSY);

        total += ADC10MEM;

        __delay_cycles(1000);
    }

    return (unsigned int)(total / 5);
}

// ---------------- PWM ----------------

void PWM_Init()
{
    P1DIR |= BIT6;

    P1SEL |= BIT6;

    TA0CCR0 = 1000 - 1;

    TA0CCTL1 = OUTMOD_7;

    TA0CCR1 = 0;

    TA0CTL = TASSEL_2 | MC_1;
}

void Update_PWM(unsigned int ldr)
{
    unsigned int pwm;

    // AYDINLIK = BUYUK
    // KARANLIK = KUCUK

    if(ldr >= 400)
    {
        pwm = 0;
    }
    else
    {
        pwm = (400 - ldr) * 3;

        if(pwm > 1000)
        {
            pwm = 1000;
        }
    }

    TA0CCR1 = pwm;
}

// ---------------- PUMP ----------------

void Pump_Init()
{
    P2DIR |= PUMP_PIN;

    P2OUT &= ~PUMP_PIN;
}

void Pump_Control()
{
    // TOPRAK KURU
    // BIR SONRAKI OLCUME KADAR CALIS

    if(soilValue >= 400)
    {
        P2OUT |= PUMP_PIN;
    }
    else
    {
        P2OUT &= ~PUMP_PIN;
    }
}

// ---------------- TIMER INTERRUPT ----------------

void Timer_Init()
{
    BCSCTL3 |= LFXT1S_2;

    TA1CCTL0 = CCIE;

    TA1CCR0 = 12000;

    TA1CTL = TASSEL_1 | ID_3 | MC_1;
}

// ---------------- DHT11 ----------------

void DHT11_Start(void)
{
    P2DIR |= DHT11_PIN;

    P2OUT &= ~DHT11_PIN;

    __delay_cycles(25000);

    P2OUT |= DHT11_PIN;

    __delay_cycles(30);

    P2DIR &= ~DHT11_PIN;
}

unsigned char DHT11_Response(void)
{
    unsigned int i = 0;

    while(P2IN & DHT11_PIN)
    {
        i++;

        if(i > 10000)
            return 0;
    }

    i = 0;

    while(!(P2IN & DHT11_PIN))
    {
        i++;

        if(i > 10000)
            return 0;
    }

    i = 0;

    while(P2IN & DHT11_PIN)
    {
        i++;

        if(i > 10000)
            return 0;
    }

    return 1;
}

unsigned char DHT11_ReadByte(void)
{
    unsigned char i;

    unsigned char data = 0;

    for(i = 0; i < 8; i++)
    {
        while(!(P2IN & DHT11_PIN));

        __delay_cycles(30);

        if(P2IN & DHT11_PIN)
        {
            data |= (1 << (7 - i));
        }

        while(P2IN & DHT11_PIN);
    }

    return data;
}

// ---------------- SCORE ----------------

unsigned char CalculateScore()
{
    unsigned char score = 0;

    if(TEMP1 >= 15 && TEMP1 <= 30)
    {
        score += 40;
    }

    if(soilValue < 400)
    {
        score += 30;
    }

    // BUYUK = AYDINLIK

    if(ldrValue >= 400)
    {
        score += 30;
    }

    return score;
}

// ---------------- OLED ----------------

void OLED_ShowSensors()
{
    unsigned char score;

    score = CalculateScore();

    ssd1306_clearDisplay();

    sprintf(line, "TEMP:%dC", TEMP1);
    ssd1306_printText(0,0,line);

    sprintf(line, "TOPRAK:%d", soilValue);
    ssd1306_printText(0,2,line);

    sprintf(line, "ISIK:%d", ldrValue);
    ssd1306_printText(0,4,line);

    sprintf(line, "SKOR:%d", score);
    ssd1306_printText(0,6,line);
}

void OLED_ShowAdvice()
{
    ssd1306_clearDisplay();

    ssd1306_printText(0,0,"AKILLI ONERI");

    if(soilValue >= 400)
    {
        ssd1306_printText(0,2,"SULAMA YAP");
    }

    if(ldrValue < 400)
    {
        ssd1306_printText(0,4,"ISIK ARTTIR");
    }

    if(TEMP1 < 15)
    {
        ssd1306_printText(0,6,"ORTAM SOGUK");
    }

    if(TEMP1 > 30)
    {
        ssd1306_printText(0,6,"ORTAM COK SICAK");
    }

    if(soilValue < 400 &&
       ldrValue >= 400 &&
       TEMP1 >= 15 &&
       TEMP1 <= 30)
    {
        ssd1306_printText(0,3,"ORTAM IDEAL");
    }
}

void OLED_ShowGraph()
{
    int i;

    ssd1306_clearDisplay();

    ssd1306_printText(0,0,"7 GUN SKOR");

    for(i = 0; i < 7; i++)
    {
        sprintf(line,"G%d:%d",i+1,dailyScores[i]);

        ssd1306_printText(0,i+1,line);
    }
}

// ---------------- UART ----------------

void Send_Report(unsigned char score)
{
    sprintf(uartBuffer,
    "TEMP:%d\r\nTOPRAK:%d\r\nISIK:%d\r\nSKOR:%d\r\n",
    TEMP1,
    soilValue,
    ldrValue,
    score);

    UART_SendString(uartBuffer);

    if(soilValue >= 400)
    {
        UART_SendString("ONERI:SULAMA GEREKLI\r\n");
    }

    if(ldrValue < 400)
    {
        UART_SendString("ONERI:ORTAM KARANLIK\r\n");
    }

    if(TEMP1 < 15 || TEMP1 > 30)
    {
        UART_SendString("ONERI:SICAKLIK UYGUN DEGIL\r\n");
    }

    UART_SendString("\r\n");
}

// ---------------- TIMER ISR ----------------

#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer_A (void)
{
    unsigned char score;

    // ---------- DHT11 ----------

    DHT11_Start();

    if(DHT11_Response())
    {
        RH1   = DHT11_ReadByte();
        RH2   = DHT11_ReadByte();

        TEMP1 = DHT11_ReadByte();
        TEMP2 = DHT11_ReadByte();

        SUM   = DHT11_ReadByte();
    }

    // ---------- ADC ----------

    // BUYUK = AYDINLIK
    // KUCUK = KARANLIK

    ldrValue = Read_LDR();

    soilValue = Read_Soil();

    // ---------- PWM ----------

    Update_PWM(ldrValue);

    // ---------- PUMP ----------

    Pump_Control();

    // ---------- SCORE ----------

    score = CalculateScore();

    // ---------- 7 GUNLUK SKOR ----------

    dailyTotal += score;

    measurementCount++;

    if(measurementCount >= 3)
    {
        dailyScores[dailyIndex] = dailyTotal / 3;

        dailyIndex++;

        if(dailyIndex >= 7)
        {
            dailyIndex = 0;
        }

        measurementCount = 0;

        dailyTotal = 0;
    }

    // ---------- UART ----------

    Send_Report(score);

    // ---------- OLED 1 ----------

    OLED_ShowSensors();

    __delay_cycles(1000000);

    // ---------- OLED 2 ----------

    OLED_ShowAdvice();

    __delay_cycles(1000000);

    // ---------- OLED 3 ----------

    OLED_ShowGraph();
}

// ---------------- MAIN ----------------

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    BCSCTL1 = CALBC1_1MHZ;
    DCOCTL  = CALDCO_1MHZ;

    UART_Init();

    ADC_Init();

    PWM_Init();

    Pump_Init();

    Timer_Init();

    i2c_init();

    ssd1306_init();

    __enable_interrupt();

    while(1)
    {
        __bis_SR_register(LPM0_bits + GIE);
    }
}
