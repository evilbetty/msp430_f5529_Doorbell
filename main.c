
#include <msp430.h>
#include <stdint.h>
#include "clockinit.h"
#include "comein.h"

#define FREQ					32000000

#define GREEN_LED				BIT7
#define RED_LED					BIT0

#define BUTTON                  BIT1
#define PIN_SPEAKER             BIT2

#define SAMPLES_PER_SECOND      11025


// Get the size of our data array
const uint16_t binary_data_size = sizeof( comein_raw ) / sizeof( comein_raw[0] );

// Data location (start higher than binary_data_size to not play at reset)
volatile uint16_t counter = ( sizeof( comein_raw ) / sizeof( comein_raw[0] ) ) + 1;

// Sample buffer
volatile uint8_t sample;


void gpio_init()
{
	P1DIR |= RED_LED;
	P1OUT |= RED_LED;
	P4DIR |= GREEN_LED;
	P4OUT &= ~GREEN_LED;

	P1SEL |= PIN_SPEAKER;
	P1DIR |= PIN_SPEAKER;
    P1DS |= PIN_SPEAKER;
	
	// Button as input, output high for pullup, and pullup on
    P2DIR &= ~BUTTON;
    P2OUT |= BUTTON;
    P2REN |= BUTTON;
    // P1 interrupt enabled with P1.3 as trigger (raising edge)
    P2IE |= BUTTON;
    P2IES |= BUTTON;
    // P1.3 interrupt flag cleared.
    P2IFG &= ~BUTTON;
}


int main()
{
	WDTCTL = WDTPW + WDTHOLD; //Stop WDT

	ucs_clockinit(FREQ, 1, 0);
	gpio_init();


    // Timer 0 for output PWM (for 8bit values)
    TA0CTL = TASSEL_2 | MC_1;
    TA0CCR0 = 255;
    TA0CCTL1 |= OUTMOD_7;
    TA0CCR1 = 0; // Added for 0 output after reset


    // Timer 1 to interrupt at sample speed
    TA1CTL = TASSEL_2 | MC_1;
    TA1CCR0 = FREQ / SAMPLES_PER_SECOND - 1;
    TA1CCTL0 |= CCIE;

	_BIS_SR(GIE);       // Enable global interrupts


	while(1){
        
        if ( counter <= binary_data_size ) 
        {   // If data is still playing do LPM0 to keep
            // the timer running and audio playing.
			sample = comein_raw[counter++];
            LPM0;
        }
        else 
        {
            // If done playing go to LPM4 so that only
            // the button wakes MCU up.
            sample = 0;
            TA0CCR1 = 0;         // Added for guaranteed 0 output.
            __delay_cycles(500); // Needed to not have random pin output
            // We really dont want Vcc going through speaker when idle
            P1OUT &= ~RED_LED;
            LPM4;
        }
    }
}

#pragma vector = TIMER1_A0_VECTOR
__interrupt void T1A0_ISR(void)
{
    // Output sample and wake mcu to get next sample ready
    TA0CCR1 = sample;
    LPM0_EXIT;
}

#pragma vector = PORT2_VECTOR
__interrupt void PORT2_ISR(void)
{
    // Set playnow var when button pressed (and a start value)
    counter = 0;     // Our audio wastes a whole KB of begin silence
    P2IFG &= ~BUTTON;   // Clearing IFG. This is required.
    P1OUT |= RED_LED;
    LPM4_EXIT;
}

