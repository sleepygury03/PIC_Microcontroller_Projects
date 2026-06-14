/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  1x Potentiometer (or Analog Voltage Source on RA0/AN0), 
 * 4x On-board LEDs (RC0-RC3) on Demo Board
 * DESCRIPTION:
 * Demonstrates basic Analog-to-Digital Converter (ADC) configuration. 
 * Samples an analog voltage channel (AN0/RA0) using a left-justified setup.
 * By shifting the 8-bit ADRESH register up by 2 bits, a fast 10-bit approximation 
 * is made, skipping the lowest 2 bits (millivolt noise). If the value exceeds 
 * 512 (~2.5V), the LED on RC0 is turned on.
 * ========================================================================== */

#include <xc.h>

#pragma config FOSC = INTRCIO   // Internal oscillator, I/O function on RA4/RA5
#pragma config WDTE = OFF       // Watchdog Timer Disabled
#pragma config PWRTE = ON       // Power-up Timer Enabled
#pragma config MCLRE = OFF      // MCLR pin function is digital input
#pragma config CP = OFF         // Code Protection Disabled
#pragma config CPD = OFF        // Data Code Protection Disabled
#pragma config BOREN = OFF      // Brown-out Reset Disabled
#pragma config IESO = OFF       // Internal External Switchover Disabled
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Disabled

#define _XTAL_FREQ 8000000     // 8 MHz system clock for __delay_ms()

// PORTC LED Bitmasks
#define led_1 0x01 // RC0
#define led_2 0x02 // RC1
#define led_3 0x04 // RC2
#define led_4 0x08 // RC3

// Analog Input Mask
#define adc_pin 0x01 // RA0 (AN0)

unsigned char LEDS[] = {led_1, led_2, led_3, led_4};
unsigned char counter = 0;

unsigned int adc_value; // 16-bit variable to hold the 10-bit constructed result

void main(void)
{
    OSCCON = 0x70;      // Set internal oscillator to 8 MHz

    ANSEL = 0x01;       // Configure RA0/AN0 as Analog input, rest as Digital
    ANSELH = 0;
    
    CM1CON0 = 0x00;     // Disable Comparator 1
    CM2CON0 = 0x00;     // Disable Comparator 2
    
    TRISA |= adc_pin;   // Set RA0 as an input

    ADCON0 = 0x01;      // Turn on ADC module, select channel AN0, Left-Justified
    ADCON1 = 0x60;      // Set ADC conversion clock to Fosc/64 (safe for 8 MHz)
    
    TRISC &= ~(led_1 | led_2 | led_3 | led_4);  // Set RC0-RC3 as outputs
    PORTC = 0x00;       // Turn off all LEDs initially
    
    while (1)
    {
        ADCON0 |= 0x02; // Start conversion by setting the GO/DONE bit
        
        // Poll the GO/DONE bit; wait here until hardware clears it upon completion
        while(ADCON0 & 0x02)
        {
        }
        
        // Read Left-Justified high byte and shift to upscale to 10-bit range
        adc_value = ADRESH;
        adc_value = adc_value << 2; 
        
        // Threshold check (512 out of 1023 corresponds to roughly VDD/2)
        if (adc_value > 512) {
            PORTC |= led_1;
        } else {
            PORTC = 0x00;
        }
        
        __delay_ms(100); // Intermittent polling delay
    }
}