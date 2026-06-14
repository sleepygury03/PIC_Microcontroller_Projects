/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  1x 5011AS 7-Segment Display (Common-Cathode)
 * HARDWARE:    1 kOhm current-limiting resistors on each segment
 * DESCRIPTION:
 * Drives a single-digit 7-segment display to automatically cycle from 0 to 9 
 * and light up the decimal point (DP) on index 9/10. Splits the segment bitmasks 
 * across PORTC (segments A-F) and PORTA (segment G and DP). Uses a precise 
 * 16-bit Timer1 interrupt setup (100 ms overflows) to increment the digit 
 * every 1 second (10 ticks) without blocking the processor.
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

// PORTC Segment Bitmasks
#define seg_A 0x01 // RC0
#define seg_B 0x02 // RC1
#define seg_C 0x04 // RC2
#define seg_D 0x08 // RC3
#define seg_E 0x10 // RC4
#define seg_F 0x20 // RC5

// PORTA Segment Bitmasks
#define seg_G 0x04 // RA2
#define seg_P 0x20 // RA5 (Decimal Point)

// Array containing segment configurations for PORTC (Digits 0-9, then blank/clear)
unsigned char number_PORTC[] = {
    (seg_A | seg_B | seg_C | seg_D | seg_E | seg_F), 
    (seg_B | seg_C), 
    (seg_A | seg_B | seg_E | seg_D), 
    (seg_A | seg_B | seg_C | seg_D),
    (seg_B | seg_C | seg_F),
    (seg_A | seg_F | seg_C | seg_D),
    (seg_A | seg_F | seg_C | seg_D | seg_E),
    (seg_A | seg_B | seg_C),
    (seg_A | seg_B | seg_C | seg_D | seg_E | seg_F),
    (seg_A | seg_B | seg_C | seg_D | seg_F),
    (0)
};

// Array containing segment configurations for PORTA (Segment G and DP controls)
unsigned char number_PORTA[] = {
    (0),
    (0),
    (seg_G),
    (seg_G),
    (seg_G),
    (seg_G),
    (seg_G),
    (0),
    (seg_G),
    (seg_G | seg_P),
    (seg_P)
};

unsigned char number = 0;

// Shared variable between ISR and main loop must be declared volatile
volatile unsigned char tick_counter = 0;

void __interrupt() isr(void)
{
    // Check if Timer1 Overflow Interrupt Flag (TMR1IF) is set
    if(PIR1 & 0x01)
    {
        TMR1 = 40536;           // Preload Timer1 to maintain the precise 100 ms interval
        tick_counter++;
        PIR1 &= ~0x01;          // Clear the TMR1IF flag safely
    }    
}

void main(void) 
{
    OSCCON = 0x70;      // Set internal oscillator to 8 MHz

    ANSEL = 0;          // Configure analog select registers as digital I/O
    ANSELH = 0;

    // Configure display pins on PORTC and PORTA as outputs
    TRISC &= ~(seg_A | seg_B | seg_C | seg_D | seg_E | seg_F); 
    PORTC = 0x00;       // Clear PORTC outputs initially
    
    TRISA &= ~(seg_G | seg_P); 
    PORTA &= ~(seg_G | seg_P);  // Clear PORTA outputs initially
    
    // Timer1 setup: 1:8 Prescaler, peripheral interrupt enabled, Timer1 ON
    T1CON = 0x31;
    TMR1 = 40536;       // Initial preload for the first 100 ms tick
    
    INTCON = 0xC0;      // Enable Global Interrupts (GIE) and Peripheral Interrupts (PEIE)
    PIE1 = 0x01;        // Enable Timer1 Overflow Interrupt (TMR1IE)
    
    // Load initial digit (0) onto the display
    PORTA |= number_PORTA[number];
    PORTC = number_PORTC[number];
    
    while(1)
    {
        // 1000 ms interval reached (10 ticks * 100 ms)
        if(tick_counter >= 10)
        {
            tick_counter = 0;   // Reset software tick buffer
            
            // Cycle digit tracker (0 to 10)
            if (number >= 10) {
                number = 0;
            } else {
                number++;
            }
            
            // Clear current segment bits and load new values
            PORTA &= ~(seg_G | seg_P);
            PORTA |= number_PORTA[number];
            PORTC = number_PORTC[number];           
        }
    }
}