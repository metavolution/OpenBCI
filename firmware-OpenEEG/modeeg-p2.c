/*
 * ModularEEG firmware for one-way transmission, v0.5.4-p2
 * Copyright (c) 2002-2003, Joerg Hansmann, Jim Peters, Andreas Robinson
 * Modification for Atmega48 (c) 2005, Yuri Smolyakov
 * License: GNU General Public License (GPL) v2
 * Compiles with AVR-GCC v3.3.
 *
 * Note: -p2 in the version number means this firmware is for packet version 2.
 */

//////////////////////////////////////////////////////////////
/*
////////// Packet Format Version 2 ////////////
// 17-byte packets are transmitted from the ModularEEG at 256Hz,
// using 1 start bit, 8 data bits, 1 stop bit, no parity, 57600 bits per second.
// Minimial transmission speed is 256Hz * sizeof(modeeg_packet) * 10 = 43520 bps.

struct modeeg_packet
{
	uint8_t		sync0;		// = 0xA5
	uint8_t		sync1;		// = 0x5A
	uint8_t		version;	// = 2
	uint8_t		count;		// packet counter. Increases by 1 each packet
	uint16_t	data[6];	// 10-bit sample (= 0 - 1023) in big endian (Motorola) format
	uint8_t		switches;	// State of PD5 to PD2, in bits 3 to 0
};

// Note that data is transmitted in big-endian format.
// By this measure together with the unique pattern in sync0 and sync1 it is guaranteed,
// that re-sync (i.e after disconnecting the data line) is always safe.

// At the moment communication direction is only from Atmel-processor to PC.
// The hardware however supports full duplex communication. This feature
// will be used in later firmware releases to support the PWM-output and
// LED-Goggles.
*/

//////////////////////////////////////////////////////////////
/*
 * Program flow:
 *
 * When 256Hz timer expires: goto SIGNAL(SIG_OVERFLOW0)
 * SIGNAL(SIG_OVERFLOW0) enables the ADC
 *
 * Repeat for each channel in the ADC:
 * Sampling starts. When it completes: goto SIGNAL(SIG_ADC)
 * SIGNAL(SIG_ADC) reads the sample and restarts the ADC.
 *
 * SIGNAL(SIG_ADC) writes first byte to UART data register
 * (UDR) which starts the transmission over the serial port.
 *
 * Repeat for each byte in packet:
 * When transmission begins and UDR empties: goto SIGNAL(SIG_UART_DATA)
 *
 * Start over from beginning.
 */

#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/wdt.h>

#define NUMCHANNELS 6
#define HEADERLEN 4
#define PACKETLEN (HEADERLEN + NUMCHANNELS * 2 + 1)

#define FOSC 7372800		// Clock Speed
#define SAMPFREQ 256
#define TIMER0VAL 256 - ((FOSC / 256) / SAMPFREQ)

#define BAUD 57600
#define BAUDL FOSC/16/BAUD - 1
#define BAUDH (BAUDL)>>8


#if defined (__AVR_ATmega48__) || defined (__AVR_ATmega88__)
	#define SIG_UART_DATA SIG_USART_DATA
	#define	ADCSR	ADCSRA
	#define UCSRB	UCSR0B
	#define UDRIE	UDRIE0
	#define UDR 	UDR0
	#define TIMSK	TIMSK0
	#define TCCR0	TCCR0B
#endif

//char const channel_order[] = {0, 3, 1, 4, 2, 5};
char const channel_order[] = {0, 1, 2, 3, 4, 5};

// The transmission packet
volatile uint8_t TXBuf[PACKETLEN];

// Next byte to read or write in the transmission packet
volatile uint8_t TXIndex;

// Current channel being sampled
volatile uint8_t CurrentCh;

/////////////////////////////////////////////
// Sampling timer (timer 0) interrupt handler
SIGNAL(SIG_OVERFLOW0){
	outb(TCNT0, TIMER0VAL); //Reset timer to get correct sampling frequency
	CurrentCh = 0;

	// Write header and footer:
	// Increase packet counter (fourth byte in header)
	TXBuf[3]++;

	//Get state of switches on PD2..5, if any (last byte in packet)
	TXBuf[2 * NUMCHANNELS + HEADERLEN] = (inp(PIND) >> 2) & 0x0F;

	cbi(UCSRB, UDRIE);	//Ensure UART IRQ's are disabled
	sbi(ADCSR, ADIF);	//Reset any pending ADC interrupts
	sbi(ADCSR, ADIE);	//Enable ADC interrupts

	//The ADC will start sampling automatically as soon
	//as sleep is executed in the main-loop
}

///////////////////////////////////////////
// AD-conversion-complete interrupt handler
SIGNAL(SIG_ADC) {
	volatile uint8_t i;

	i = 2 * CurrentCh + HEADERLEN;

	TXBuf[i+1]	= inp(ADCL);	// Fill buffer from ADC
	TXBuf[i]	= inp(ADCH);

	CurrentCh++;
	if (CurrentCh < NUMCHANNELS) {
		outb(ADMUX, channel_order[CurrentCh]);  //Select the next channel
		//The next sampling is started automatically
	} else {
		outb(ADMUX, channel_order[0]);	//Prepare next conversion, on channel 0

	        // Disable ADC interrupts to prevent further calls to SIG_ADC
		cbi(ADCSR,  ADIE);

		// Hand over to SIG_UART_DATA, by starting
		// the UART transfer and enabling UDR IRQ's
		outb(UDR,  TXBuf[0]);
		sbi(UCSRB, UDRIE);

		TXIndex = 1;
	}
}

//////////////////////////////////////////////////////////
// UART data transmission register-empty interrupt handler
SIGNAL(SIG_UART_DATA) {
	outb(UDR, TXBuf[TXIndex]);	//Send next byte

	TXIndex++;
	if (TXIndex == PACKETLEN){	//See if we're done with this packet
		cbi(UCSRB, UDRIE);		//Disable SIG_UART_DATA interrupts
		//Next interrupt will be a SIG_OVERFLOW0
	}
}

////////////////////////////////////////////////////////
// Initialize PWM output (PB1 = 14Hz square wave signal)
void pwm_init(void) {
	// Set timer/counter 1 to use 10-bit PWM mode.
	// The counter counts from zero to 1023 and then back down
	// again. Each time the counter value equals the value
	// of OCR1(A), the output pin is toggled.
	// The counter speed is set in TCCR1B, to clk / 256 = 28800Hz.
	// Effective frequency is then clk / 256 / 2046 = 14 Hz

#if defined (__AVR_ATmega8__) || defined (__AVR_ATmega48__) || defined (__AVR_ATmega88__)
	outb(OCR1AH, 2);		// Set OCR1A = 512
	outb(OCR1AL, 0);
	outb(TCCR1A, BV(COM1A1) + BV(WGM11) + BV(WGM10)); // Set 10-bit PWM mode
	outb(TCCR1B, (1 << CS12));	// Start and let run at clk / 256 Hz
#else	//__AVR_AT90S4433
	outb(OCR1H, 2);		// Set OCR1 = 512
	outb(OCR1L, 0);
	outb(TCCR1A, BV(COM11) + BV(PWM11) + BV(PWM10)); // Set 10-bit PWM mode
	outb(TCCR1B, (1 << CS12));	// Start and let run at clk / 256 Hz
#endif
}

////////////////
// Initialize uC
void init(void) {
	//Set up the ports
	outb(DDRD,  0xC2);
	outb(DDRB,  0x07);
	outb(PORTD, 0xFF);
	outb(PORTB, 0xFF);

	//Select sleep mode = idle
#if defined (__AVR_ATmega48__) || defined (__AVR_ATmega88__)
	outb(SMCR, (inp(SMCR)  | BV(SE)) & (~BV(SM0) | ~BV(SM1) | ~BV(SM2)));
#elif defined (__AVR_ATmega8__)
	outb(MCUCR,(inp(MCUCR) | BV(SE)) & (~BV(SM0) | ~BV(SM1) | ~BV(SM2)));
#else // __AVR_AT90S4433__
	outb(MCUCR,(inp(MCUCR) | BV(SE)) & (~BV(SM)));
#endif

	// Initialize the ADC
	// Timings for sampling of one 10-bit AD-value:
	// prescaler > ((XTAL / 200kHz) = 36.8 =>
	// prescaler = 64 (ADPS2 = 1, ADPS1 = 1, ADPS0 = 0)
	// ADCYCLE = XTAL / prescaler = 115200Hz or 8.68 us/cycle
	// 14 (single conversion) cycles = 121.5 us (8230 samples/sec)
	// 26 (1st conversion) cycles = 225.69 us
	outb(ADMUX, 0);		//Select channel 0

	//Prescaler = 64, free running mode = off, interrupts off
	outb(ADCSR, BV(ADPS2) | BV(ADPS1));
	sbi(ADCSR, ADIF);	//Reset any pending ADC interrupts
	sbi(ADCSR, ADEN);	//Enable the ADC

	// Analog comparator OFF
	outb(ACSR, BV(ACD));

	//Initialize the UART
#if defined (__AVR_ATmega48__) || defined (__AVR_ATmega88__)
	outb(UBRR0H, BAUDH);		//Set speed to BAUD bps
	outb(UBRR0L, BAUDL);
	outb(UCSR0A, 0);
	outb(UCSR0C, BV(UCSZ01) | BV(UCSZ00));
	outb(UCSR0B, BV(TXEN0));	//Enable transmitter
#elif defined (__AVR_ATmega8__)
	outb(UBRRH, BAUDH);			//Set speed to BAUD bps
	outb(UBRRL, BAUDL);
	outb(UCSRA, 0);
	outb(UCSRC, BV(URSEL) | BV(UCSZ1) | BV(UCSZ0));
	outb(UCSRB, BV(TXEN));
#else // __AVR_AT90S4433__
	outb(UBRR, BAUDL);			//Set speed to BAUD bps
	outb(UCSRB, BV(TXEN));
#endif

	// Initialize timer 0
	outb(TCNT0, 0);			//Clear it
	outb(TCCR0, 4);			//Start it. Frequency = clk / 256
	outb(TIMSK, BV(TOIE0));		//Enable the interrupts
}

//////////////////////////////////////////////////////////
int main(void) {

	// Write packet header
	TXBuf[0] = 0xA5;	//Sync 0
	TXBuf[1] = 0x5A;	//Sync 1
	TXBuf[2] = 2;		//Protocol version
	TXBuf[3] = 0;		//Packet counter

	// Initialize
	init();

	// Initialize PWM (optional)
	pwm_init();

	sei();
	// Now, we wait
	// This is an event-driven program,
	// so nothing much happens here
	while (1) {
		__asm__ __volatile__ ("sleep");	// sleep until something happens
	}
}
