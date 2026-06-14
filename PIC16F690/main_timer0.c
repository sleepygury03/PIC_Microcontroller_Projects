/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  4x On-board LEDs (RC0-RC3) on Demo Board
 * DESCRIPTION:
 * Demonstrates basic 8-bit Timer0 usage with interrupts to blink an LED.
 * Configures the Timer0 prescaler to 1:256 with the internal instruction 
 * clock. The hardware generates an overflow interrupt roughly every 32.77 ms.
 * By polling for 31 ticks in main, a precise non-blocking ~1-second toggle 
 * interval is achieved on RC0.
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

#define _XTAL_FREQ 8000000     // 8 MHz system clock

// PORTC LED Bitmasks
#define led_1 0x01 // RC0
#define led_2 0x02 // RC1
#define led_3 0x04 // RC2
#define led_4 0x08 // RC3

// PORTA Button Bitmask (RA3)
#define btn 0x08

unsigned char LEDS[] = {led_1, led_2, led_3, led_4};
unsigned char counter = 0;

// Shared flag between ISR and main loop must be declared volatile
volatile unsigned char tick_counter = 0;

void __interrupt() isr(void)
{
    // Check if Timer0 Overflow Interrupt Flag (T0IF) is set
    if(INTCON & 0x04)
    {     
        tick_counter++;
        INTCON &= ~0x04;        // Clear the T0IF flag safely
    }    
}

void main(void)
{
    OSCCON = 0x70;      // Set internal oscillator to 8 MHz

    ANSEL = 0;          // Configure analog select registers as digital I/O
    ANSELH = 0;
    
    CM1CON0 = 0x00;     // Disable Comparator 1
    CM2CON0 = 0x00;     // Disable Comparator 2
    
    TRISA |= btn;       // Set RA3 (button) as input

    INTCON = 0xA0;      // Enable Global Interrupts (GIE) and Timer0 Overflow Interrupts (T0IE)
    IOCA = btn;         // Note: IOCA configured but IOC interrupts not enabled in INTCON
    
    OPTION_REG = 0x07;  // Assign 1:256 Prescaler to Timer0, internal instruction clock
    
    TRISC &= ~(led_1 | led_2 | led_3 | led_4);  // Set RC0-RC3 as outputs
    PORTC = 0x00;       // Turn off all LEDs initially
    
    unsigned char led_state = 0;
    
    while(1) 
    {
        // ~1016 ms interval reached (31 ticks * ~32.77 ms)
        if(tick_counter >= 31)
        {
            tick_counter = 0;   // Reset the software tick buffer
            
            // Toggle LED state on RC0
            if (led_state == 0) {
                PORTC |= led_1;
                led_state = 1;
            } else {
                PORTC = 0x00; 
                led_state = 0;
            }
        }
    }
}