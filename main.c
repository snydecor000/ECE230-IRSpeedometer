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

void ADC_init(void);
unsigned int ADC_convert(unsigned char);

void main(void) {
    lcd_init();
    ADC_init();
    
    while(1){
        lcd_goto(0);
        lcd_puts("Value: ");
        unsigned int photoVal = ((unsigned long)ADC_convert(13)*500)/1023;
        DisplayVolt(photoVal);
        lcd_putch(0x30+RB5);
        for(unsigned int x = 0;x<10000;x++);
    }
}

void ADC_init(void) {
    TRISB5 = 1; //Make RB5/AN13 inputs
    ANS13 = 1;  //Configure as analog

    ADON = 1;   // Turn on ADC Module
    ADFM = 1;   // Right justify result in ADRES = ADRESH:ADRESL registers
    VCFG1 = 0;  // Use VSS and VDD to set conversion range to 0V-5V
    VCFG0 = 0;  
    ADCS1 = 0;  // ADC clock freq set to Fosc/8 = 1 MH (Fosc=8MHz)
    ADCS0 = 1;
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
