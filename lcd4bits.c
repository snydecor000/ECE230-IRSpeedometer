// Created by Hi-Tech Software Modified by Jianjian Song to work with PIC16F887, August 13, 2009
// Modified by Keith Hoover for easier use in ECE230, July 15, 2011
// Modified by Chris Miller for easier use in ECE230, September, 2014
/*
 *	"lcd4bits.c" LCD Display Panel Driver Routines
 *  Reference:  Refer to the Optrex LCD User Manual for all details and command formats
 *              used in these routines "Optrex LCD Manual Dmcman_full-user manual.pdf" 
 *
 *	All necessary LCD and delay functions have been bundled into this file (lcd4bit.c),
 *  and the necessary function prototype statements are bundled into the 
 *  companion header file, lcd4bit.h.  Therefore by including lcd4bit.c into 
 *  a project along with a separate "main.c" program that calls one or more of these
 *  the LCD panel display routines and contains an "#include "lcd4bit.h" header file,
 *  you need not cut and paste all of these routines into the main program file itself.
 *
 *	This code will interface to a wide variety of B/W LCD Display Controllers
 *	like the Hitachi HD44780. It uses the 4-bit transfer mode, and it assumes that the
 *      pins of PORT D (RD7:4) are connected to the LCD panel, with
 *	the hardware connected as follows, assuming the standard 16 pin LCD pinout:
 
 *    GND to VSS Pin of LCD Pin 1
 *    +5V to VDD Pin of LCD Pin 2
 *     ~  to V0 Pin of LCD Pin 3
 *    GND to R/W* Pin of LCD Pin 5 (This means that the software can only write to the LCD panel, and never
 *                                  read from it.)
 *	  RD4:7 are connected to the LCD data bits 4-7 LCD Pins 11-14
 *	  RD2 is connected to the LCD RS input (register select) LCD Pin 4
 *  	  RD3 is connected to the LCD EN bit (enable)  LCD Pin 6
 *	
 *	The available routines in this file are:
 *
 *    1.  lcd_init( ) 
 *        Always call lcd_init() first, which follows the manufacturer's
 *        directions for initializing the LCD display panel into 8-bit transfer mode.
 *        Then you may call any of the other routines, as needed.  Note that this
 *        initialization routine also makes RD7:0 all outputs, as required to drive
 *		  the LCD panel connected to RD7:0.  It also selects 8 MHz internal clock.
 *
 *    2.  lcd_clear( )
 *        Clears LCD display and homes the cursor
 *
 *    3.  lcd_puts(const char s*)
 *        writes a constant character string to the lcd panel
 *
 *    4.  lcd_putch(char s)
 *        writes a single character to lcd panel
 *
 *    5.  lcd_goto(unsigned char pos)
 *        Moves cursor to desired position.  For 16 x 2 LCD display panel, 
 *        the columns of Row 1 are 0x00....0x10
 *        the columns of Row 2 are 0x40....0x50
 *
 *    6.  DelayMs(unsigned int NrMs)
 *		  Delays for NrMs milliseconds.
 *
 */

#include	<xc.h>

#define CMD_MODE                0       //(0 for command mode)
#define DTA_MODE                1       //(1 for data mode)
#define LCD_RS                  RD2     //rename RD2 	-LCD command/data 
#define LCD_EN                  RD3     //rename RD3 	-LCD Enable (clocks data into LCD on falling edge)
#define LCD_DATA                PORTD   //rename PORTD 	-LCD Data pins (D4 - D7)
#define LCDCMD_ClearDisplay     0x01    //clear display: clear, move cursor home
#define LCDCMD_EMS              0x06    //entry mode set: auto increment cursor after each char sent
#define LCDCMD_DisplaySettings  0x0C    //display ON/OFF control: display on, cursor off, blink off
#define LCDCMD_FunctionSet      0x28    //function set: 4-bit mode, 2 lines, 5x7 dots

void tmr0_init() {
    // T0CS = 0 -- Fosc/4 clock source
    // PSA = 0 -- prescalar set to TMR0
    // PS = 0 -- 1:2
    OPTION_REG &= 0xC0;
    // PS = 1 -- 1:8
    OPTION_REG |= 0x02;
}

/*
 * Delay for indicated number of milliseconds
 *  (256 - 9)*8 + 24 instruction cycles => 2000 instruction cycles (1ms)
 *  assumes PS 1:8, 8MHz clock
 */
void DelayMs(unsigned int millis) {
    while (millis != 0) {
        TMR0 = 9;
        T0IF = 0;
        while(T0IF == 0);
        millis--;
    }
}

/*
 * Provides delay of ~40 instruction cycles (20us)
 *  assumes 8MHz clock
 */
void Delay20us() {
    unsigned char next;
    for (next = 0; next < 3; next++);
}

/*
 * Pulses (strobes) the Enable line making it rise and then fall. This
 *  falling edge writes data on LCD Panel pins DB7:0 into the LCD Panel.
 */
void LCD_strobe() {
    LCD_EN = 1;
    Delay20us();
    LCD_EN = 0;
}

/*
 * lcd_write function ---writes a byte to the LCD in 8-bit mode.
 * Note that the "mode" argument is set to either CMD_MODE (=0) or DTA_MODE (=1), 
 * so that the LCD panel knows whether an instruction byte is being written to it 
 * or an ASCII code is being written to it that is to be displayed on the panel.
 */ 
void lcd_write(unsigned char mode, unsigned char Char) {
    LCD_RS = mode;
    
    //UPPER NIBBLE TRANSMIT
    LCD_DATA = (LCD_DATA & 0b00001111) | (Char & 0b11110000);
    LCD_strobe(); // Write 4 upper bits of data on D7-4
    Delay20us();
    
    //LOWER NIBBLE TRANSMIT
    LCD_DATA = (LCD_DATA & 0b00001111) | ((Char & 0b00001111)<<4);
    LCD_strobe(); // Write 4 lower bits of data on D7-4
    Delay20us();
}

/*
 * Clear the LCD and go to home position
 */
void lcd_clear(void) {
    lcd_write(CMD_MODE, LCDCMD_ClearDisplay);
    DelayMs(2);
}

/* Write a string of chars to the LCD */
void lcd_puts(const char *string) {
    while (*string != 0) // Last character in a C-language string is alway "0" (ASCII NULL character)
        lcd_write(DTA_MODE, *string++);
}

/* Write one character to the LCD */
void lcd_putch(char character) {
    lcd_write(DTA_MODE, character);
}

/*
 * Moves cursor to desired position.  
 * For 16 x 2 LCD display panel, 
 *     the columns of Row 1 are 0x00....0x10
 *     the columns of Row 2 are 0x40....0x50
 */
void lcd_goto(unsigned char position) {
    lcd_write(CMD_MODE, 0x80 + position); // The "cursor move" command is indicated by MSB=1 (0x80)
    // followed by the panel position address (0x00- 0x7F)
    DelayMs(2);
}

/*
 * Initialize the LCD - put into 8 bit mode
 */
void lcd_init() //See Section 2.2.2.2 of the Optrex LCD DMCman User Manual
{
    unsigned char cmd;
    TRISD = 0b00000000; //Make PORTD (D7-4) all output
    TRISD2 = 0; //Make RC4 (RS) output
    TRISD3 = 0; //Make RC5 (EN) output
    tmr0_init();
    LCD_RS = CMD_MODE;
    LCD_EN = 0;
    DelayMs(50); // wait 50mSec after power applied,
    lcd_write(CMD_MODE, LCDCMD_FunctionSet); // function set: 4-bit mode, 2 lines, 5x7 dots
    lcd_write(CMD_MODE, LCDCMD_DisplaySettings); // display ON/OFF control: display on, cursor off, blink off
    lcd_clear(); // Clear screen
    lcd_write(CMD_MODE, LCDCMD_EMS); // Set entry Mode
}

/*
 * Displays voltage on LCD
 * volts - volts in 100ths of volt
 * valid range : 0 -> 500
 * display: X.XX Volts
 */
void DisplayVolt(unsigned int volts) {
    unsigned char digits[3];
    unsigned char i;
    
    // convert voltage value to an array of digits
    for (i = 0; i < 3; i++) {
        digits[2 - i] = (volts % 10);
        volts = volts / 10;
    }
    
    //output significant digit left of decimal point
    lcd_putch(digits[0] + 0x30);
    lcd_putch('.');
    //put last 2 digits after decimal point
    lcd_putch(digits[1] + 0x30);
    lcd_putch(digits[2] + 0x30);
}