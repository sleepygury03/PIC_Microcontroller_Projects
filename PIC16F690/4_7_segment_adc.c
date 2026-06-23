/* ==========================================================================
 * MCU:          PIC16F690
 * FREQUENCY:    8.0 MHz Internal Oscillator
 * COMPONENTS:   1x 3461AS 4-Digit 7-Segment Display (Common-Cathode)
 * HARDWARE:     2.2 kOhm current-limiting resistors on each segment
 * DESCRIPTION:
 * Drives all 4 digits on a 3461AS display. Integrates an ADC polling routine
 * that reads analog input AN0 (RA0) and outputs the 10-bit raw binary reading
 * (scaled up from the left-justified ADRESH register) directly to the screen.
 * Segments A-F are controlled via PORTC, while Segment G and DP are handled via
 * PORTB. A dual Finite State Machine (FSM) architecture runs in the main loop:
 * one handles ADC conversion timing/polling, and the other manages the 
 * continuous multiplexed display refresh (~111 Hz).
 * * NOTE FOR LOW PIN COUNT DEMO BOARD / PICKIT 2 USERS:
 * When using the standard Microchip Low Pin Count Demo Board, the onboard RA0/AN0 
 * potentiometer shares a node with the MCLR circuitry. The pull-up resistor network 
 * creates a voltage divider loop when powered solely by the PICkit 2 USB rail. 
 * This prevents the maximum analog input from reaching VDD (limiting top ADC counts). 
 * Use an external power supply to clear this loading effect and achieve full range.
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

// PORTC Segment Bitmasks (Anodes)
#define seg_A 0x01 // RC0
#define seg_B 0x02 // RC1
#define seg_C 0x04 // RC2
#define seg_D 0x08 // RC3
#define seg_E 0x10 // RC4
#define seg_F 0x20 // RC5

// PORTB Segment Bitmasks (Anodes - Reassigned from PORTA)
#define seg_G 0x40 // RB6
#define seg_P 0x02 // RB1 (Decimal Point)

// PORTA display sinking (Common Cathodes)
#define dig_1 0x04 // RA2
#define dig_2 0x10 // RA4
#define dig_3 0x20 // RA5

// PORTB display sinking (Common Cathodes)
#define dig_4 0x80 // RB7

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

// Array containing segment configurations for PORTB (Segment G control mapped to indices)
unsigned char number_PORTA[] = {
    (0),       // 0
    (0),       // 1
    (seg_G),   // 2
    (seg_G),   // 3
    (seg_G),   // 4
    (seg_G),   // 5
    (seg_G),   // 6
    (0),       // 7
    (seg_G),   // 8
    (seg_G),   // 9 (Clean segment maps, decimal point injected via FSM logic)
    (seg_P)    // 10
};

// Global registers for data values and system states
uint16_t number = 0;
unsigned char display_number = 1;
unsigned char adc_done = 0;

// Shared variables between ISR and main loop (declared volatile)
volatile unsigned char ms_counter = 0;
volatile unsigned char flicker_counter = 0;

// Global buffers storing isolated base-10 digits for display multiplexing
uint8_t number_base_1000;
uint8_t number_base_100;
uint8_t number_base_10;
uint8_t number_base_1;

// State tracking enum for Display Multiplexing FSM
typedef enum {
    STATE_TIMER,
    STATE_SEGMENT
} segment_state_t;

segment_state_t segment_state = STATE_TIMER;

// State tracking enum for ADC Polling FSM
typedef enum {
    STATE_ADC_OFF,
    STATE_ADC_ON
} adc_state_t;

adc_state_t adc_state = STATE_ADC_ON;

void __interrupt() isr(void)
{
    // Check if Timer1 Overflow Interrupt Flag (TMR1IF) is set
    if(PIR1 & 0x01)
    {   
        ms_counter++;           // Increment clock tick used to throttle ADC sampling intervals
        TMR1 = 65286;           // Preload Timer1 to maintain precise 1 ms ticks (with 1:8 prescaler)
        flicker_counter++;      // Increment display refresh multiplexing counter
        PIR1 &= ~0x01;          // Clear the TMR1IF flag safely
    }    
}

void main(void) 
{
    OSCCON = 0x70;      // Set internal oscillator to 8 MHz

    ANSEL = 0x01;       // Configure RA0/AN0 as Analog input, all other pins as Digital I/O
    ANSELH = 0;
    
    CM1CON0 = 0x00;     // Disable Comparator 1 to free up pins
    CM2CON0 = 0x00;     // Disable Comparator 2 to free up pins
    
    // Configure display segment pins on PORTC as outputs
    TRISC &= ~(seg_A | seg_B | seg_C | seg_D | seg_E | seg_F); 
    PORTC = 0x00;       // Clear PORTC outputs initially
    
    // Configure common cathode pins on PORTA as outputs
    TRISA &= ~(dig_1 | dig_2 | dig_3);
    PORTA &= ~(dig_1 | dig_2 | dig_3);  // Clear PORTA outputs initially
    
    // Configure common cathode (dig_4) and segment controls (G, DP) on PORTB as outputs
    TRISB &= ~(dig_4 | seg_G | seg_P);
    PORTB &= ~(dig_4 | seg_G | seg_P);
    
    // Timer1 setup: 1:8 Prescaler, peripheral interrupt enabled, Timer1 ON
    T1CON = 0x31;
    TMR1 = 65286;       // Initial preload for the first 1 ms tick
    
    INTCON = 0xC0;      // Enable Global Interrupts (GIE) and Peripheral Interrupts (PEIE)
    PIE1 = 0x01;        // Enable Timer1 Overflow Interrupt (TMR1IE)
    
    // Load initial data onto the IO latches
    PORTA |= number_PORTA[number];
    PORTB |= number_PORTC[number];
    
    // Initialize the ADC Module
    ADCON0 = 0x01;      // Turn on ADC module, select channel AN0, Left-Justified layout
    ADCON1 = 0x60;      // Set conversion clock source to Fosc/64 for stability
    
    while(1)
    { 
        // ==================================================================
        // FSM 1: ADC SAMPLING AND POLLING CONTROLLER
        // ==================================================================
        switch(adc_state){
            case STATE_ADC_OFF:
                // Provide a 10 ms rest window between conversions to throttle sample rates
                if(ms_counter > 10 ){
                    adc_state = STATE_ADC_ON;
                }
            break;
            
            case STATE_ADC_ON:
                // Trigger a new conversion if one isn't currently running
                if (adc_done == 0){
                    ADCON0 |= 0x02;     // Set the GO/DONE bit to launch conversion
                    adc_done = 1;       // Lock flag to prevent repeated restarts
                }
                
                // Check if the hardware has completed the conversion (GO/DONE bit clears)
                if (!(ADCON0 & 0x02)){
                    // Read Left-Justified high byte and shift left to scale up to 10-bit range (0-1020)
                    number = ADRESH;
                    number = number << 2; 
    
                    // Break down integer value into independent decimal elements safely via explicit casts
                    number_base_1    = (uint8_t)(number % 10);
                    number_base_10   = (uint8_t)((number / 10) % 10);
                    number_base_100  = (uint8_t)((number / 100) % 10);
                    number_base_1000 = (uint8_t)(number / 1000);
                    
                    adc_done = 0;       // Release the software start lock
                    ms_counter = 0;     // Clear counter to track the next 10 ms delay period
                    adc_state = STATE_ADC_OFF; // Transition to standard delay buffer state
                }
            break;
        }

        // ==================================================================
        // FSM 2: 7-SEGMENT DISPLAY MULTIPLEXING CONTROLLER
        // ==================================================================
        switch(segment_state){
            case STATE_TIMER:
                // Wait 3 ms before shifting to the next digit to prevent flickering (~111 Hz refresh rate)
                if (flicker_counter >= 3){
                    flicker_counter = 0;
                    segment_state = STATE_SEGMENT;
                }
                break;
              
            case STATE_SEGMENT:
                // --- DIGIT 1 (Ones Place) ---
                if(display_number == 1){
                    PORTA |= (dig_2 | dig_3);            // Set inactive digits to HIGH (turn off)
                    PORTA &= ~dig_1;                     // Pull active digit to LOW (sink/turn on)
                    PORTB &= ~(seg_G | seg_P);           // Clear PORTB segments before writing new data
                    PORTB |= dig_4;                      // Ensure Digit 4 is off
                    PORTB |= number_PORTA[number_base_1]; // Output Segment G data
                    PORTC = number_PORTC[number_base_1];  // Output Segments A-F data
                }
                // --- DIGIT 2 (Tens Place) ---
                else if(display_number == 2){
                    // Leading zero blanking: Skip if tens, hundreds, and thousands digits are zero
                    if(number_base_10 == 0 && number_base_100 == 0 && number_base_1000 == 0)
                        segment_state = STATE_TIMER;
                    else{
                        PORTA |= (dig_1 | dig_3);        // Set inactive digits to HIGH (turn off)
                        PORTA &= ~dig_2;                 // Pull active digit to LOW (sink/turn on)
                        PORTB &= ~(seg_G | seg_P);       // Clear PORTB segments before writing new data
                        PORTB |= dig_4;                      
                        PORTB |= number_PORTA[number_base_10];
                        PORTC = number_PORTC[number_base_10];
                    }
                }
                // --- DIGIT 3 (Hundreds Place) ---
                else if(display_number == 3){
                    // Leading zero blanking: Skip if hundreds and thousands digits are zero
                    if(number_base_100 == 0 && number_base_1000 == 0)
                        segment_state = STATE_TIMER;
                    else{
                        PORTA |= (dig_1 | dig_2);         // Set inactive digits to HIGH (turn off)
                        PORTA &= ~dig_3;                 // Pull active digit to LOW (sink/turn on)
                        PORTB &= ~(seg_G | seg_P);       // Clear PORTB segments before writing new data
                        PORTB |= dig_4;                      
                        PORTB |= number_PORTA[number_base_100];
                        PORTC = number_PORTC[number_base_100];
                    }
                }
                // --- DIGIT 4 (Thousands Place) ---
                else if(display_number == 4){
                    // Leading zero blanking: Skip if thousands digit is zero
                    if(number_base_1000 == 0)
                        segment_state = STATE_TIMER;
                    else{
                        PORTA |= (dig_1 | dig_2 | dig_3);         // Set inactive digits to HIGH (turn off)
                        PORTB &= ~dig_4;                          // Pull active digit to LOW (sink/turn on)
                        PORTB &= ~(seg_G | seg_P);                // Clear PORTB segments before writing new data
                        
                        // Force the decimal point (seg_P) to turn ON explicitly for digit 4
                        PORTB |= (number_PORTA[number_base_1000] | seg_P);
                        PORTC = number_PORTC[number_base_1000];
                    }
                }
                
                // Cyclically cycle through display digits 1 -> 2 -> 3 -> 4 -> 1
                if(display_number >= 4)
                    display_number = 1;
                else
                    display_number++;
                
                segment_state = STATE_TIMER; // Flip back to timing buffer state
                break;
        }           
    }
}