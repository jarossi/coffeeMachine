/*===========================================================================
  Serial 7 Segment LED Example of Serial7 Library
  Creator: Austin Saint Aubin (AustinSaintAubin@gmail.com)
  Data: 2012 / 05 / 05

  Based off of Serial7 library by Josh Villbrandt (http://javconcepts.com/), February 13, 2012.
  ============================================================================= */
// [ Included Library's ]
#include <Serial7.h>

#if ARDUINO >= 100
 #include <SoftwareSerial.h>
#else
  // Older Arduino IDE requires NewSoftSerial, download from:
  // http://arduiniana.org/libraries/newsoftserial/
 #include <NewSoftSerial.h>
 // DO NOT install NewSoftSerial if using Arduino 1.0 or later!
#endif


// [ Global Pin Constants / Varables ]
const int Serial7PowerPin  = 13;  // Digital pin that will be used to suppy power to the LED Segment, so that it can be easly toggled on and off
const int SoftwareSerialRX = 2;   // Software serial for getting messageses
const int SoftwareSerialTX = 3;   // Software serial for sending Serial 7 Segment LED, you can also use the anilog pins for TX (A0, A1, A2, A3, A4, A5)

// [ Global Constants / Varables ]
const byte animationSequence8Track[]   = {0b0000001, 0b0000010, 0b1000000, 0b0010000, 0b0001000, 0b0000100, 0b1000000, 0b0100000, 0b0000000};  // Circuit 8 Track
const byte animationSequence8TrackSize = sizeof(animationSequence8Track) / sizeof(byte);
const byte animationSequenceGPSLock[]   = {0b0001001, 0b0101101, 0b0111111, 0b1111111, 0b0000000};  // GPS Lock Animation
const byte animationSequenceGPSLockSize = sizeof(animationSequenceGPSLock) / sizeof(byte);

// [ Global Classes ]
#if ARDUINO >= 100
  SoftwareSerial mySerial(SoftwareSerialRX, SoftwareSerialTX);  // RX, TX
#else
  NewSoftSerial mySerial(SoftwareSerialRX, SoftwareSerialTX);  // RX, TX
#endif

// Setup Serial7 Serial Connection on the Ardunio
Serial7 serial7(&mySerial);  // Sets up serial port for Comunication, Use (&Serial) for hardware serial.


void setup()
{
  // Serial Start
  Serial.begin(57600);
  
  // Serial Conected message
  Serial.write(27);  // Clears Screen, prints "esc"
  Serial.println("Connected: ");
  
  // Send Power to Display
  pinMode(Serial7PowerPin, OUTPUT);
  digitalWrite(Serial7PowerPin, HIGH);
  delay(250); // Wait for display to power up
  
  serial7.begin(9600);
  //serial7.brightness(0); // (0 - 254) The lower the number the brighter the display.
}

void loop()
{
  // Text Output
  serial7.print("ani");
  delay(250);
  
  // Animations
  for (int i = 0; i < 8; i++)
  {
    // Animation 1
    byte animation = 0b111110;
    for(byte i = 0; i < 6; i++)
    {
      serial7.send(0x7E);
      serial7.send(animation);
      
      animation = ((animation << 1) & 0b111111) + 1;
      
      delay(100);
    }
  }
  for (int i = 0; i < 20; i++)
    serial7.animation(0x7E, animationSequence8Track, animationSequence8TrackSize, 50);

  for (int i = 0; i < 8; i++)
    serial7.animation(0x7E, animationSequenceGPSLock, animationSequenceGPSLockSize, 250);
  
  // Multiple Animations
  for (int i = 0; i < 4; i++)
  {
    serial7.animation(0x7B, animationSequenceGPSLock, animationSequenceGPSLockSize, 250);
    serial7.animation(0x7C, animationSequence8Track, animationSequence8TrackSize, 50);
    serial7.animation(0x7D, animationSequence8Track, animationSequence8TrackSize, 50);
    serial7.animation(0x7E, animationSequenceGPSLock, animationSequenceGPSLockSize, 250);
  }
  
  // Demo Clock 12 Hour
  serial7.print("12HR");
  delay(2000);
  for(byte i = 0; i <= 24; i++)
  {
    serial7.clock12(i, i*2);
    delay(500);
  }
  
  // Demo Clock 24 Hour
  serial7.print("24HR");
  delay(2000);
  for(byte i = 0; i <= 24; i++)
  {
    serial7.clock24(i, i*2);
    delay(500);
  }
  
  // Demo Counter Float
  serial7.print("Flao");
  delay(2000);
  for(float i = -1050; i < 10500; i+=1.321)
  {
    serial7.write(i);
    delay(10);
  }

  // Demo Counter Int
  serial7.print("Int");
  delay(2000);
  for(int i = -1050; i < 10500; i++)
  {
    serial7.write(i);
    delay(10);
  }
  
  // Read analog Voltage
  serial7.print("Int");
  delay(2000);
  for (int i = 0; i < 1000; i++)
  {
    float volts = analogRead(A0) / 1024.0 * 5.0;  // Calculate Volts
    serial7.write(volts);
    delay(100);
  }
}
