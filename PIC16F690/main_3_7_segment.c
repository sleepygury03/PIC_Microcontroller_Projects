/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  1x 3461AS 4-Digit 7-Segment Display (Common-Cathode)
 * HARDWARE:    2.2 kOhm current-limiting resistors on each segment
 * DESCRIPTION:
 * Drives 3 out of 4 digits on a 3461AS display to automatically cycle from 
 * 000 to 999. The 4th digit is left disconnected due to MCLR/demoboard limits.
 * Splits the segment bitmasks across PORTC (segments A-F) and PORTA (segment 
 * G and DP). Uses a precise 16-bit Timer1 interrupt setup with a 1:8 prescaler 
 * to generate 1 ms ticks. The main loop tracks these ticks to update the data 
 * value every 100 ms, while a fast FSM handles the active digit multiplexing.
 * ========================================================================== */

#include <xc.h>

#pragma config FOSC = INTRCIO   // Internal oscillator, I/O function on RA4/RA5
#pragma config WDTE = OFF       // Watchdog Timer Disabled
#pragma config PWRTE = ON        // Power-up Timer Enabled
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
#define seg_G 0x01 // RA0
#define seg_P 0x02 // RA1 (Decimal Point)

// PORTA display sinking (Common Cathodes)
#define dig_1 0x04 // RA2
#define dig_2 0x10 // RA4
#define dig_3 0x20 // RA5

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

uint16_t number = 0;
unsigned char display_number = 1;

// Shared variables between ISR and main loop (declared volatile)
volatile unsigned char ms_counter = 0;
volatile unsigned char flicker_counter = 0;

uint8_t number_base_100;
uint8_t number_base_10;
uint8_t number_base_1;

typedef enum {
    STATE_TIMER,
    STATE_SEGMENT
} segment_state_t;

segment_state_t state = STATE_TIMER;

void __interrupt() isr(void)
{
    // Check if Timer1 Overflow Interrupt Flag (TMR1IF) is set
    if(PIR1 & 0x01)
    {
        TMR1 = 65286;           // Preload Timer1 to maintain precise 1 ms ticks (with 1:8 prescaler)
        ms_counter++;
        flicker_counter++;
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
    
    TRISA &= ~(seg_G | seg_P | dig_1 | dig_2 | dig_3);
    PORTA &= ~(seg_G | seg_P | dig_1 | dig_2 | dig_3);  // Clear PORTA outputs initially
    
    // Timer1 setup: 1:8 Prescaler, peripheral interrupt enabled, Timer1 ON
    T1CON = 0x31;
    TMR1 = 65286;       // Initial preload for the first 1 ms tick
    
    INTCON = 0xC0;      // Enable Global Interrupts (GIE) and Peripheral Interrupts (PEIE)
    PIE1 = 0x01;        // Enable Timer1 Overflow Interrupt (TMR1IE)
    
    // Load initial digit (0) onto the display
    PORTA |= number_PORTA[number];
    PORTC = number_PORTC[number];
    
    while(1)
    {
        // 100 ms software tick buffer check
        if(ms_counter >= 100)
        {
            ms_counter = 0;   // Reset software tick buffer
            
            // Cycle digit tracker (0 to 999)
            if (number >= 999) {
                number = 0;
            } else {
                number++;
            }
            number_base_1 = number % 10;
            number_base_10 = (number/10) % 10;
            number_base_100 = (uint8_t)(number/100);
        }
        
        switch(state){
            case STATE_TIMER:
                // Wait 3 ms before shifting to the next digit to prevent flickering (~111 Hz refresh rate)
                if (flicker_counter >= 3){
                    flicker_counter = 0;
                    state = STATE_SEGMENT;
                }
                break;
              
            case STATE_SEGMENT:
                if(display_number == 1){
                    PORTA |= (dig_2 | dig_3);            // Set inactive digits to HIGH (turn off)
                    PORTA &= ~dig_1;                     // Pull active digit to LOW (sink/turn on)
                    PORTA &= ~(seg_G | seg_P);           // Clear PORTA segments before writing new data
                    PORTA |= number_PORTA[number_base_1];
                    PORTC = number_PORTC[number_base_1];
                }
                else if(display_number == 2){
                    // Leading zero blanking: Skip if both tens and hundreds digits are zero
                    if(number_base_10 == 0 && number_base_100 == 0)
                        state = STATE_TIMER;
                    else{
                        PORTA |= (dig_1 | dig_3);        // Set inactive digits to HIGH (turn off)
                        PORTA &= ~dig_2;                 // Pull active digit to LOW (sink/turn on)
                        PORTA &= ~(seg_G | seg_P);       // Clear PORTA segments before writing new data
                        PORTA |= number_PORTA[number_base_10];
                        PORTC = number_PORTC[number_base_10];
                    }
                }
                else if(display_number == 3){
                    // Leading zero blanking: Skip if hundreds digit is zero
                    if(number_base_100 == 0)
                        state = STATE_TIMER;
                    else{
                        PORTA |= (dig_1 | dig_2);        // Set inactive digits to HIGH (turn off)
                        PORTA &= ~dig_3;                 // Pull active digit to LOW (sink/turn on)
                        PORTA &= ~(seg_G | seg_P);       // Clear PORTA segments before writing new data
                        PORTA |= number_PORTA[number_base_100];
                        PORTC = number_PORTC[number_base_100];
                    }
                }
                
                // Cyclically cycle through display digits 1 -> 2 -> 3 -> 1
                if(display_number >= 3)
                    display_number = 1;
                else
                    display_number++;
                
                state = STATE_TIMER;
                break;
        }           
    }
}