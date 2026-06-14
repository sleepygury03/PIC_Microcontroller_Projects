/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  4x On-board LEDs (RC0-RC3) on Demo Board
 * DESCRIPTION:
 * Demonstrates 16-bit Timer1 usage with interrupts to achieve precise timing.
 * Configures Timer1 with a 1:8 prescaler. By preloading TMR1 with 40536, the 
 * hardware triggers an overflow interrupt exactly every 100 ms (0.1s). 
 * Counting 10 ticks in the main loop provides a perfect non-blocking 
 * 1-second toggle rate on LED RC0.
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

// Shared variable between ISR and main loop must be declared volatile
volatile unsigned char tick_counter = 0;

void __interrupt() isr(void)
{
    // Check if Timer1 Overflow Interrupt Flag (TMR1IF) is set
    if(PIR1 & 0x01)
    {
        TMR1 = 40536;           // Preload Timer1 to maintain the 100 ms interval
        tick_counter++;
        PIR1 &= ~0x01;          // Clear the TMR1IF flag safely
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
 
    T1CON = 0x31;       // Timer1 settings: 1:8 Prescaler, Dedicated Oscillator disabled, Timer1 ON
    TMR1 = 40536;       // Initial preload for first 100 ms tick
    
    INTCON = 0xC0;      // Enable Global Interrupts (GIE) and Peripheral Interrupts (PEIE)
    PIE1 = 0x01;        // Enable Timer1 Overflow Interrupt (TMR1IE)
    IOCA = btn;         // Note: IOCA configured but IOC interrupts not enabled in INTCON
    
    TRISC &= ~(led_1 | led_2 | led_3 | led_4);  // Set RC0-RC3 as outputs
    PORTC = 0x00;       // Turn off all LEDs initially
    
    unsigned char led_state = 0;
    
    while(1) 
    {
        // 1000 ms interval reached (10 ticks * 100 ms)
        if(tick_counter >= 10)
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