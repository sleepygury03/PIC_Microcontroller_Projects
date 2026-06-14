/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  1x 5011AS 7-Segment Display (Common-Cathode), 1x Push Button (RA3)
 * HARDWARE:    1 kOhm current-limiting resistors on segments, external pull-up on button
 * DESCRIPTION:
 * Implements a non-blocking digital dice (0-9) via a Finite State Machine (FSM).
 * Uses a single Timer1 overflow interrupt to handle two asynchronous timebases.
 * First, handles a 1 ms mechanical switch debounce verification window where 2 ticks 
 * equal 1 us. Second, switches to an ultra-fast software counter wheel (0-250) 
 * ticking at microsecond intervals while the button is held. Releasing the switch 
 * freezes the wheel and updates the display across PORTC and PORTA using an optimized 
 * if-else map to save flash memory.
 * ========================================================================== */
#include <xc.h>

// Configuration Bits
#pragma config FOSC = INTRCIO // Internal oscillator, no external crystal needed
#pragma config WDTE = OFF     // Watchdog Timer disabled
#pragma config PWRTE = ON     // Power-up Timer enabled (holds MCU until voltage stabilizes)
#pragma config MCLRE = OFF    // Master Clear (reset pin) functions as a standard digital input
#pragma config CP = OFF       // Flash Program Memory code protection disabled
#pragma config CPD = OFF      // Data EEPROM code protection disabled
#pragma config BOREN = OFF    // Brown-out Reset disabled
#pragma config IESO = OFF     // Internal/External Switchover mode disabled
#pragma config FCMEN = OFF    // Fail-Safe Clock Monitor disabled

#define _XTAL_FREQ 8000000    // System clock frequency (8 MHz)

// Bitmasks for the 7-segment display segments mapped to PORTC
#define seg_A 0x01
#define seg_B 0x02
#define seg_C 0x04
#define seg_D 0x08
#define seg_E 0x10
#define seg_F 0x20

// Bitmasks for segment G, decimal point (P), and button mapped to PORTA
#define seg_G 0x04
#define seg_P 0x20
#define btn   0x08            // Button connected to RA3 (1 << 3)

// Lookup table for PORTC segments (Digits 0-9 and blank state)
uint8_t number_PORTC[] = {
    (seg_A | seg_B | seg_C | seg_D | seg_E | seg_F), // 0
    (seg_B | seg_C),                                 // 1
    (seg_A | seg_B | seg_E | seg_D),                 // 2
    (seg_A | seg_B | seg_C | seg_D),                 // 3
    (seg_B | seg_C | seg_F),                         // 4
    (seg_A | seg_F | seg_C | seg_D),                 // 5
    (seg_A | seg_F | seg_C | seg_D | seg_E),         // 6
    (seg_A | seg_B | seg_C),                         // 7
    (seg_A | seg_B | seg_C | seg_D | seg_E | seg_F), // 8
    (seg_A | seg_B | seg_C | seg_D | seg_F),         // 9
    (0)                                              // Cleared segments
};

// Lookup table for PORTA segments (Segment G and Decimal Point for digits 0-9)
uint8_t number_PORTA[] = {
    (0), (0), (seg_G), (seg_G), (seg_G), (seg_G), (seg_G), (0), (seg_G), (seg_G | seg_P), (seg_P)
};

// Global flags and variables
uint8_t random_on = 0;              // Flag: Activates the fast random number wheel in the ISR
uint8_t number = 0;                 // Holds the current displayed or rolled digit (0-9)
volatile uint16_t tick_counter = 0; // Main timekeeper increments inside the ISR
volatile uint8_t tick_counter_on = 0; // Flag: Activates the 0.5 us debounce timebase in the ISR

// Interrupt Service Routine (ISR)
void __interrupt() isr(void){
    // Check if the interrupt was triggered by a Timer1 overflow
    if(PIR1 & 0x01){
        
        // MODE 1: Mechanical switch debouncing in progress
        if (tick_counter_on == 1){
            tick_counter++;
            TMR1 = 65286; // Reload for ~0.5 us base interval (65536 - 250 steps @ 0.5 us)
        }
        
        // MODE 2: Button is securely held down, generate hardware entropy
        if (random_on == 1){
            tick_counter++;
            // Wrap-around to keep the wheel strictly within the 0-250 range
            if (tick_counter > 250) {
                tick_counter = 0;
            }
            TMR1 = 64536; // Reload for 500 steps (250 us base interval per interrupt)
        }
        
        PIR1 &= ~0x01; // Clear the Timer1 Interrupt Flag before exiting
    }    
}  

void main(void) {
    OSCCON = 0x70;    // Set internal clock to 8 MHz (Instruction cycle Fosc/4 = 2 MHz -> 0.5 us)

    ANSEL = 0;        // Disable analog functions on PORTA
    ANSELH = 0;       // Disable analog functions on PORTB (ANSEL High register)

    // Configure I/O Directions (0 = Output, 1 = Input)
    TRISC &= ~(seg_A | seg_B | seg_C | seg_D | seg_E | seg_F); // Set PORTC segment pins as outputs
    PORTC = 0x00;     // Clear PORTC outputs
    
    TRISA &= ~(seg_G | seg_P);  // Set PORTA segment G and P pins as outputs
    PORTA &= ~(seg_G | seg_P);  // Clear these outputs
    
    // Configure Timer1
    T1CON = 0x01;     // Bit 0: TMR1ON = 1 (Start Timer1), Bits 4-5: T1CKPS = 00 (Prescaler 1:1)
    TMR1 = 64536;     // Initial seed value for Timer1
    
    // Enable Core Interrupts
    INTCON = 0xC0;    // Enable Global Interrupts (GIE) and Peripheral Interrupts (PEIE)
    PIE1 = 0x01;      // Enable Timer1 Overflow Interrupt (TMR1IE)
 
    TRISA |= btn;     // Set RA3 (button pin) as an input
    
    // Display the initial digit (0) on startup
    PORTA |= number_PORTA[number];
    PORTC = number_PORTC[number];

    // Finite State Machine (FSM) state declaration
    typedef enum {
        STATE_IDLE,         // Waiting for a button press transition
        STATE_DEBOUNCE,     // Filtering mechanical contact noise
        STATE_WAIT_RELEASE  // High-speed random wheel cycling until button release
    } button_state_t;

    button_state_t state = STATE_IDLE; // Initialize FSM in IDLE state

    while(1)
    {   
        // Poll button status: Active-Low configuration (external or internal pull-up).
        // 'pressed' evaluates to 1 if the switch pulls RA3 down to GND.
        uint8_t pressed = !(PORTA & (1 << 3)); 
        
        switch(state)
        {
            case STATE_IDLE:
                // Look for an initial button down transition
                if(pressed) {
                    tick_counter = 0;
                    tick_counter_on = 1; // Start the 0.5 us debounce timebase clock
                    TMR1 = 64535;
                    state = STATE_DEBOUNCE;
                }
                break;
                
            case STATE_DEBOUNCE:
                // Wait for the counter to reach 2000 ticks (2 ticks = 1 us -> 2000 ticks = 1 ms window)
                if(tick_counter >= 2000) {
                    tick_counter_on = 0;  // Turn off the debounce interval clock in the ISR
                    
                    // Re-evaluate if the button is still pressed after the delay window
                    if(pressed) {
                        tick_counter = 0; // Clear index counter for the random number generator
                        random_on = 1;    // Enable the ultra-fast random loop (0-250 ticks) in the ISR
                        TMR1 = 65036;
                        state = STATE_WAIT_RELEASE; 
                    } else {
                        state = STATE_IDLE; // False trigger or transient spike detected, abort
                    }
                }
                break;
                
            case STATE_WAIT_RELEASE:
                // Trap execution here while the user holds down the switch.
                // The tick_counter increments at microsecond speeds in the background.
                if(!pressed) {
                    // Switch released! Immediately halt the random wheel to freeze the value
                    random_on = 0;
                    
                    // Map the final tick_counter state (0-250) into 10 uniform blocks of 25 steps
                    // Replaces modulo (%) to significantly reduce compiled program footprint (Flash)
                    if (tick_counter < 25) {
                        number = 0;
                    } else if (tick_counter < 50) {
                        number = 1;
                    } else if (tick_counter < 75) {
                        number = 2;
                    } else if (tick_counter < 100) {
                        number = 3;
                    } else if (tick_counter < 125) {
                        number = 4;
                    } else if (tick_counter < 150) {
                        number = 5;
                    } else if (tick_counter < 175) {
                        number = 6;
                    } else if (tick_counter < 200) {
                        number = 7;
                    } else if (tick_counter < 225) {
                        number = 8;
                    } else if (tick_counter <= 250) {
                        number = 9;
                    }                      
                    
                    // Update the hardware ports to render the rolled value
                    PORTA &= ~(seg_G | seg_P);      // Flush old state flags on PORTA
                    PORTA |= number_PORTA[number];  // Set new segment profiles on PORTA
                    PORTC = number_PORTC[number];  // Output parallel segment states onto PORTC
                    
                    state = STATE_IDLE; // Loop completed. Reset FSM for the next cycle
                }
                break;
        }
    }
    return;
}
