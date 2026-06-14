/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  4x On-board LEDs (DS1 - DS4) on Low Pin Count Demo Board
 * DESCRIPTION:
 * A simple LED chaser project. Sequentially turns on 4 LEDs connected to 
 * PORTC (RC0-RC3) one by one, and then sequentially turns them off in reverse
 * order using blocking delays.
 * ========================================================================== */

#include <xc.h>

#pragma config FOSC = INTRCIO   // Internal oscillator, I/O function on RA4/RA5
#pragma config WDTE = OFF       // Watchdog Timer Disabled
#pragma config PWRTE = ON       // Power-up Timer Enabled
#pragma config MCLRE = ON       // MCLR pin function is digital input
#pragma config CP = OFF         // Code Protection Disabled
#pragma config CPD = OFF        // Data Code Protection Disabled
#pragma config BOREN = OFF      // Brown-out Reset Disabled
#pragma config IESO = OFF       // Internal External Switchover Disabled
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Disabled

#define _XTAL_FREQ 8000000     // 8 MHz system clock for __delay_ms()

// Bitmasks for the 4 demo board LEDs on PORTC
#define led_1 0x01 // RC0
#define led_2 0x02 // RC1
#define led_3 0x04 // RC2
#define led_4 0x08 // RC3

// Array containing the LED bitmasks for sequential access
unsigned char LEDS[] = {led_1, led_2, led_3, led_4};

void main(void)
{
    OSCCON = 0x70;      // Set internal oscillator to 8 MHz

    ANSEL = 0;          // Configure analog select registers as digital I/O
    ANSELH = 0;

    TRISC = 0x00;       // Configure all PORTC pins as outputs
    PORTC = 0x00;       // Turn off all LEDs initially
    
    while(1)
    {
        // Turn LEDs ON sequentially from RC0 to RC3
        for (int i = 0; i < 4; i++) {
            PORTC |= LEDS[i];
            __delay_ms(500);
        }
        
        // Turn LEDs OFF sequentially in reverse order from RC3 to RC0
        for (int i = 3; i >= 0; i--) {
            PORTC &= ~(1 << i);
            __delay_ms(500);
        }
    }
}