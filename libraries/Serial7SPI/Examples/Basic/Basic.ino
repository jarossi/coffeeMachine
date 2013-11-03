/*===========================================================================
  Serial 7 Segment LED Example of Serial7 Library
  Creator: Austin Saint Aubin (AustinSaintAubin@gmail.com)
  Data: 2012 / 05 / 05

  Based off of Serial7 library by Josh Villbrandt (http://javconcepts.com/), February 13, 2012.
  ============================================================================= */
// [ Included Library's ]
#include <Serial7.h>
#include <SoftwareSerial.h>

// [ Global Pin Constants / Varables ]
const int Serial7PowerPin  = 13;  // Digital pin that will be used to suppy power to the LED Segment, so that it can be easly toggled on and off
const int SoftwareSerialRX = 2;   // Software serial for getting messageses
const int SoftwareSerialTX = 3;   // Software serial for sending Serial 7 Segment LED, you can also use the anilog pins for TX (A0, A1, A2, A3, A4, A5)

// [ Global Constants / Varables ]
const byte animationSequence8Track[]   = {0b0000001, 0b0000010, 0b1000000, 0b0010000, 0b0001000, 0b0000100, 0b1000000, 0b0100000, 0b0000000};  // Circuit 8 Track
const byte animationSequence8TrackSize = sizeof(animationSequence8Track) / sizeof(byte);

// [ Global Classes ]
SoftwareSerial mySerial(SoftwareSerialRX, SoftwareSerialTX);  // RX, TX

// Setup Serial7 Serial Connection on the Ardunio
Serial7 serial7(&mySerial);  // Sets up serial port for Comunication, Use (&Serial) for hardware serial.


void setup()
{
  Serial.begin(57600);
  
  // Serial Conected message
  Serial.write(27);  // Clears Screen, prints "esc"
  Serial.println("Connected: ");
  
  // Send Power to Display
  pinMode(Serial7PowerPin, OUTPUT);
  digitalWrite(Serial7PowerPin, HIGH);
  
  // Set Serial Display Baud Rate
  serial7.begin(9600);
  
  // Text Output
  serial7.print("UOlT");
  delay(2000);
  
  // Display Amimation, Errases Text
  serial7.animation(0x7B, animationSequence8Track, animationSequence8TrackSize, 50);
  serial7.animation(0x7C, animationSequence8Track, animationSequence8TrackSize, 50);
  serial7.animation(0x7D, animationSequence8Track, animationSequence8TrackSize, 50);
  serial7.animation(0x7E, animationSequence8Track, animationSequence8TrackSize, 50);
}

void loop()
{   
  // Read analog Voltage
  float volts = analogRead(A0) / 1024.0 * 5.0;  // Calculate Volts
  
  // Output Results
  Serial.println(volts, 3);
  serial7.write(volts);
  
  // Wait for a bit
  delay(100);
}
