/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  4x On-board LEDs (RC0-RC3), 1x Push Button (RA3) on Demo Board
 * DESCRIPTION:
 * Demonstrates the usage of Interrupt-on-Change (IOC) on PORTA. Pressing the
 * button on RA3 triggers an asynchronous port change interrupt. The ISR 
 * handles hardware mismatch clearing, provides a short debounce window, and 
 * toggles a global status flag to control the state of an LED.
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

// PORTA Button Bitmask (RA3)
#define btn 0x08

unsigned char LEDS[] = {led_1, led_2, led_3, led_4};
unsigned char counter = 0;

// Shared flag between ISR and main loop must be declared volatile
volatile unsigned char flag = 0;

void __interrupt() isr(void)
{
    // Check if PORTA/PORTB Change Interrupt Flag (RABIF) is set
    if(INTCON & 0x01)
    {        
        unsigned char dummy = PORTA;  // 1. IMPORTANT: Read PORTA to clear the hardware mismatch condition
        
        __delay_ms(20);               // 2. Wait out mechanical button bouncing
        
        // 3. Check if button is STILL pressed after 20 ms (RA3 is active-low)
        if((PORTA & btn) == 0) 
        {
            if(flag) {
                flag = 0;
            } else {
                flag = 1;
            }
        }
        
        INTCON &= ~0x01;              // 4. Clear the RABIF flag safely after the read
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
 
    INTCON = 0x88;      // Enable Global Interrupts (GIE) and Port Change Interrupts (RABIE)
    IOCA = btn;         // Enable Interrupt-on-Change specifically for RA3
    
    TRISC &= ~(led_1 | led_2 | led_3 | led_4);  // Set RC0-RC3 as outputs
    PORTC = 0x00;       // Turn off all LEDs initially
    
    while(1) 
    {
        // Control LED based on the flag toggled by the ISR
        if(flag) {
            PORTC |= led_1;
        } else {
            PORTC = 0x00;             
        }
    }
}