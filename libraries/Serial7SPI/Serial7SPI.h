/*===========================================================================
  Serial 7 Segment LED Example of Serial7 Library
  Creator: Austin Saint Aubin (AustinSaintAubin@gmail.com)
  Data: 2012 / 05 / 05

  Based off of Serial7 library by Josh Villbrandt (http://javconcepts.com/), February 13, 2012.
  ============================================================================= */

#ifndef Serial7SPI_h
#define Serial7SPI_h

#include "Arduino.h"

class Serial7SPI
{
  public:
    Serial7SPI(int SS); // Constructor when using SoftwareSerial

    void begin();
    void send(byte command);
    void reset();
    void brightness(byte b);
    void print(const char *str);
    void write(int value);
    void write(float value);
    void clock24(byte hour, byte min); // 24hour
    void clock12(byte hour, byte min); // 12hour
    void animation(byte digit, const byte animationSequence[], const byte size, int speed);

  private:
    int SLAVESELECT;
};

#endif
