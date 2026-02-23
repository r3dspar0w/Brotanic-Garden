/*
 * File:   lcd utilities.cpp
 *
 */
#undef __ARM_FP

#include "mbed.h"
#include "lcd.h"	// Include file is located in the project directory

#define DISPLAY_LCD_MASK 0x00000F00 //PORT A1: PA_15 : PA_8, 4-bit mode, using PA_11 : PA_8
#define DISPLAY_LCD_RESET 0x00000000

// HD44780 timing in microseconds. The previous code used milliseconds, which
// blocked the UI loop and made keypad input feel unresponsive during LCD updates.
static constexpr int LCD_SETTLE_US = 40;
static constexpr int LCD_ENABLE_PULSE_US = 1;
static constexpr int LCD_NIBBLE_DELAY_US = 40;
static constexpr int LCD_CLEAR_DELAY_MS = 2;

PortOut lcdPort(PortA, DISPLAY_LCD_MASK);
DigitalOut LCD_RS(PA_14);   //  Register Select on LC
DigitalOut LCD_EN(PA_12);   //  Enable on LCD controller
DigitalOut LCD_WR(PA_13);   //  Write on LCD controller

void lcd_strobe(void);

//--- Function for writing a command byte to the LCD in 4 bit mode -------------

void lcd_write_cmd(unsigned char cmd)
{
    unsigned char temp2;
    int tempLCDPort = 0;

    LCD_RS = 0;
    wait_us(LCD_SETTLE_US);

    temp2 = (cmd >> 4) & 0x0F;
    tempLCDPort = ((int)temp2 << 8) & 0x00000F00;
    lcdPort = tempLCDPort;
    wait_us(LCD_NIBBLE_DELAY_US);
    lcd_strobe();

    temp2 = cmd & 0x0F;
    tempLCDPort = ((int)temp2 << 8) & 0x00000F00;
    lcdPort = tempLCDPort;
    wait_us(LCD_NIBBLE_DELAY_US);
    lcd_strobe();

    // Clear/home commands need a longer execution time on the LCD controller.
    if (cmd == 0x01 || cmd == 0x02) {
        thread_sleep_for(LCD_CLEAR_DELAY_MS);
    }
}

//---- Function to write a character data to the LCD ---------------------------

void lcd_write_data(char data)
{
    char temp1;
    int tempLCDPort = 0;

    LCD_RS = 1;
    wait_us(LCD_SETTLE_US);

    temp1 = (data >> 4) & 0x0F;
    tempLCDPort = ((int)temp1 << 8) & 0x00000F00;
    lcdPort = tempLCDPort;
    wait_us(LCD_NIBBLE_DELAY_US);
    lcd_strobe();

    temp1 = data & 0x0F;
    tempLCDPort = ((int)temp1 << 8) & 0x00000F00;
    lcdPort = tempLCDPort;
    wait_us(LCD_NIBBLE_DELAY_US);
    lcd_strobe();
}


//-- Function to generate the strobe signal for command and character----------

void lcd_strobe(void)
{
    LCD_EN = 1;
    wait_us(LCD_ENABLE_PULSE_US);
    LCD_EN = 0;
    wait_us(LCD_ENABLE_PULSE_US);
}


//---- Function to initialise LCD module ----------------------------------------
void lcd_init(void)
{
    lcdPort = DISPLAY_LCD_RESET;                // lcd port (portA, PA8 - PA15) is connected to LCD data pin
    LCD_EN = 0;
    LCD_RS = 0;                                 // Select LCD for command mode
    LCD_WR = 0;                                 // Select LCD for write mode

    // Wait for LCD power-up initialization.
    thread_sleep_for(50);

    /* The data sheets warn that the LCD module may fail to initialise properly when
       power is first applied. This is particularly likely if the Vdd
       supply does not rise to its correct operating voltage quickly enough.

       It is recommended that after power is applied, a command sequence of
       3 bytes of 30h be sent to the module. This will ensure that the module is in
       8-bit mode and is properly initialised. Following this, the LCD module can be
       switched to 4-bit mode.
    */

    lcd_write_cmd(0x33);
    lcd_write_cmd(0x32);

    lcd_write_cmd(0x28);        // 001010xx - Function Set instruction
                                // DL=0 :4-bit interface,N=1 :2 lines,F=0 :5x7 dots

    lcd_write_cmd(0x0E);        // 00001110 - Display On/Off Control instruction
                                // D=1 :Display on,C=1 :Cursor on,B=0 :Cursor Blink on

    lcd_write_cmd(0x06);        // 00000110 - Entry Mode Set instruction
                                // I/D=1 :Increment Cursor position
                                // S=0 : No display shift

    lcd_write_cmd(0x01);        // 00000001 Clear Display instruction
    thread_sleep_for(LCD_CLEAR_DELAY_MS);
}

void lcd_Clear(void)
{
    lcd_write_cmd(0x01);        // 00000001 Clear Display instruction
    thread_sleep_for(LCD_CLEAR_DELAY_MS);
}
