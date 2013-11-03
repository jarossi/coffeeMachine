/*===========================================================================
  Serial 7 Segment LED Example of Serial7 Library
  Creator: Austin Saint Aubin (AustinSaintAubin@gmail.com)
  Data: 2012 / 07 / 05

  Based off of Serial7 library by Josh Villbrandt (http://javconcepts.com/), February 13, 2012.
  Added support for ATtiny45 & ATtiny85
  ============================================================================= */

#include "Serial7SPI.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Serial7SPI::Serial7SPI(int SS)
{
	SLAVESELECT=SS;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Begins the Serial Connection ]  starts the serial connection
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::begin()
{
  pinMode(11, OUTPUT);
  pinMode(12, INPUT);
  pinMode(13, OUTPUT);
  pinMode(SLAVESELECT, OUTPUT);
  digitalWrite(SLAVESELECT, HIGH); //disable device
  // SPCR = 01010010
  //interrupt disabled,spi enabled,msb 1st,master,clk low when idle,
  //sample on leading edge of clk,system clock/64 
  SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR1);
  reset();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Send Command / Data ]  sends something to the display
// Example: serial7.send('e');    // Displays e
// Example: serial7.send(0x7F); send(0x01);  // Sets the baud rate to 9600
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::send(byte command) {
  digitalWrite(SLAVESELECT, LOW);
  SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR1);
  delay(1); 
  SPDR = command;                    // Start the transmission
  while (!(SPSR & (1<<SPIF)))       // Wait the end of the transmission
    {
    };
  digitalWrite(SLAVESELECT, HIGH);
  return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Reset Display ]  resets the display
// Example: serial7.reset();
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::reset()
{ 
  // Clear & Set Cursor 
  send(0x76);
  
  // Clear Decimal Points
  send(0x77);         // Send decimal point control character
  send(0b00000000); // Turn off all decimal marks
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Sets the Brightness ]  sets the brightness, 0 = brightest, 0-254, will be kept even after power off
// Example: serial7.brightness(0);    // BRIGHT
// Example: serial7.brightness(200);  // DIM
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::brightness(byte b)
{
  reset();
  
  send(0x7A);
  send(b);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Displays a String ]  displays any string of 4 characters and prints them, left to right
// Example: serial7.print("ani");   // Displays ani
// Example: serial7.print("HI U");  // Displays HI U
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::print(const char *str) // Prints most any 4 characters, see manual for full list
{
  // Reset Display
  reset();
  
  // Clear Decimal Points
  //send(0x77);         // Send decimal point control character
  //send(0b00000000); // Turn off all decimal marks
    
  // Print out Character Array
    for(int i = 0; i < 4; i++)
    {
	  // If not end of string print blank
        if (str[i] != NULL)
            send(str[i]);
        else
            send(0x78);
    }
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Display a Floating Point Number ]  send a positive or negative int
// Example: serial7.write(123);  // Displays _123
// Example: serial7.write(-55);  // Displays _-55
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::write(int value)
{
  // Variables
  boolean negative;
  boolean leading = true;  // Used to help set leading BLANKS and the NEG SIGN, gets set to false when a value is found.

  // Detect Overflow & Negative Overflow
  send(0x77);         // Send decimal point control character
  if (value > 9999 || value < -999)  // Cant effectively display a number grater than 9999 or less than -999
    send(0b00100000); // Set apostrophe to on to show overflow
  else
    send(0b00000000); // Turn off all decimal marks

  // Detect Negative Number and Convert Value to Positive
  if (value < 0)
  {
    negative = true;
    value *= -1;  // Flip the value to positive number
  }
  else
    negative = false;

  // Loop and output
  for (int multiplyer = 1000; multiplyer >= 1; multiplyer /= 10)
  {
      if (!leading || value / multiplyer % 10 || (multiplyer == 1 && value == 0))  // If not the first number, or current number have a value grater than zero, or number is last number and is zero
      {
          send(value / multiplyer % 10);
          leading = false;  // Set the leading flag to false
      }
      else if (negative && leading && value / (multiplyer / 10) % 10)  // check if negative, leading, and if the next value is not zero
          send('-');   // Send "-" Character
      else
          send(0x78);  // Send Blank Character
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Display a Floating Point Number ]  send a positive or negative float
// Example: serial7.write(2.34);  // Displays 2.340
// Example: serial7.write(-55.3);  // Displays -55.3
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::write(float value)
{
  // Variables
  float output;
  byte decimal;

  // Detect Negative Number and Convert Output to Positive
  if (value < 0)
    output = value * -10;
  else
    output = value;

  // Switch Decimal Point Depending on Value
  if (output < 10) {
    decimal = 0b00000001;  // Set first decimal place
    output = output*1000;
  }
  else if (output < 100) {
    decimal = 0b00000010;  // Set second decimal place
    output = output*100;
  }
  else if (output < 1000) {
    decimal = 0b00000100;  // Set third decimal place
    output = output*10;
  }
  else {
    decimal = 0b00001000;  // Set fourth decimal place
	output = output;
  }

  // Detect Overflow & Negative Overflow
  if (value > 9999)  // Cant effectively display a number grater than 9999
    decimal = decimal | 0b00100000; // Set apostrophe to on to show overflow
  else if (value < -999)   // Cant effectively display a number less than -999
    decimal = 0b00101000;  // Set apostrophe to on to show overflow, and set farthest decimal po

  // Send Decimal
  send(0x77);     // Send decimal point control character
  send(decimal);  // Send decimal as is, no overflow noted

  // Output for Positive or Negative Number
  if (value < 0)
  {
    // Negative Number
    send('-');               // Flows into Digit 1
    send((int)(output/1000) % 10);  // Flows into Digit 2
    send((int)(output/100) % 10);   // Flows into Digit 3
    send((int)(output/10) % 10);    // Flows into Digit 4
  }
  else
  {
    // Positive Number
    send((int)(output/1000) % 10);  // Flows into Digit 1
    send((int)(output/100) % 10);   // Flows into Digit 2
    send((int)(output/10) % 10);    // Flows into Digit 3
    send((int)(output) % 10);       // Flows into Digit 4
  }
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Display 24 Hour Clock ]  send hour as 1-24 hours, send min as 0-59 for minutes
// Example: serial7.clock24(11, 30);  // Displays 11:30
// Example: serial7.clock24(20, 45);  // Displays 20:45
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::clock24(byte hour, byte min)
{
    // Send Delimiter
    send(0x77);  // decimal point control character
    send(0b00010000);  // Send decimal as is, no overflow noted

    // Send the time
    send(hour/10 % 10);
    send(hour % 10);
    send(min/10 % 10);
    send(min % 10);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Display 12 Hour Clock ]  send hour as 1-24 hours, send min as 0-59 for minutes
// Example: serial7.clock12(11, 30);  // Displays 11:30* with AM indicator on
// Example: serial7.clock24(20, 45);  // Displays 08:45  with AM indicator off
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::clock12(byte hour, byte min)
{
  byte decimal = 0b00010000;

  // Detect Overflow
  if (hour <= 12)
    decimal = decimal | 0b00100000; // Set apostrophe to show AM
  else
    hour = hour - 12;

  // Send Delimiter
  send(0x77);  // decimal point control character
  send(decimal);  // Send decimal as is, no overflow noted

  // Send the time
  send(hour/10 % 10);
  send(hour % 10);
  send(min/10 % 10);
  send(min % 10);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// -- Function [ Show Animation ]  send byte digit for animation "0x7B or 0x7C or 0x7D or 0x7E", then animation sequence array, then size of array, and the speed in milliseconds
// Example: serial7.animation(0x7B, animationSequence8Track, animationSequence8TrackSize, 50);  // Animation for first digit
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Serial7SPI::animation(byte digit, const byte animationSequence[], const byte size, int speed)
{
  // First Part of Animation
  send(digit);
  send(animationSequence[0]);

  // Animation
  for (int i = 1; i < size; i++)
  {
    delay(speed);
    send(digit);
    send(animationSequence[i]);
  }
}
