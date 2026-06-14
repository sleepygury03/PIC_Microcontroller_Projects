/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  4x On-board LEDs (RC0-RC3), 1x Push Button (RA3) on Demo Board
 * DESCRIPTION:
 * Combines Timer0 and Interrupt-on-Change (IOC) in a single ISR.
 * Pressing the button on RA3 toggles a global operation flag asynchronously. 
 * When active, the non-blocking Timer0 system clock (~32.77 ms overflows) 
 * drives a 1-second blinking cycle on LED RC0. When inactive, blinking stops 
 * and the outputs are cleared.
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

// Shared variables modified inside ISR must be declared volatile
volatile unsigned char tick_counter = 0;
volatile unsigned char buttonflag = 0;

void __interrupt() isr(void)
{
    // Source 1: Check if Timer0 Overflow Interrupt Flag (T0IF) is set
    if(INTCON & 0x04)
    {     
        tick_counter++;
        INTCON &= ~0x04;        // Clear T0IF safely
    }
    
    // Source 2: Check if PORTA/PORTB Change Interrupt Flag (RABIF) is set
    if(INTCON & 0x01)
    {         
        unsigned char dummy = PORTA;  // 1. IMPORTANT: Read PORTA to clear the hardware mismatch condition
        
        __delay_ms(1);                // 2. Short filter delay for mechanical bounce settlement
        
        // 3. Check if button is still pressed (RA3 is active-low)
        if((PORTA & btn) == 0) 
        {
            if(buttonflag) {
                buttonflag = 0;
            } else {
                buttonflag = 1;
            }
        }
        
        INTCON &= ~0x01;              // 4. Clear the RABIF flag safely after the port read
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
 
    INTCON = 0xA8;      // Enable Global (GIE), Timer0 (T0IE), and Port Change (RABIE) interrupts
    IOCA = btn;         // Enable Interrupt-on-Change specifically for RA3
    
    OPTION_REG = 0x07;  // Assign 1:256 Prescaler to Timer0, internal instruction clock
    
    TRISC &= ~(led_1 | led_2 | led_3 | led_4);  // Set RC0-RC3 as outputs
    PORTC = 0x00;       // Turn off all LEDs initially
    
    unsigned char led_state = 0;
    
    while(1) 
    {
        // Blink logic executes only when button flag is enabled
        if (buttonflag == 1)
        {
            // ~1016 ms interval reached (31 ticks * ~32.77 ms)
            if(tick_counter >= 31)
            {
                tick_counter = 0;
                
                if (led_state == 0) {
                    PORTC |= led_1;
                    led_state = 1;
                } else {
                    PORTC = 0x00; 
                    led_state = 0;
                }
            }
        }
        else 
        {
            PORTC = 0x00;       // Force LEDs off immediately when blinking is disabled
        }
    }
}