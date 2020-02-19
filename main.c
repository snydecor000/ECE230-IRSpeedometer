/*
 * File:   speedometer.c
 * Authors: Conner Ozatalar and Cory Snyder
 *
 * Final Project for Rose-Hulman ECE230 2019 Winter
 * 
 * The purpose of this program is to measure the average speed of objects 
 * using the PIC16F887.  This is accomplished using two IR photo gates, each of 
 * which is composed of an IRLED and an IR Transistor. 
 * 
 * The program starts off by asking for an 8 bit char on the LCD:
 * The bits of the character represent the inches between the two photo gates
 * RB1 and RB2 have buttons which correspond to 1 and 0 respectively, and 
 * the user enters bits from the MSB to the LSB
 * 
 * The format for the inches character is as follows: XXXXX.XXX 
 * It is unsigned and the last 3 bits represent fractional binary
 * This means that the distance between can be anywhere from 0 to 31.875 in
 * 
 * Then, the main loop starts and the gates' analog voltages are being constantly
 * read in order to detect objects that pass through.
 * 
 * Once an object has reached the first gate, it begins a counter that keeps 
 * track of time to the nearest .1 ms.  This counter is stopped as soon as the 
 * object reaches the second gate.  The average speed in miles per hour is then
 * calculated and displayed on the LCD screen and the program waits for the 
 * reset button to be pressed before another trial can begin
 * 
 * Created on February 13, 2020, 10:03 AM
 */

// CONFIG1
#pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator: High-speed crystal/resonator on RA6/OSC2/CLKOUT and RA7/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled and can be enabled by SWDTEN bit of the WDTCON register)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = ON       // RE3/MCLR pin function select bit (RE3/MCLR pin function is MCLR)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)
#pragma config BOREN = OFF      // Brown Out Reset Selection bits (BOR disabled)
#pragma config IESO = OFF       // Internal External Switchover bit (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enabled bit (Fail-Safe Clock Monitor is disabled)
#pragma config LVP = OFF        // Low Voltage Programming Enable bit (RB3 pin has digital I/O, HV on MCLR must be used for programming)

// CONFIG2
#pragma config BOR4V = BOR21V   // Brown-out Reset Selection bit (Brown-out Reset set to 2.1V)
#pragma config WRT = OFF        // Flash Program Memory Self Write Enable bits (Write protection off)

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

#include <xc.h>
#include "lcd4bits.h"

#define GateTransistor1 13
#define GateTransistor2 11

void ADC_init(void);
void Timer_CCP_init(void);
void Port_init(void);
unsigned int ADC_convert(unsigned char);
void debounce(void);
void __interrupt() interrupt_handler(void);

unsigned long timer = 0;        //Each tick of this is 0.1 ms, records time
unsigned char isRecording = 0;  //Variable only set when in the middle of a trial
unsigned char enteredInches = 0;//char of user inches input 0bXXXXX.XXX
unsigned long inches = 0;       //Inches * 10000 entered by the user in init
unsigned char inchDigits[5];   
unsigned char mphDigits[6];//characters representing mph

void main(void) {
    lcd_init();
    ADC_init();
    Timer_CCP_init();
    Port_init();
    
    ////////////////////////////////////////////////////////////////////////////
    //Initialization: Ask for distance between gates
    //                Display it
    ////////////////////////////////////////////////////////////////////////////
    lcd_goto(0);
    lcd_puts("Enter 0b Dist.");
    lcd_goto(0x40);//Second Line of LCD
    
    for(int x = 0;x < 8; x++) {//Get the 8 bits of the inches byte
        while(RB1 == 1 && RB2 == 1);//wait for the 1 button or 0 button
        debounce();
        if(RB1 == 0){//The 1 button is pressed
            CARRY = 0;//clear carry to not accidentally carry a 1 in
            enteredInches <<= 1;//Shift a 1 in
            enteredInches++;
        } if(RB2 == 0) {//The 0 button is pressed
            CARRY = 0;//clear carry to not accidentally carry a 1 in
            enteredInches <<= 1;//Shift a 0 in
        }
        while(RB1 == 0 || RB2 == 0);//wait for both buttons to be released
        debounce();
        lcd_putch(0x30 + (enteredInches & 0x01));//Put the "0/1" on the LCD
    }
    
    ////////////////////////////////////////////////////////////////////////////
    //Conversion: Convert the character representing inches from 0bXXXXX.XXX
    //            to decimal inches * 10000 in preparation for future calcs
    //            Output the result to the LCD in the top right where it will
    //            stay for the rest of the program
    ////////////////////////////////////////////////////////////////////////////
    unsigned int fractional = 0;//fractional part of inches
    unsigned long integer = 0;//integer part of inches
        
    integer = (long)((enteredInches & 0xF8) >> 3)*10000;//Mask out the fraction
    
    if(enteredInches & 0x01){//remainder 1/8 th
        fractional += 1250; 
    }if(enteredInches & 0x02){//remainder 1/4 th
        fractional += 2500;
    }if(enteredInches & 0x04){//remainder 1/2 th
        fractional += 5000;
    }
    
    inches = fractional + integer;
    unsigned long inchesC = inches/10;
    //Extract inches as 5 decimal digits
    for(char x = 0;x < 5;x++){
        inchDigits[x] = inchesC % 10;  
        inchesC = inchesC / 10;
    }
    
    lcd_clear();
    lcd_goto(0x09);//Display the number of inches the user inputed
    lcd_putch(inchDigits[4] + 0x30);
    lcd_putch(inchDigits[3] + 0x30);
    lcd_putch('.');
    lcd_putch(inchDigits[2] + 0x30);
    lcd_putch(inchDigits[1] + 0x30);
    lcd_putch(inchDigits[0] + 0x30);
    lcd_putch(0x22); //'"' quotation mark for inches
    
    ////////////////////////////////////////////////////////////////////////////
    //Main loop: Initially wait for the reset button to be pressed
    //
    //           Then start reading the analog values of both photo-transistors
    //           in order to detect an object passing through.
    //
    //           Once an object has passed through the first gate, it enables
    //           the interrupt to allow for the timer variable to accumulate.
    //           Then when the object passes through the second gate, it stops  
    //           the interrupts
    //
    //           With the timer variable, the average miles per hour of the 
    //           object is calculated and displayed on the LCD.
    //           The program then waits for the reset button in order to start 
    //           the next trial
    ////////////////////////////////////////////////////////////////////////////
    while(RB0 == 1);//wait for start button to be pressed
    debounce();
    while(RB0 == 0);//wait for start button to be released
    
    while(1){//Main Loop
        lcd_goto(0);
        lcd_puts("1: ");
        unsigned int photoVal1 = ((unsigned long)ADC_convert(GateTransistor1)*500)/1023;
        DisplayVolt(photoVal1);
        lcd_goto(0x40);
        unsigned int photoVal2 = ((unsigned long)ADC_convert(GateTransistor2)*500)/1023;
        lcd_puts("2: ");
        DisplayVolt(photoVal2);
        
        if(photoVal2 > 300 && isRecording == 0){//First gate becomes blocked
            PEIE = 1;
            timer = 0;
            isRecording = 1;
        }
        
        if(photoVal1 > 300 && isRecording == 1){//Second gate becomes blocked
            PEIE = 0;
            isRecording = 0;
            
            //100*mph,2 decimal points
            unsigned long mph = (unsigned long)(inches*125)/(unsigned long)(22*timer);

            // convert mph value to an array of digits
            for (char i = 0; i < 5; i++) {
                mphDigits[4 - i] = (mph % 10);
                mph = mph / 10;
            }

            //output mph to LCD
            lcd_goto(0x48);
            lcd_puts("v:");
            lcd_putch(mphDigits[0] + 0x30);
            lcd_putch(mphDigits[1] + 0x30);
            lcd_putch(mphDigits[2] + 0x30);
            lcd_putch('.');//decimal point
            lcd_putch(mphDigits[3] + 0x30);
            lcd_putch(mphDigits[4] + 0x30);
            
            while(RB0 == 1);//Wait for restart button to be pressed
        }
    }
}

void Timer_CCP_init() {    
    //TMR1
    TMR1GE = 0; TMR1ON = 1; //Turn TMR1 on with gate enable off
    TMR1CS = 0;             //freq = (Fosc/4) = 2 MHz
    T1CKPS1 = 0;T1CKPS0 = 1;//Prescale down to 1 MHz clock pulse
    TMR1IE = 0;             //Enable Timer 1 Interupt
    TMR1IF = 0;             //Reset Timer 1 flag
    
    //CCP1
	CCP1M3 = 1;CCP1M2 = 0;      //Set CCP1 to "Compare with software interrupt"
    CCP1M1 = 1;CCP1M0 = 0; 	 
	CCPR1 = TMR1 + 100;
	CCP1IF = 0;                 //Reset CCP1 flag
	CCP1IE = 1;					//Unmask (enable) Compare Interrupt from CCP1
    
    PEIE = 0;//Disable peripheral interrupts for now
    GIE = 1;//Enable global interrupts
}

void Port_init() {
    TRISB0 = 1;//RB0,1,2 inputs
    TRISB1 = 1;
    TRISB2 = 1;
    nRBPU = 0;//Enable Pull-ups
    ANS8 = 0;//RB0,1,2 digital not analog
    ANS10 = 0;
    ANS12 = 0;
}

void ADC_init(void) {
    TRISB4 = 1; //Make RB4/AN11 and RB5/AN13 inputs
    TRISB5 = 1;
    ANS11 = 1;  //Configure as digital
    ANS13 = 1; 

    ADON = 1;   // Turn on ADC Module
    ADFM = 1;   // Right justify result in ADRES = ADRESH:ADRESL registers
    VCFG1 = 0;  // Use VSS and VDD to set conversion range to 0V-5V
    VCFG0 = 0;  
    ADCS1 = 0;  // ADC clock freq set to Fosc/2 = 4 MH (Fosc=8MHz)
    ADCS0 = 0;
}

unsigned int ADC_convert(unsigned char analogCh) {
    //select the correct analogChannel
    switch(analogCh) {
        case 11: 
            CHS3 = 1; CHS2 = 0; CHS1 = 1; CHS0 = 1;
            break;
        case 13: 
            CHS3 = 1; CHS2 = 1; CHS1 = 0; CHS0 = 1;
            break;
        default:	
            CHS3 = 0; CHS2 = 0; CHS1 = 0; CHS0 = 0;
            break;
    }
    GO = 1; // Start conversion ("GO" is the GO/DONE* bit)
    while (GO == 1); // Wait here while converting
    return (unsigned int) ADRESH * 256 + ADRESL; //converted 10-bit value (0 -> 1023)
}

void debounce() {
    for(int x = 0;x<6000;x++);//6000 instructions is approx 6ms
}

void __interrupt() interrupt_handler() {
    if(CCP1IF == 1){
        CCPR1 = TMR1 + 100;
        timer++;
        CCP1IF = 0;
    }
}
