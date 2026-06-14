/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  1x 5011AS 7-Segment Display (Common-Cathode), 1x On-board Button
 * HARDWARE:    1 kOhm resistors for segments (Uses on-board button on RA3)
 * DESCRIPTION:
 * An advanced non-blocking 7-segment display controller. Increments a counter 
 * (0-9 + DP) when the on-board button on RA3 is pressed. Uses Timer1 interrupts 
 * to generate a stable 1 ms time base, which drives a Finite State Machine (FSM) 
 * for software debouncing (250 ms) and reliable single-press action (prevents 
 * auto-repeat).
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
#define seg_A 0x01
#define seg_B 0x02
#define seg_C 0x04
#define seg_D 0x08
#define seg_E 0x10
#define seg_F 0x20

// PORTA Segment/Button Bitmasks
#define seg_G 0x04
#define seg_P 0x20
#define btn   0x08 // RA3

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

// Shared variables between ISR and main loop must be declared volatile
volatile unsigned char tick_counter = 0;
volatile unsigned char tick_counter_on = 0;

// Button state machine types
typedef enum {
    STATE_IDLE,
    STATE_DEBOUNCE,
    STATE_WAIT_RELEASE
} button_state_t;

void __interrupt() isr(void)
{
    // Check if Timer1 Overflow Interrupt Flag (TMR1IF) is set
    if(PIR1 & 0x01)
    {
        TMR1 = 65286;           // Preload Timer1 to trigger overflow every 1 ms
        if (tick_counter_on == 1) {
            tick_counter++;
        }
        PIR1 &= ~0x01;          // Clear TMR1IF safely
    }    
}  

void main(void) 
{
    // Local variable declarations at the top of main
    button_state_t state = STATE_IDLE; 
    
    OSCCON = 0x70;      // Set internal oscillator to 8 MHz

    ANSEL = 0;          // Configure analog select registers as digital I/O
    ANSELH = 0;

    // Configure display pins on PORTC and PORTA as outputs
    TRISC &= ~(seg_A | seg_B | seg_C | seg_D | seg_E | seg_F); 
    PORTC = 0x00;
    
    TRISA &= ~(seg_G | seg_P); 
    PORTA &= ~(seg_G | seg_P); 
    
    // Timer1 setup: 1:8 Prescaler, peripheral interrupt enabled, Timer1 ON
    T1CON = 0x31;
    TMR1 = 65286;       // Initial preload for 1 ms interval
    INTCON = 0xC0;      // Enable Global (GIE) and Peripheral (PEIE) interrupts
    PIE1 = 0x01;        // Enable Timer1 Overflow Interrupt (TMR1IE)
 
    TRISA |= btn;       // Set RA3 (button) as input
    
    // Display initial number
    PORTA |= number_PORTA[number];
    PORTC = number_PORTC[number];

    while(1)
    {   
        // Poll current raw button state (1 = Pressed, 0 = Released)
        unsigned char pressed = !(PORTA & (1 << 3)); 
        
        switch(state)
        {
            case STATE_IDLE:
                // Wait for the initial key-down event
                if(pressed) {
                    tick_counter = 0;
                    tick_counter_on = 1;  // Unmask the software tick counter in ISR
                    state = STATE_DEBOUNCE;
                }
                break;
                
            case STATE_DEBOUNCE:
                // Debounce window active (Wait 250 ms)
                if(tick_counter >= 250) {
                    tick_counter_on = 0;  // Mask the tick counter
                    
                    // Verify that the button is still firmly pressed after 250 ms
                    if(pressed) {
                        // Increment display digit tracker
                        if (number >= 10) {
                            number = 0;
                        } else {
                            number++;
                        }
                        
                        // Output update sequence
                        PORTA &= ~(seg_G | seg_P);
                        PORTA |= number_PORTA[number];
                        PORTC = number_PORTC[number];
                        
                        state = STATE_WAIT_RELEASE; // Transition to avoid autorepeat
                    } else {
                        state = STATE_IDLE; // False alarm (glitch/bounce), reset
                    }
                }
                break;
                
            case STATE_WAIT_RELEASE:
                // Hold in this state until button is completely released
                if(!pressed) {
                    state = STATE_IDLE;
                }
                break;
        }
    }
    return;
}