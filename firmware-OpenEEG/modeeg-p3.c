/*
 * ModularEEG firmware for one-way transmission, v0.5.5-p3
 * Copyright (c) 2002-2003, Joerg Hansmann, Jim Peters, Andreas Robinson
 * Modification for Atmega48 (c) 2005, Yuri Smolyakov
 * License: GNU General Public License (GPL) v2
 * Compiles with AVR-GCC v3.3.
 *
 * Note: -p3 in the version number means this firmware is for packet version 3.
 */

//////////////////////////////////////////////////////////////

/*
 * ModularEEG Packet Format Version 3
 *
 * One packet can have zero, two, four or six channels (or more).
 * The default is a 6-channel packet, shown below.
 *
 * 0ppppppx     packet header
 * 0xxxxxxx
 *
 * 0aaaaaaa     channel 0 LSB
 * 0bbbbbbb     channel 1 LSB
 * 0aaa-bbb     channel 0 and 1 MSB
 *
 * 0ccccccc     channel 2 LSB
 * 0ddddddd     channel 3 LSB
 * 0ccc-ddd     channel 2 and 3 MSB
 *
 * 0eeeeeee     channel 4 LSB
 * 0fffffff     channel 5 LSB
 * 1eee-fff     channel 4 and 5 MSB
 *
 * Key:
 *
 * 1 and 0 = sync bits.
 *   Note that the '1' sync bit is in the last byte of the packet,
 *   regardless of how many channels are in the packet.
 * p = 6-bit packet counter
 * x = auxillary channel byte
 * a - f = 10-bit samples from ADC channels 0 - 5
 * - = unused, must be zero
 *
 * There are 8 auxillary channels that are transmitted in sequence.
 * The 3 least significant bits of the packet counter determine what
 * channel is transmitted in the current packet.
 *
 * Aux Channel Allocations:
 *
 * 0: Zero-terminated ID-string (ASCII encoded).
 * 1:
 * 2:
 * 3:
 * 4: Port D status bits
 * 5:
 * 6:
 * 7:
 *
 * The ID-string is currently "mEEGv1.0".
 */

//////////////////////////////////////////////////////////////

/*
 *
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

// Number of channels. 2, 4 or 6
#define NUMCHANNELS 6
// Sample frequency in Hz
#define SAMPFREQ 256

#define HEADERLEN 2
#define PACKETLEN (3 * (NUMCHANNELS >> 1) + HEADERLEN)

#define FOSC 7372800			// Clock Speed
#define TIMER0VAL 256 - ((FOSC / 256) / SAMPFREQ)

#define BAUD 57600
#define BAUDL FOSC/16/BAUD - 1
#define BAUDH (BAUDL)>>8

#if defined (__AVR_ATmega48__) || defined (__AVR_ATmega88__)
	#define SIG_UART_DATA SIG_USART_DATA
	#define	ADCSR	ADCSRA
	#define UCSRB	UCSR0B
	#define UDRIE	UDRIE0
	#define UDR	UDR0
	#define TIMSK	TIMSK0
	#define TCCR0	TCCR0B
#endif

const char channel_order[]= {0, 3, 1, 4, 2, 5};

// The transmission packet
uint8_t tx_buf[PACKETLEN];

// Next byte to read or write in the transmission packet
register uint8_t tx_index asm("r3");

// ID string
const char id_string[] = "mEEGv1.0";

// ID string index
register uint8_t id_index asm("r4");

// Packet counter
register uint8_t packet_cnt asm("r5");

// Current channel being sampled
register uint8_t cur_channel asm("r6");

/////////////////////////////////////////////
// Sampling timer (timer 0) interrupt handler
SIGNAL(SIG_OVERFLOW0) {
	outb(TCNT0, TIMER0VAL);  //Reset timer to get correct sampling frequency
	cur_channel = 0;

	cbi(UCSRB, UDRIE);	//Ensure UART IRQ's are disabled
	sbi(ADCSR, ADIF);	//Reset any pending ADC interrupts
	sbi(ADCSR, ADIE);	//Enable ADC interrupts

	//The ADC will start sampling automatically as soon
	//as sleep is executed in the main-loop.
}

///////////////////////////////////////////
// AD-conversion-complete interrupt handler
SIGNAL(SIG_ADC) {
	register uint8_t i asm("r20");
	register uint8_t lsb asm("r21");
	register uint8_t msb asm("r22");
	register uint8_t aux_data asm("r23");

	lsb = inp(ADCL);	// Read 8 least significant sample bits
	msb = inp(ADCH);	// Read 2 most significant sample bits

	// The assembler code below does the same as
	// msb = ((msb << 1) | (lsb >> 7)) & 0x3;
	// lsb = lsb & 0x7F;
	// but in a prettier way

	// The end result is
	// msb = 3 most significant bits of sample, unused bits clear
	// lsb = 7 least significant bits of sample, unused bit clear

	asm volatile (
		" rol %0"    "\n"
		" rol %1"    "\n"
		" lsr %0"    "\n"
		" andi %1,7" "\n"
		: "=r" (lsb), "=r" (msb)
		:  "r" (lsb),  "r" (msb)
        );

	i = (cur_channel >> 1);
	i = i+i+i + HEADERLEN;	// i = 3*i+2, but without multiplication

	if (cur_channel & 1) {	// Store odd channel
		tx_buf[i+1] = lsb;
		tx_buf[i+2] |=  msb;
	} else {		// Store even channel
		tx_buf[i] = lsb & 0x7F;
		tx_buf[i+2] = msb << 4;
	}

	cur_channel++;
	if (cur_channel < NUMCHANNELS) {
		outb(ADMUX, channel_order[cur_channel]);	//Select the next channel
		return;		//The next sampling is started automatically
	}

	outb(ADMUX, channel_order[0]);   //Prepare next conversion, on channel 0

	// Disable ADC interrupts to prevent further calls to SIG_ADC
	cbi(ADCSR, ADIE);

	// Write the packet heade.
	switch (packet_cnt & 7) {
	case 0:
		// Get the next character in the ID string
		aux_data = id_string[id_index++];
		if (aux_data == 0) id_index = 0;
		break;
	case 4:
		// Get the state of switches on PD2..5, if any
		aux_data = (inp(PIND) >> 2) & 0x0F;
		break;
	default:
		aux_data = 0;
	}

	// Store the header
	tx_buf[0] = ((packet_cnt << 1) & 0x7E) | (aux_data >> 7);
	tx_buf[1] = aux_data & 0x7F;

	// Set the sync bit
	tx_buf[PACKETLEN-1] |= 0x80;

	packet_cnt++;

	// Hand over to SIG_UART_DATA, by starting
	// the UART transfer and enabling UDR IRQ's
	outb(UDR,  tx_buf[0]);
	sbi(UCSRB, UDRIE);

	tx_index = 1;
}

//////////////////////////////////////////////////////////
// UART data transmission register-empty interrupt handler
SIGNAL(SIG_UART_DATA) {

	outb(UDR, tx_buf[tx_index]);	//Send next byte

	tx_index++;
	if (tx_index == PACKETLEN) {	//See if we're done with this packet
		cbi(UCSRB, UDRIE);      //Disable SIG_UART_DATA interrupts
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
#else // __AVR_AT90S4434__
	outb(OCR1H,2);			// Set OCR1 = 512
	outb(OCR1L,0);
	outb(TCCR1A, BV(COM11) + BV(PWM11) + BV(PWM10)); // Set 10-bit PWM mode
	outb(TCCR1B, (1 << CS12)); // Start and let run at clk / 256 Hz
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
	outb(SMCR,(inp(SMCR) | BV(SE)) & (~BV(SM0) | ~BV(SM1) | ~BV(SM2)));
#elif defined (__AVR_ATmega8__)
	outb(MCUCR,(inp(MCUCR) | BV(SE)) & (~BV(SM0) | ~BV(SM1) | ~BV(SM2)));
#else // __AVR_AT90S4434__
	outb(MCUCR,(inp(MCUCR) | BV(SE)) & (~BV(SM)));
#endif

	//Initialize the ADC

	// Timings for sampling of one 10-bit AD-value:
	// prescaler > ((XTAL / 200kHz) = 36.8 =>
	// prescaler = 64 (ADPS2 = 1, ADPS1 = 1, ADPS0 = 0)
	// ADCYCLE = XTAL / prescaler = 115200Hz or 8.68 us/cycle
	// 14 (single conversion) cycles = 121.5 us (8230 samples/sec)
	// 26 (1st conversion) cycles = 225.69 us
	outb(ADMUX, 0);         //Select channel 0

	//Prescaler = 64, free running mode = off, interrupts off
	outb(ADCSR, BV(ADPS2) + BV(ADPS1));
	sbi(ADCSR, ADIF);       //Reset any pending ADC interrupts
	sbi(ADCSR, ADEN);       //Enable the ADC

	// Analog comparator OFF
	outb(ACSR, BV(ACD));

	//Initialize the UART
#if defined (__AVR_ATmega48__) || defined (__AVR_ATmega88__)
	outb(UBRR0H, BAUDH);		//Set speed to BAUD bps
	outb(UBRR0L, BAUDL);
	outb(UCSR0A, 0);
	outb(UCSR0C, BV(UCSZ01) | BV(UCSZ00));
	outb(UCSR0B, BV(TXEN0));		//Enable transmitter
#elif defined (__AVR_ATmega8__)
	outb(UBRRH, BAUDH);		//Set serial port speed
	outb(UBRRL, BAUDL);
	outb(UCSRA, 0);
	outb(UCSRC, BV(URSEL) | BV(UCSZ1) | BV(UCSZ0));
	outb(UCSRB, BV(TXEN));
#else // __AVR_AT90S4434__
	outb(UBRR, BAUDL);          // Set serial port speed
	outb(UCSRB, BV(TXEN));
#endif

	//Initialize timer 0
	outb(TCNT0, 0);				// Clear it
	outb(TCCR0, 4);				// Start it. Frequency = clk / 256
	outb(TIMSK, 1 << TOIE0);	// Enable the interrupts
}

////////////////
int main(void) {
	packet_cnt = 0;
	id_index = 0;

	// Initialize
	init();

	//Initialize PWM (optional)
	pwm_init();

	sei();

	// Now, we wait
	// This is an event-driven program,
	// so nothing much happens here
	while (1) {
		__asm__ __volatile__ ("sleep");	// sleep until something happens
	}
}
