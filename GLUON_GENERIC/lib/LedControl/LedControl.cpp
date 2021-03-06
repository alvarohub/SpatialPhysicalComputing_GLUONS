/*
  LedControl.h - A library for controling Leds with a MAX7219/MAX7221
  Copyright (c) 2007 Eberhard Fahle
  Modified 2016 by Alvaro Cassinelli (reduce memory footprint - char not used, only numbers)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "LedControl.h"

//the opcodes for the MAX7221 and MAX7219
#define OP_NOOP   0
#define OP_DIGIT0 1
#define OP_DIGIT1 2
#define OP_DIGIT2 3
#define OP_DIGIT3 4
#define OP_DIGIT4 5
#define OP_DIGIT5 6
#define OP_DIGIT6 7
#define OP_DIGIT7 8
#define OP_DECODEMODE  9
#define OP_INTENSITY   10
#define OP_SCANLIMIT   11
#define OP_SHUTDOWN    12
#define OP_DISPLAYTEST 15

LedControl::LedControl(uint8_t dataPin, uint8_t clkPin, uint8_t csPin, uint8_t numDevices) {
    SPI_MOSI=dataPin;
    SPI_CLK=clkPin;
    SPI_CS=csPin;
    if(numDevices<=0 || numDevices>8 )
	numDevices=8;
    maxDevices=numDevices;
    pinMode(SPI_MOSI,OUTPUT);
    pinMode(SPI_CLK,OUTPUT);
    pinMode(SPI_CS,OUTPUT);
    digitalWrite(SPI_CS,HIGH);
    SPI_MOSI=dataPin;
    for(uint8_t i=0;i<64;i++)
	status[i]=0x00;
    for(uint8_t i=0;i<maxDevices;i++) {
	spiTransfer(i,OP_DISPLAYTEST,0);
	//scanlimit is set to max on startup
	setScanLimit(i,7);
	//decode is done in source
	spiTransfer(i,OP_DECODEMODE,0);
	clearDisplay(i);
	//we go uint8_to shutdown-mode on startup
	shutdown(i,true);
    }
}

uint8_t LedControl::getDeviceCount() {
    return maxDevices;
}

void LedControl::shutdown(uint8_t addr, bool b) {
    if(addr<0 || addr>=maxDevices)
	return;
    if(b)
	spiTransfer(addr, OP_SHUTDOWN,0);
    else
	spiTransfer(addr, OP_SHUTDOWN,1);
}

void LedControl::setScanLimit(uint8_t addr, uint8_t limit) {
    if(addr<0 || addr>=maxDevices)
	return;
    if(limit>=0 || limit<8)
    	spiTransfer(addr, OP_SCANLIMIT,limit);
}

void LedControl::setIntensity(uint8_t addr, uint8_t uint8_tensity) {
    if(addr<0 || addr>=maxDevices)
	return;
    if(uint8_tensity>=0 || uint8_tensity<16)
	spiTransfer(addr, OP_INTENSITY,uint8_tensity);

}

void LedControl::clearDisplay(uint8_t addr) {
    uint8_t offset;

    if(addr<0 || addr>=maxDevices)
	return;
    offset=addr*8;
    for(uint8_t i=0;i<8;i++) {
	status[offset+i]=0;
	spiTransfer(addr, i+1,status[offset+i]);
    }
}

void LedControl::setLed(uint8_t addr, uint8_t row, uint8_t column, boolean state) {
    uint8_t offset;
    byte val=0x00;

    if(addr<0 || addr>=maxDevices)
	return;
    if(row<0 || row>7 || column<0 || column>7)
	return;
    offset=addr*8;
    val=B10000000 >> column;
    if(state)
	status[offset+row]=status[offset+row]|val;
    else {
	val=~val;
	status[offset+row]=status[offset+row]&val;
    }
    spiTransfer(addr, row+1,status[offset+row]);
}

void LedControl::setRow(uint8_t addr, uint8_t row, byte value) {
    uint8_t offset;
    if(addr<0 || addr>=maxDevices)
	return;
    if(row<0 || row>7)
	return;
    offset=addr*8;
    status[offset+row]=value;
    spiTransfer(addr, row+1,status[offset+row]);
}

void LedControl::setColumn(uint8_t addr, uint8_t col, byte value) {
    byte val;

    if(addr<0 || addr>=maxDevices)
	return;
    if(col<0 || col>7)
	return;
    for(uint8_t row=0;row<8;row++) {
	val=value >> (7-row);
	val=val & 0x01;
	setLed(addr,row,col,val);
    }
}

void LedControl::setDigit(uint8_t addr, uint8_t digit, uint8_t value, boolean dp) {
    uint8_t offset;
    byte v;

    if(addr<0 || addr>=maxDevices)
	return;
    if(digit<0 || digit>7 || value>15)
	return;
    offset=addr*8;
    v=decimalTable[value];
    if(dp)
	v|=B10000000;
    status[offset+digit]=v;
    spiTransfer(addr, digit+1,v);

}

/*
void LedControl::setChar(uint8_t addr, uint8_t digit, char value, boolean dp) {
    uint8_t offset;
    byte index,v;

    if(addr<0 || addr>=maxDevices)
	return;
    if(digit<0 || digit>7)
 	return;
    offset=addr*8;
    index=(byte)value;
    if(index >127) {
	//nothing define we use the space char
	value=32;
    }
    v=charTable[index];
    if(dp)
	v|=B10000000;
    status[offset+digit]=v;
    spiTransfer(addr, digit+1,v);
}
*/

void LedControl::spiTransfer(uint8_t addr, volatile byte opcode, volatile byte data) {
    //Create an array with the data to shift out
    uint8_t offset=addr*2;
    uint8_t maxbytes=maxDevices*2;

    for(uint8_t i=0;i<maxbytes;i++)
	spidata[i]=(byte)0;
    //put our device data uint8_to the array
    spidata[offset+1]=opcode;
    spidata[offset]=data;
    //enable the line
    digitalWrite(SPI_CS,LOW);
    //Now shift out the data
    for(uint8_t i=maxbytes;i>0;i--)
 	shiftOut(SPI_MOSI,SPI_CLK,MSBFIRST,spidata[i-1]);
    //latch the data onto the display
    digitalWrite(SPI_CS,HIGH);
}
