/*
 * HCSR04Bridge.c
 *
 * Created: 10/30/2015 9:23:20 PM
 * Author: Daniel Mack
 * 
 * Function:
 * to serve as an inbetween for a microcontroller and the HCSR04 sonar.
 * Triggers the sonar and stores the distance and sends it out the SPI
 * in slave mode. Can return about 55 measurements a second at
 * distance = 10 feet, which about the max of the HCSR04 
 * 
 * PORT B PIN 0 is connected to the Echo pin of the HCSR04
 * PORT D PIN 0 is connected to the Trigger pin of the HCSR04
 * 
 * code good at 16 MHz
 */
 
// includes
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
 
// defines
// TIMEOUT should be set at (.0185*fclk)/8
#define TIMEOUT 40000   // timeout at about 10.3 feet at 16 MHz
#define low(x) ((x) & 0x00FF)
#define high(x) (((x)>>8)& 0x00FF)
#define TRIGGER PD0
#define ECHO PB0
 
// function prototypes
uint16_t time_stamp(uint8_t);
 
// global variables
volatile uint8_t edgeDetect = 0;
volatile uint8_t conversionComplete = 0;
volatile uint16_t distance = 0;
 
int main(){
    // initialize hardware
    // Ports
    cli();
	DDRD = (1<<TRIGGER); // TRIGGER as output
	DDRB =  (1<<PB4); // MISO
    // spi
    SPCR = (1<<SPIE) | (1<<SPE) | (1<<SPR0);    // interrupt, f/4
    SPDR = 0;
    // timer counter 1
    OCR1A = TIMEOUT;
    uint16_t risingEdge = 0;
    uint16_t fallingEdge = 0;
    
	// trigger the sonar with a 10 micro second pulse
	DDRD |= (1<<TRIGGER); 
    while(1){
        cli();
        // atmega88pa pins can be flipped by writing to them
        // hence the two writes and not one write and one
        // clear
        PIND |= (_BV(TRIGGER));
        _delay_us(10);
        PIND |= (_BV(TRIGGER));
        sei();
        // Start Timer
        // set timer up to capture incoming pulse
        // rising edge, ctc, OCR1A as top, f/8
        TCCR1B = (1<<ICES1) | (1<<WGM12) | (1<<CS11);
        // clear pending interrupts
        TIFR1 |= (1<<ICF1) | (1<<OCF1B) | (1<<OCF1A);
        // input capture compA interrupt enabled
        TIMSK1 = (1<<ICIE1) | (1<<OCIE1A);
         
        // look for rising edge, timestamp it then look for falling edge, timestamp it.
        // Do math to find distance. Distance is just a number of counts not normal units
        // ie inches or cm.
        while(conversionComplete == 0){
            if(edgeDetect == 1){
				cli();
				risingEdge = time_stamp(0);
				sei();
            }
            if(edgeDetect == 3){
                cli();
                fallingEdge = time_stamp(1);
                distance = fallingEdge - risingEdge;
				SPDR = low(distance);
                conversionComplete = 1;
                sei();
            }
        }
        // delay 200 microseconds before next reading 
        // as instructed by Ping))) datasheet
        // stop and reset timer
        conversionComplete = 0;
        TCCR1B = 0x00;
		cli();
        TCNT1 = 0x0000;
        sei();
		TIMSK1 = 0x00;
        _delay_us(200);
    }
}
 
// functions
 
// returns timer value at rising edge, sets timer to 
// look for falling edge and returns timer value
// at falling edge
uint16_t time_stamp(uint8_t c){
	switch(c){
        case 0:
            edgeDetect++;
            TCCR1B &= ~_BV(ICES1);
            break;
        case 1:
            edgeDetect = 0;
            break;
    }
    return(ICR1);
}
 
// ISR
ISR(SPI_STC_vect){
	SPDR = high(distance);
}
 
// sets resets after timeout
ISR(TIMER1_COMPA_vect){
	conversionComplete = 1;
    edgeDetect = 0;
    SPDR = 0;
    distance = 0;
	
}
 
ISR(TIMER1_CAPT_vect){
	edgeDetect++;
}
