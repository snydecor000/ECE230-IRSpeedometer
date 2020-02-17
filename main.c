/*
 * File:   main.c
 * Author: snyderc1
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
unsigned int ADC_convert(unsigned char);
void __interrupt() interrupt_handler(void);
void Timer_CCP_init(void);

unsigned long timer = 0;
unsigned char currentlyTiming = 0;

void main(void) {
    lcd_init();
    ADC_init();
    Timer_CCP_init();
    TRISB0 = 1;
    nRBPU = 0;
    ANS12 = 0;
    
    while(1){
        lcd_goto(0);
        lcd_puts("1: ");
        unsigned int photoVal1 = ((unsigned long)ADC_convert(GateTransistor1)*500)/1023;
        DisplayVolt(photoVal1);
        lcd_goto(0x40);
        unsigned int photoVal2 = ((unsigned long)ADC_convert(GateTransistor2)*500)/1023;
        lcd_puts("2: ");
        DisplayVolt(photoVal2);
        
        if(photoVal2 > 300 && currentlyTiming == 0){
            //PEIE = 1;
            timer = 0;
            currentlyTiming = 1;
        }
        
        if(photoVal1 > 300 && currentlyTiming == 1){
            //PEIE = 0;
            currentlyTiming = 0;
                    
            unsigned char digits[6];
            unsigned char i;

            // convert voltage value to an array of digits
            for (i = 0; i < 6; i++) {
                digits[5 - i] = (timer % 10);
                timer = timer / 10;
            }

            //output significant digit left of decimal point
            lcd_putch(digits[0] + 0x30);
            lcd_putch(digits[1] + 0x30);
            lcd_putch(digits[2] + 0x30);
            lcd_putch(digits[3] + 0x30);
            lcd_putch(digits[4] + 0x30);
            lcd_putch(digits[5] + 0x30);
            while(RB0 == 1);
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
    
    PEIE = 1;
    GIE = 1;
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

void __interrupt() interrupt_handler() {
    if(CCP1IF == 1){
        CCPR1 = TMR1 + 100;
        timer++;
        CCP1IF = 0;
    }
}
