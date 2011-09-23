/*
 * Serial.c - Functions for the serial port PC interface on the 
 * Robotis CM-510 controller. 
 * Can either use serial cable or Zig2Serial via Zig-110
 *
 * Based on the embedded C library provided by Robotis  
 * Version 0.4		30/09/2011
 * Modified by Peter Lanius
 * Please send suggestions and bug fixes to peter_lanius@yahoo.com.au
 * 
*/

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <string.h>
#include <ctype.h>
#include "global.h"
#include "serial.h"


// Command Strings List - kept in Flash to conserve RAM
const char COMMANDSTR0[]  PROGMEM = "STOP";
const char COMMANDSTR1[]  PROGMEM = "WF  ";
const char COMMANDSTR2[]  PROGMEM = "WB  ";
const char COMMANDSTR3[]  PROGMEM = "WLT ";
const char COMMANDSTR4[]  PROGMEM = "WRT ";
const char COMMANDSTR5[]  PROGMEM = "WLS ";
const char COMMANDSTR6[]  PROGMEM = "WRS ";
const char COMMANDSTR7[]  PROGMEM = "WFLS";
const char COMMANDSTR8[]  PROGMEM = "WFRS";
const char COMMANDSTR9[]  PROGMEM = "WBLS";
const char COMMANDSTR10[] PROGMEM = "WBRS";
const char COMMANDSTR11[] PROGMEM = "WAL ";
const char COMMANDSTR12[] PROGMEM = "WAR ";
const char COMMANDSTR13[] PROGMEM = "WFLT";
const char COMMANDSTR14[] PROGMEM = "WFRT";
const char COMMANDSTR15[] PROGMEM = "WBLT";
const char COMMANDSTR16[] PROGMEM = "WBRT";
const char COMMANDSTR17[] PROGMEM = "SIT ";
const char COMMANDSTR18[] PROGMEM = "BAL ";
const char COMMANDSTR19[] PROGMEM = "M   ";
PGM_P COMMANDSTR_POINTER[] PROGMEM = { 
COMMANDSTR0, COMMANDSTR1, COMMANDSTR2, COMMANDSTR3, COMMANDSTR4,
COMMANDSTR5, COMMANDSTR6, COMMANDSTR7, COMMANDSTR8, COMMANDSTR9,
COMMANDSTR10, COMMANDSTR11, COMMANDSTR12, COMMANDSTR13, COMMANDSTR14, 
COMMANDSTR15, COMMANDSTR16, COMMANDSTR17, COMMANDSTR18, COMMANDSTR19 };

// set up the read buffer
volatile unsigned char gbSerialBuffer[MAXNUM_SERIALBUFF] = {0};
volatile unsigned char gbSerialBufferHead = 0;
volatile unsigned char gbSerialBufferTail = 0;
static FILE *device;

// global variables
extern volatile uint8 bioloid_command;			// current command
extern volatile uint8 last_bioloid_command;		// last command
extern volatile uint8 flag_receive_ready;		// received complete command flag
extern volatile uint8 current_motion_page;		// current motion page
extern volatile uint8 last_motion_page;			// last motion page (might still be in progress)

// internal function prototypes
void serial_put_queue( unsigned char data );
unsigned char serial_get_queue(void);
int std_putchar(char c,  FILE* stream);
int std_getchar( FILE* stream );


// ISR for serial receive, Serial Port/ZigBEE use USART1
SIGNAL(USART1_RX_vect)
{
	char c;
	
	c = UDR1;
	// check if we have received a CR+LF indicating complete string
	if (c == '\r')
	{
		// command complete, set flag and write termination byte to buffer
		flag_receive_ready = 1;
		serial_put_queue( 0xFF );
		c = '\n';
		std_putchar(c, device);
		// test
		std_putchar(' ', device);
	} 
	else
	{
		// put each received byte into the buffer until full
		serial_put_queue( c );
		// echo the character
		std_putchar(c, device);
	}
}

// initialize the serial port with the specified baud rate
void serial_init(long baudrate)
{
	unsigned short Divisor;

	// set UART register A
	//Bit 7: USART Receive Complete
	//Bit 6: USART Transmit Complete
	//Bit 5: USART Data Register Empty 
	//Bit 4: Frame Error
	//Bit 3: Data OverRun
	//Bit 2: Parity Error
	//Bit 1: Double The USART Transmission Speed
	//Bit 0: Multi-Processor Communication Mode
	UCSR1A = 0b01000010;
	
	// set UART register B
	// bit7: enable rx interrupt
    // bit6: enable tx interrupt
    // bit4: enable rx
    // bit3: enable tx
    // bit2: set sending size(0 = 8bit)
	UCSR1B = 0b10011000;
	
	// set UART register C
	// bit6: communication mode (1 = synchronous, 0 = asynchronous)
    // bit5,bit4: parity bit(00 = no parity) 
    // bit3: stop bit(0 = stop bit 1, 1 = stop bit 2)
    // bit2,bit1: data size(11 = 8bit)
	UCSR1C = 0b00000110;

	// Set baud rate
	Divisor = (unsigned short)(2000000.0 / baudrate) - 1;
	UBRR0H = (unsigned char)((Divisor & 0xFF00) >> 8);
	UBRR0L = (unsigned char)(Divisor & 0x00FF);

	// initialize
	UDR1 = 0xFF;
	gbSerialBufferHead = 0;
	gbSerialBufferTail = 0;

	// open the serial device for printf()
	device = fdevopen( std_putchar, std_getchar );
	
	// reset commands and flags
	bioloid_command = 0;				
	last_bioloid_command = 0;		
	flag_receive_ready = 0;			
}

// Top level serial port task
// manages all requests to read from or write to the serial port
// Receives commands from the serial port and writes output (excluding printf)
// Checks the status flag provided by the ISR for operation
void SerialReceiveCommand()
{
	char c1, c2, c3, c4, command[6], buffer[6];
	int match;
	
	if (flag_receive_ready == 0)
	{
		// nothing to do, go straight back to main loop
		return;
	}
	
	// we have a new command, get characters 
	c1 = serial_get_queue();
	if ( c1 >= 'a' && c1 <= 'z' ) c1 = toupper(c1); // convert to upper case if required
	command[0] = c1;
	c2 = serial_get_queue();
	if ( c2 >= 'a' && c2 <= 'z' ) c2 = toupper(c2); // convert to upper case if required
	if( c2 == 0xFF ) c2 = ' ';						// pad with blanks to 4 characters
	command[1] = c2;				
	c3 = serial_get_queue();
	if ( c3 >= 'a' && c3 <= 'z' ) c3 = toupper(c3); // convert to upper case if required
	if( c3 == 0xFF ) c3 = ' ';						// pad with blanks to 4 characters
	command[2] = c3;
	c4 = serial_get_queue();
	if ( c4 >= 'a' && c4 <= 'z' ) c4 = toupper(c4); // convert to upper case if required
	if( c4 == 0xFF ) c4 = ' ';						// pad with blanks to 4 characters
	command[3] = c4;
	command[4] = 0x00;			// finish the string

	// flush the queue in case we received 4 bytes
	serial_get_queue();
	
	// loop over all known commands to find a match
	for (uint8 i=0; i<NUMBER_OF_COMMANDS; i++)
	{
		strcpy_P(buffer, (PGM_P)pgm_read_word(&(COMMANDSTR_POINTER[i])));
		match = strcmp(	buffer, command	);
		if ( match == 0 )
		{
			// we found a match set the command
			last_bioloid_command = bioloid_command;
			bioloid_command = i;
			break;
		} 
		
		// if we get to end of loop we haven't found a match
		if ( i== NUMBER_OF_COMMANDS-1 )
		{
			// 0xFF means no match found
			bioloid_command = COMMAND_NOT_FOUND;
		}
	}
	
	// before we leave we need to check for special case of Motion Page command
	if( bioloid_command == COMMAND_NOT_FOUND ) 
	{
		if ( c1 == 'M' && (c2 >= '0' && c2 <= '9') )
		{
			// we definitely have a motion page, find the number
			bioloid_command = COMMAND_MOTIONPAGE;
			last_motion_page = current_motion_page;
			current_motion_page = c2 - 48;	// converts ASCII to number
			// check if next character is still a number
			if ( c3 >= '0' && c3 <= '9' )
			{
				current_motion_page = current_motion_page * 10;
				current_motion_page += (c3-48); 
			}
			// check if next character is still a number
			if ( c4 >= '0' && c4 <= '9' )
			{
				current_motion_page = current_motion_page * 10;
				current_motion_page += (c4-48); 
			}
		}
	}
	
	// reset the flag
	flag_receive_ready = 0;
	
	// finally echo the command and write new command prompt
	if ( bioloid_command == COMMAND_MOTIONPAGE ) {
		printf( "%c%c%c%c - MotionPageCommand %i\n> ", c1, c2, c3, c4, current_motion_page );
	} else if( bioloid_command != COMMAND_NOT_FOUND ) {
		printf( "%c%c%c%c - Command # %i\n> ", c1, c2, c3, c4, bioloid_command );
	} else {
		printf( "%c%c%c%c \nUnknown Command! \n> ", c1, c2, c3, c4 );
	}	
}


// write out a data string to the serial port
void serial_write( unsigned char *pData, int numbyte )
{
	int count;

	for( count=0; count<numbyte; count++ )
	{
		// wait for the data register to empty
		while(!bit_is_set(UCSR1A,5));
		// before writing the next byte
		UDR1 = pData[count];
	}
}

// read a data string from the serial port
unsigned char serial_read( unsigned char *pData, int numbyte )
{
	int count, numgetbyte;
	
	// buffer is empty, nothing to read 
	if( gbSerialBufferHead == gbSerialBufferTail )
		return 0;
	
	// check number of bytes requested does not exceed whats in the buffer
	numgetbyte = serial_get_qstate();
	if( numgetbyte > numbyte )
		numgetbyte = numbyte;
	
	for( count=0; count<numgetbyte; count++ )
		pData[count] = serial_get_queue();
	
	// return how many bytes have been read
	return numgetbyte;
}

// get the number of bytes in the buffer
int serial_get_qstate(void)
{
	short NumByte;
	
	if( gbSerialBufferHead == gbSerialBufferTail )
		// buffer is empty
		NumByte = 0;
	// buffer is used in cyclic fashion
	else if( gbSerialBufferHead < gbSerialBufferTail )
		// head is to the left of the tail
		NumByte = gbSerialBufferTail - gbSerialBufferHead;
	else
		// head is to the right of the tail
		NumByte = MAXNUM_SERIALBUFF - (gbSerialBufferHead - gbSerialBufferTail);
	
	return (int)NumByte;
}

// puts a received byte into the buffer
void serial_put_queue( unsigned char data )
{
	// buffer is full, character is ignored
	if( serial_get_qstate() == (MAXNUM_SERIALBUFF-1) )
		return;
	
	// append the received byte to the buffer
	gbSerialBuffer[gbSerialBufferTail] = data;

	// have reached the end of the buffer, restart at beginning
	if( gbSerialBufferTail == (MAXNUM_SERIALBUFF-1) )
		gbSerialBufferTail = 0;
	else
	// move the tail by one byte
		gbSerialBufferTail++;
}

// get a byte out of the buffer
unsigned char serial_get_queue(void)
{
	unsigned char data;
	
	// buffer is empty, return 0xFF
	if( gbSerialBufferHead == gbSerialBufferTail )
		return 0xff;
	
	// buffer not empty, return next byte	
	data = gbSerialBuffer[gbSerialBufferHead];
		
	// head has reached end of buffer, restart at beginning
	if( gbSerialBufferHead == (MAXNUM_SERIALBUFF-1) )
		gbSerialBufferHead = 0;
	else
	// move the head by one byte
		gbSerialBufferHead++;
		
	return data;
}

// writes a single character to the serial port
// newline is replaced with CR+LF
int std_putchar(char c,  FILE* stream)
{
	char tx[2];
	
    if( c == '\n' )
	{
        tx[0] = '\r';
		tx[1] = '\n';
		serial_write( (unsigned char*)tx, 2 );
	}
	else
	{
		tx[0] = c;
		serial_write( (unsigned char*)tx, 1 );
	}
 
    return 0;
}

// get a single character out of the read buffer
// wait for byte to arrive if none in the buffer
int std_getchar( FILE* stream )
{
    char rx;
	
	while( serial_get_qstate() == 0 );
	rx = serial_get_queue();
	
	if( rx == '\r' )
		rx = '\n';

    return rx;
}
