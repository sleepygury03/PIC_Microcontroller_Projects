/*
 * File:   7_segment_button.c
 * Author: gusta
 *
 * Created on June 13, 2026, 9:24 PM
 */


#include <xc.h>

#pragma config FOSC = INTRCIO
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config MCLRE = OFF
#pragma config CP = OFF
#pragma config CPD = OFF
#pragma config BOREN = OFF
#pragma config IESO = OFF
#pragma config FCMEN = OFF

#define _XTAL_FREQ 8000000

//RC
#define seg_A 0x01
#define seg_B 0x02
#define seg_C 0x04
#define seg_D 0x08
#define seg_E 0x10
#define seg_F 0x20

//RA
#define seg_G 0x04
#define seg_P 0x20
#define btn 0x08

uint8_t number_PORTC[] = {(seg_A | seg_B | seg_C | seg_D | seg_E | seg_F), 
(seg_B |seg_C), 
(seg_A | seg_B | seg_E | seg_D), 
(seg_A | seg_B | seg_C | seg_D),
(seg_B | seg_C | seg_F),
(seg_A | seg_F | seg_C | seg_D ),
(seg_A | seg_F | seg_C | seg_D | seg_E ),
(seg_A | seg_B | seg_C),
(seg_A | seg_B | seg_C | seg_D | seg_E | seg_F),
(seg_A | seg_B | seg_C | seg_D | seg_F),
(0)};
uint8_t number_PORTA[] = {(0),
(0),
(seg_G),
(seg_G),
(seg_G),
(seg_G),
(seg_G),
(0),
(seg_G),
(seg_G | seg_P),
(seg_P)};

uint8_t number = 0;
volatile uint8_t tick_counter = 0;
volatile uint8_t tick_counter_on = 0;
void __interrupt() isr(void){
    if(PIR1 & 0x01){
        TMR1 = 65286;
        if (tick_counter_on == 1)
            tick_counter++;
        PIR1 &= ~0x01;
        }    
    }  

void main(void) {
     OSCCON = 0x70;

    ANSEL = 0;
    ANSELH = 0;

    TRISC &= ~(seg_A | seg_B | seg_C | seg_D | seg_E | seg_F); 
    PORTC = 0x00;
    
    TRISA &= ~(seg_G | seg_P); 
    PORTA &= ~(seg_G | seg_P); 
    
    T1CON = 0x31;
    TMR1 = 65286;
    INTCON = 0xC0;
    PIE1 = 0x01;
 
    TRISA |= btn;
 
    
    PORTA |= number_PORTA[number];
    PORTC = number_PORTC[number];

    // Vi skapar tydliga namn för våra tre tillstånd
typedef enum {
    STATE_IDLE,
    STATE_DEBOUNCE,
    STATE_WAIT_RELEASE
} button_state_t;

button_state_t state = STATE_IDLE;

while(1)
{   
    // Enkel polling: Är knappen tryckt just nu? (1 = Ja, 0 = Nej)
    uint8_t pressed = !(PORTA & (1 << 3)); 
    
    switch(state)
    {
        case STATE_IDLE:
            // Vänta på att knappen trycks ner
            if(pressed) {
                tick_counter = 0;
                tick_counter_on = 1;  // Starta timern i ISR
                state = STATE_DEBOUNCE;
            }
            break;
            
        case STATE_DEBOUNCE:
            // Vänta i 250 ms (avstudsning)
            if(tick_counter >= 250) {
                tick_counter_on = 0;  // Stäng av timern
                
                // Kontrollera om knappen fortfarande är tryckt efter 250 ms
                if(pressed) {
                    // --- Uppdatera siffran (Exakt din kod) ---
                    if (number >= 10) number = 0;
                    else number++;
                    
                    PORTA &= ~(seg_G | seg_P);
                    PORTA |= number_PORTA[number];
                    PORTC = number_PORTC[number];
                    // -----------------------------------------
                    
                    state = STATE_WAIT_RELEASE; // Gå och vänta på att användaren släpper knappen
                } else {
                    state = STATE_IDLE; // Det var bara en kort studs, återgå
                }
            }
            break;
            
        case STATE_WAIT_RELEASE:
            // Förhindra autorepeat: Vänta här tills knappen är helt släppt
            if(!pressed) {
                state = STATE_IDLE;
            }
            break;
    }
}
return;
}
