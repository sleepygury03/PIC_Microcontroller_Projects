/* ==========================================================================
 * MCU:         PIC16F690
 * FREQUENCY:   8.0 MHz Internal Oscillator
 * COMPONENTS:  1x 5011AS 7-Segment Display (Common-Cathode), 1x Push Button (RA3)
 * HARDWARE:    1 kOhm current-limiting resistors on segments, internal/external pull-up
 * DESCRIPTION:
 * Drives a single-digit 7-segment display using a non-blocking Finite State 
 * Machine (FSM) to sequentially increment from 0 to 9 on button press. Splits 
 * the segment bitmasks across PORTC (segments A-F) and PORTA (segment G and DP).
 * Uses a 16-bit Timer1 setup with a 1:8 prescaler (~1 ms overflows) to run a 
 * 250 ms mechanical debounce window when a button edge is tracked, preventing 
 * double-triggering or contact noise.
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

// Global variables
uint8_t number = 0;                 // Holds the current active sequential digit counter (0-9)
volatile uint8_t tick_counter = 0;  // Software accumulator increments inside the ISR
volatile uint8_t tick_counter_on = 0; // Control flag: Activates debounce accumulation in ISR

// Interrupt Service Routine (ISR)
void __interrupt() isr(void){
    // Check if the interrupt was triggered by a Timer1 overflow
    if(PIR1 & 0x01){
        TMR1 = 65286; // Reload for a ~1 ms base tick (65536 - 250 steps @ 4.0 us per step)
        
        // Track elapsed time only if the state machine flags debounce tracking active
        if (tick_counter_on == 1)
            tick_counter++;
            
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
    T1CON = 0x31;     // Bits 4-5: T1CKPS = 11 (Prescaler 1:8 -> 4 us steps), Bit 0: TMR1ON = 1 (ON)
    TMR1 = 65286;     // Seed initial preload
    
    // Enable Core Interrupts
    INTCON = 0xC0;    // Enable Global Interrupts (GIE) and Peripheral Interrupts (PEIE)
    PIE1 = 0x01;      // Enable Timer1 Overflow Interrupt (TMR1IE)
 
    TRISA |= btn;     // Set RA3 (button pin) as an input
    
    // Render the initial configuration (digit 0) onto the hardware ports
    PORTA |= number_PORTA[number];
    PORTC = number_PORTC[number];

    // Finite State Machine (FSM) state declaration
    typedef enum {
        STATE_IDLE,         // Waiting for a button press transition
        STATE_DEBOUNCE,     // Filtering mechanical contact noise
        STATE_WAIT_RELEASE  // Preventing autorepeat until button is released
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
                    tick_counter_on = 1;  // Arm debounce accumulation inside the ISR
                    TMR1 = 64535;
                    state = STATE_DEBOUNCE;
                }
                break;
                
            case STATE_DEBOUNCE:
                // Wait for the counter to track 250 ticks (~250 ms debounce confirmation gate)
                if(tick_counter >= 250) {
                    tick_counter_on = 0;  // Turn off debounce interval tracking in the ISR
                    
                    // Validate if the button signal remains low after the noise window
                    if(pressed) {
                        // Increment and cycle the value index bounds
                        if (number >= 10) number = 0;
                        else number++;
                        
                        // Output parallel segment states onto PORTA and PORTC
                        PORTA &= ~(seg_G | seg_P);
                        PORTA |= number_PORTA[number];
                        PORTC = number_PORTC[number];
                        
                        state = STATE_WAIT_RELEASE; // Advance to hold execution until switch open
                    } else {
                        state = STATE_IDLE; // Spurious transient noise detected, return to IDLE
                    }
                }
                break;
                
            case STATE_WAIT_RELEASE:
                // Trap execution here while the user physically holds down the switch
                if(!pressed) {
                    state = STATE_IDLE; // Transition back to IDLE once physical link breaks
                }
                break;
        }
    }
    return;
}
