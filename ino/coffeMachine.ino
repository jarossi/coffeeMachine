#include <Serial7SPI.h>
#include <avr/eeprom.h>
#include <PID_v1.h>
#include <Timer.h>

#define tempSensorPin A0
#define displaySlaveSelect 10

#define steamButton 2
#define steamLed 5
#define hotWaterButton 3
#define hotWaterLed 4

#define pump 9
#define heater 8
#define powerLed 7
#define powerButton 6
#define steamWandSwitch A1
#define brewSwitch A2


// flags for power on and hotwater mode on the wand, default is to steam
boolean powerOn=false;
boolean hotWaterOn=false;


Serial7SPI displaySerial7(displaySlaveSelect);

// A bunch of general purpose timers
Timer gTimers;
int displayTempTimer, serialDebugTimer, autoPowerOffTimer;

#define MAX_DELTA  10000
#define MIN_DELTA  0.01


// configuration struct
struct config_t {
  double brewTemp;
  double steamTemp;
  
  double pidKpDefault;
  double pidKiDefault;
  double pidKdDefault;
  
  double pidKpStartup;
  double pidKiStartup;
  double pidKdStartup;
  
  
  double pidKpBrew;
  double pidKiBrew;
  double pidKdBrew;

  double pidKpSteam;
  double pidKiSteam;
  double pidKdSteam;

  
  long heaterUpdateInterval;
  
  unsigned int pumpSteamPulseOff;
  unsigned int pumpSteamPulseOn;
  
  unsigned long autoPowerOffMillis;
  unsigned int displayTempRefreshMillis;
  unsigned int serialDebugMillis;
 
  float delta;
 
} config;

// pid global variables
double pidSetPoint=0;
double pidInput=0;
double pidOutput=0;

PID hPID(&pidInput, &pidOutput, &pidSetPoint, 0, 0, 0, DIRECT);

// Vars for the pump state machine
/*
0 = ready

1-9 initing

10-19 brew status 

20-29 steam mode status

30-39 hot water mode status
*/

int pumpStatus=0;
Timer pumpTimers;
int pumpTimerToStop=0;
int pumpNextStatus=0;

int pumpNextStatusTimeout=0;
int pumpTimerTimeout=0;

void pumpUpdate(){
  
  switch (pumpStatus){
    case 0:
      // we stop all the events
      for (int i=0; i<MAX_NUMBER_OF_EVENTS; i++){
        pumpTimers.stop(i);
      }
      digitalWrite(pump, LOW);
      break;
    // we start purging the pump for 3200 millis
    case 1:
      pumpNextStatus=2;
      pumpTimerToStop=pumpTimers.after(3200, changePumpStatus);
      digitalWrite(pump, HIGH);
      break;
    // we finish purging the pump
    case 2:
      digitalWrite(pump, LOW);
      pumpStatus=0;
      pumpNextStatus=0;
      break;
    // we start a brew, purging
    case 10:
      pumpNextStatus=11;
      pumpTimerToStop=pumpTimers.after(2200, changePumpStatus);
      digitalWrite(pump, HIGH);
      break;
    // we do setup doing pulses for a timeout;
    case 11:
      // we do pulses for 2000 milis
      pumpTimers.after(2600, changePumpStatusTimeout);
      pumpNextStatus=12;
      pumpNextStatusTimeout=16;
      digitalWrite(pump, LOW);
      pumpStatus=12;
      break;
    case 12:
      //pumpNextStatus=13;
      pumpTimerToStop=pumpTimers.after(700, changePumpStatus);
      digitalWrite(pump, LOW);
      pumpStatus=13;
      break;
    case 13:
      pumpNextStatus=14;
      digitalWrite(pump, LOW);
      break;
    case 14:
      pumpTimerToStop=pumpTimers.after(45, changePumpStatus);
      digitalWrite(pump, HIGH);
      pumpStatus=15;
      break;
    case 15:
      pumpNextStatus=12;
      digitalWrite(pump, HIGH);
      break;
    case 16:
      digitalWrite(pump, HIGH);
      break;
      
    // We do the steam pulses
    case 20:
      //pumpNextStatus=13;
      pumpTimerToStop=pumpTimers.after(config.pumpSteamPulseOff, changePumpStatus);
      digitalWrite(pump, LOW);
      pumpStatus=21;
      break;
    case 21:
      pumpNextStatus=22;
      digitalWrite(pump, LOW);
      break;
    case 22:
      pumpTimerToStop=pumpTimers.after(config.pumpSteamPulseOn, changePumpStatus);
      digitalWrite(pump, HIGH);
      pumpStatus=23;
      break;
    case 23:
      pumpNextStatus=20;
      digitalWrite(pump, HIGH);
      break;
    
      
  }
  pumpTimers.update();
  
}

void changePumpStatus() {
  pumpTimers.stop(pumpTimerToStop);
  pumpStatus=pumpNextStatus;
}


void changePumpStatusTimeout() {
 for (int i=0; i<MAX_NUMBER_OF_EVENTS; i++){
   pumpTimers.stop(i);
 }
  
//  pumpTimers.stop(pumpTimerTimeout);
  pumpStatus=pumpNextStatusTimeout;
  
}


void heaterUpdate(){
  static long heaterStartTime;
  
  if (heaterStartTime > millis()){
    heaterStartTime = millis();
  }
  
  if (millis() > (heaterStartTime + 2 * config.heaterUpdateInterval) ){
    heaterStartTime = millis(); 
  }
  
  if(millis() - heaterStartTime>config.heaterUpdateInterval)
  { //time to shift the Relay Window
    heaterStartTime += config.heaterUpdateInterval;
  }
  if(pidOutput < millis() - heaterStartTime) digitalWrite(heater,LOW);
  else digitalWrite(heater,HIGH);
 
}


void setup(){
  //set pin modes
  pinMode(steamButton, INPUT);
  pinMode(hotWaterButton, INPUT);
  pinMode(powerButton, INPUT);
  pinMode(steamWandSwitch, INPUT);
  pinMode(brewSwitch, INPUT);
  pinMode(hotWaterLed, OUTPUT);
  pinMode(steamLed, OUTPUT);
  pinMode(powerLed, OUTPUT);
  pinMode(heater, OUTPUT);
  pinMode(pump, OUTPUT);
  
  // set default values for output
  digitalWrite(hotWaterLed, LOW);
  digitalWrite(steamLed, LOW);
  digitalWrite(powerLed, LOW);
  digitalWrite(heater, LOW);
  digitalWrite(pump, LOW);
  
  
  //set reference as external one for analogRead
  analogReference(EXTERNAL);
 
  //init the display
  displaySerial7.begin();
  delay(100);
  //displaySerial7.brightness(254);
  displaySerial7.print("INIT");
  
 
  // we init the serial
  Serial.begin(115200);
  /*
  //initial seed:
  */
  
  // we read the configuration
  eeprom_read_block((void*)&config, (void*)0, sizeof(config));
  
  // We preset the temp to be by default brew
  pidSetPoint = config.brewTemp;
  
  hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);


  //turn the PID off
  hPID.SetMode(MANUAL);

  // We insert serial debug output on the events timers  
  serialDebugTimer = gTimers.every(config.serialDebugMillis, serialDebug); 
    
  delay(1000);
  displaySerial7.reset();
  
  //tell the PID to range between 0 and the full window size
  hPID.SetOutputLimits(0, config.heaterUpdateInterval);


}



void loop(){
  if (digitalRead(powerButton) == LOW){
    // shutdown everthing
    if (powerOn == true){
      powerOffSystem();
      delay(500);
    } else {  
      // start the default mode
      powerOn=true;
      hotWaterOn=false;

      digitalWrite(powerLed, HIGH);
      digitalWrite(steamLed, HIGH);
      // reset the display and insert the display refresh and auto power off events
      displaySerial7.reset();
      displayTempTimer=gTimers.every(config.displayTempRefreshMillis, displayTemp);
      autoPowerOffTimer=gTimers.after(config.autoPowerOffMillis, powerOffSystem);
      hPID.SetMode(AUTOMATIC);
                 
      // purge the heater
      if (pidSetPoint - readTemp () > 30 ) {
        pumpStatus=1;
      }
      delay(500);
    }
     
  }  
 
  // steam button handling 
  if (powerOn && digitalRead(steamButton) == LOW){
    // we reset the autoPowerOffTimer
    gTimers.stop(autoPowerOffTimer);
    autoPowerOffTimer = gTimers.after(config.autoPowerOffMillis, powerOffSystem);
    hotWaterOn=false;
    digitalWrite(hotWaterLed, LOW);
    digitalWrite(steamLed, HIGH);
    /*
    // we enter into the config menu we shutdown heater and pump just in case
    if (digitalRead(hotWaterButton) == LOW){
      // we shutdown the steam and hot water leds for stetics, also heater and pump just in case
      digitalWrite(hotWaterLed, LOW);
      digitalWrite(steamLed, LOW);
      digitalWrite(heater, LOW);
      digitalWrite(pump, LOW);
      displaySerial7.print("sett");
      delay(1000);
      while(true){
        // we exit the menu and save settings
        if (digitalRead(steamButton) == LOW && digitalRead(hotWaterButton) == LOW){
          displaySerial7.print("saUe");
          delay(1000);
          // we revert the status of the LEDs
          if (hotWaterOn){
            digitalWrite(hotWaterLed, HIGH);
          } else {
            digitalWrite(steamLed, HIGH);
          }
          // we exit the menu mode
          break;
        }
      }      
    } 
    */
  }
  
  // hotWaterButton handling
  if (powerOn && digitalRead(hotWaterButton) == LOW){
    // we reset the autoPowerOffTimer
    gTimers.stop(autoPowerOffTimer);
    autoPowerOffTimer = gTimers.after(config.autoPowerOffMillis, powerOffSystem);
    hotWaterOn=true;
    digitalWrite(hotWaterLed, HIGH);
    digitalWrite(steamLed, LOW);
  }
  
  
  // brew switch handling
  if (powerOn && digitalRead(brewSwitch) == LOW){
    // we reset the autoPowerOffTimer
    gTimers.stop(autoPowerOffTimer);
    autoPowerOffTimer = gTimers.after(config.autoPowerOffMillis, powerOffSystem);
    delay(500);
    pumpStatus=10;
    pidSetPoint=config.brewTemp;
    hPID.SetTunings(config.pidKpBrew, config.pidKiBrew, config.pidKdBrew);
    while (digitalRead(brewSwitch) == LOW){
      // keep other events going
      readSerialCommands();
      pidInput = readTemp();
      gTimers.update();
      hPID.Compute();
      pumpUpdate();
      heaterUpdate();  
    }
    // we stop the pump
    pumpStatus=0;
    pidSetPoint=config.brewTemp;
    hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);
    
  }

  
  // steam wand switch handling, steam routine
  if (powerOn && digitalRead(steamWandSwitch) == LOW && !hotWaterOn){
    // we reset the autoPowerOffTimer
    gTimers.stop(autoPowerOffTimer);
    autoPowerOffTimer = gTimers.after(config.autoPowerOffMillis, powerOffSystem);
    delay(500);
    
    pidSetPoint=config.steamTemp;
    hPID.SetTunings(config.pidKpSteam, config.pidKiSteam, config.pidKdSteam);
    // we wait till we are at the correct temp
    while((pidSetPoint - 10 > pidInput) && (digitalRead(steamWandSwitch) == LOW ) ){
      // keep events going
      //pumpStatus=0;
      readSerialCommands();
      pidInput = readTemp();
      gTimers.update();
      hPID.Compute();
      heaterUpdate();
    }
    pumpStatus=20;
    while (digitalRead(steamWandSwitch) == LOW){
      // keep other events going
      readSerialCommands();
      pidInput = readTemp();
      gTimers.update();
      hPID.Compute();
      pumpUpdate();
      heaterUpdate();
    }
    // we stop the pump
    pidSetPoint=config.brewTemp;
    hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);
    pumpStatus=0;
    pumpUpdate();
    delay(2000);
    pidInput = readTemp();
    
   // we cool down the water block
    while (pidInput > pidSetPoint + 4){
      readSerialCommands();
      pidInput = readTemp();
      gTimers.update();
      hPID.Compute();
      pumpUpdate();
      digitalWrite(pump, HIGH);
    }
    digitalWrite(pump, LOW);
  }

  // steam wand switch handling, hotwater routine
  if (powerOn && digitalRead(steamWandSwitch) == LOW && hotWaterOn){
    // we reset the autoPowerOffTimer
    gTimers.stop(autoPowerOffTimer);
    autoPowerOffTimer = gTimers.after(config.autoPowerOffMillis, powerOffSystem);
    delay(500);    
    pidSetPoint=config.brewTemp;
    hPID.SetTunings(config.pidKpBrew, config.pidKiBrew, config.pidKdBrew);
    while (digitalRead(steamWandSwitch) == LOW){
      // keep other events going
      readSerialCommands();
      pidInput = readTemp();
      gTimers.update();
      hPID.Compute();
      digitalWrite(pump, HIGH);
      heaterUpdate();
    }
    // we stop the pump
    digitalWrite(pump, LOW);
    pidSetPoint=config.brewTemp;
    hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);
    pumpStatus=0;
    pumpUpdate();
    delay(2000);
  }


  readSerialCommands();
  pidInput = readTemp();
  gTimers.update();

  
  if (powerOn){
    if ( pidSetPoint - 7 > pidInput ){
      hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);
    }else{
      hPID.SetTunings(config.pidKpStartup, config.pidKiStartup, config.pidKdStartup);
    }

    hPID.Compute();
    // update PID and pump status
    pumpUpdate();
    heaterUpdate();
    
    
    
    true;
    //delay(50); 
  } else {
    //powerOffSystem();
    // display RTC
    true;
    
  }
  
   
}


float readTemp(){
  int reading = analogRead(tempSensorPin);
  float voltage = reading * 3.3;
  voltage /= 1024.0;
  float temperatureC= (voltage - 0.5 ) * 100;  
  if (temperatureC > 120){
    powerOffSystem();
  }  
  return temperatureC;
}

void displayTemp(){
  displaySerial7.write(readTemp());
}


void serialDebug(){
  Serial.print(millis());
  Serial.print(" S:");
  Serial.print(pidSetPoint);
  Serial.print(" I:");
  Serial.print(pidInput);
  Serial.print(" O:");
  Serial.print(pidOutput);
  Serial.print(" Kp:");
  Serial.print(hPID.GetKp());
  Serial.print(" Ki:");
  Serial.print(hPID.GetKi());
  Serial.print(" Kd");
  Serial.print(hPID.GetKd());
  Serial.print(" PS:");
  Serial.print(pumpStatus);
  Serial.print(" bT:");
  Serial.print(config.brewTemp);
  Serial.print(" sT:");
  Serial.println(config.steamTemp);
  
  Serial.print("defaultKp:");
  Serial.print(config.pidKpDefault);
  Serial.print(" defaultKi:");
  Serial.print(config.pidKiDefault);
  Serial.print(" defaultKd:");
  Serial.print(config.pidKdDefault);
  
  Serial.print(" startupKp:");
  Serial.print(config.pidKpStartup);
  Serial.print(" startupKi:");
  Serial.print(config.pidKiStartup);
  Serial.print(" startupKd:");
  Serial.println(config.pidKdStartup);
  
  Serial.print("brewKp:");
  Serial.print(config.pidKpBrew);
  Serial.print(" brewKi:");
  Serial.print(config.pidKiBrew);
  Serial.print(" brewKd:");
  Serial.print(config.pidKdBrew);
  
  Serial.print(" steamKp:");
  Serial.print(config.pidKpSteam);
  Serial.print(" steamKi:");
  Serial.print(config.pidKiSteam);
  Serial.print(" steamKd:");
  Serial.println(config.pidKdSteam);
  
  Serial.print("pump Steam Pulse Off: ");
  Serial.print(config.pumpSteamPulseOff);
  Serial.print(" pump Steam Pulse On: ");
  Serial.println(config.pumpSteamPulseOn);
  
  
}

void powerOffSystem(){
  powerOn=false;
  hotWaterOn=false;
  //turn the PID off
  hPID.SetMode(MANUAL);
  pidOutput=0;
  digitalWrite(steamLed, LOW);
  digitalWrite(hotWaterLed, LOW);
  digitalWrite(powerLed, LOW);
  digitalWrite(heater, LOW);
  digitalWrite(pump, LOW);
  gTimers.stop(displayTempTimer);
  gTimers.stop(autoPowerOffTimer);
  displaySerial7.reset();
  
}


void readSerialCommands(){ 
  int incomingByte = 0;
  
  
  while(Serial.available()){
    incomingByte = Serial.read();
    if (incomingByte == '?') {
      Serial.println("Send these characters for control:");
      Serial.println("? : print help");
      Serial.println("+/- : adjust delta by a factor of ten");
      Serial.println("I : reset/initialize default values");
      Serial.println("O : Save settings");   
      Serial.println("Qq/Ww/Ee : adjust default pid Kp/Ki/Kd pid by delta");
      Serial.println("Aa/Ss/Dd : adjust startup pid Kp/Ki/Kd pid by delta");
      Serial.println("Zz/Xx/Cc : adjust brew pid Kp/Ki/Kd pid by delta");
      Serial.println("Rr/Tt/Yy : adjust steam pid Kp/Ki/Kd pid by delta");
      Serial.println("B/b : up/down adjust brew temp by delta");
      Serial.println("N/n : up/down adjust steam temp by delta");
      Serial.println("K/k : up/down pump steam pulse off" );
      Serial.println("L/l : up/down pump steam pulse on");
      Serial.println("u : toggle periodic status update");
      Serial.println("<space> : print status now");
    }
    if (incomingByte == '+') {
      config.delta *= 10.0;
      if (config.delta > MAX_DELTA)
        config.delta = MAX_DELTA;
      Serial.print("delta:");
      Serial.println(config.delta);
    } 
    if (incomingByte == '-') {
      config.delta /= 10.0;
      if (config.delta < MIN_DELTA)
        config.delta = MIN_DELTA;
      Serial.print("delta:");
      Serial.println(config.delta);

    }
    if (incomingByte == 'I') {
      config.brewTemp = 89.0;
      config.steamTemp = 104.0;
      config.pidKpDefault = 66.0;
      config.pidKiDefault = 0.10;
      config.pidKdDefault = 0;
      config.pidKpStartup = 22.0;
      config.pidKiStartup = 0.02;
      config.pidKdStartup = 0;
      config.pidKpBrew = 1320.0;
      config.pidKiBrew = 0.10;
      config.pidKdBrew = 0;
      config.pidKpSteam = 1320.0;
      config.pidKiSteam = 0.10;
      config.pidKdSteam = 0;

      config.autoPowerOffMillis=1200000;
      config.displayTempRefreshMillis=500;
      config.serialDebugMillis=1000;
      config.heaterUpdateInterval=1000;
      config.pumpSteamPulseOff=700;
      config.pumpSteamPulseOn=45;
      config.delta = 1.0;
      pidSetPoint = config.brewTemp;
      hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);
    } 
    
    if (incomingByte == 'O') {
      Serial.println("configs saved");
      eeprom_write_block((const void*)&config, (void*)0, sizeof(config));
    }
    
    // adjust pid default values
    if (incomingByte == 'Q') {
      config.pidKpDefault = config.pidKpDefault + config.delta;
      hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);      
    } 
    
    if (incomingByte == 'q') {
      config.pidKpDefault = config.pidKpDefault - config.delta;
      hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault); 
      
    } 
    if (incomingByte == 'W') {
      config.pidKiDefault = config.pidKiDefault + config.delta;
      hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault); 
    } 
    if (incomingByte == 'w') {
      config.pidKiDefault = config.pidKiDefault - config.delta;
      hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);
    } 
    if (incomingByte == 'E') {
      config.pidKdDefault = config.pidKdDefault + config.delta;
      hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);
    } 
    if (incomingByte == 'e' ){
      config.pidKdDefault = config.pidKdDefault - config.delta;
      hPID.SetTunings(config.pidKpDefault, config.pidKiDefault, config.pidKdDefault);
    } 
    
    // adjust pid startup values
    if (incomingByte == 'A') {
      config.pidKpStartup = config.pidKpStartup + config.delta;
    } 
    if (incomingByte == 'a') {
      config.pidKpStartup = config.pidKpStartup - config.delta; 
    } 
    if (incomingByte == 'S') {
      config.pidKiStartup = config.pidKiStartup + config.delta; 
    } 
    if (incomingByte == 's') {
      config.pidKiStartup = config.pidKiStartup - config.delta;
    } 
    if (incomingByte == 'D') {
      config.pidKdStartup = config.pidKdStartup + config.delta;
    } 
    if (incomingByte == 'd' ){
      config.pidKdStartup = config.pidKdStartup - config.delta;
    } 
   
    // adjust pid brew values
    if (incomingByte == 'Z') {
      config.pidKdBrew = config.pidKdBrew + config.delta;
    } 
    if (incomingByte == 'z') {
      config.pidKdBrew = config.pidKdBrew - config.delta; 
    } 
    if (incomingByte == 'X') {
      config.pidKiBrew = config.pidKiBrew + config.delta; 
    } 
    if (incomingByte == 'x') {
      config.pidKiBrew = config.pidKiBrew - config.delta;
    } 
    if (incomingByte == 'C') {
      config.pidKdBrew = config.pidKdBrew + config.delta;
    } 
    if (incomingByte == 'c' ){
      config.pidKdBrew = config.pidKdBrew - config.delta;
    }

    // adjust pid steam values
    if (incomingByte == 'R') {
      config.pidKdSteam = config.pidKdSteam + config.delta;
    } 
    if (incomingByte == 'r') {
      config.pidKdSteam = config.pidKdSteam - config.delta; 
    } 
    if (incomingByte == 'T') {
      config.pidKiSteam = config.pidKiSteam + config.delta; 
    } 
    if (incomingByte == 't') {
      config.pidKiSteam = config.pidKiSteam - config.delta;
    } 
    if (incomingByte == 'Y') {
      config.pidKdSteam = config.pidKdSteam + config.delta;
    } 
    if (incomingByte == 'y' ){
      config.pidKdSteam = config.pidKdSteam - config.delta;
    }    
    
    
    if (incomingByte == 'B') {
      config.brewTemp = config.brewTemp + config.delta;
      pidSetPoint = config.brewTemp;     
    } 
    if (incomingByte == 'b') {
      config.brewTemp = config.brewTemp - config.delta;
      pidSetPoint = config.brewTemp;
    }
    if (incomingByte == 'N') {
      config.steamTemp = config.steamTemp + config.delta;      
    } 
    if (incomingByte == 'n') {
      config.steamTemp = config.steamTemp - config.delta;
    }
    if (incomingByte == 'K') {
      config.pumpSteamPulseOff = config.pumpSteamPulseOff + config.delta;
    } 
    if (incomingByte == 'k') {
      config.pumpSteamPulseOff = config.pumpSteamPulseOff - config.delta;
    }
    if (incomingByte == 'L') {
      config.pumpSteamPulseOn = config.pumpSteamPulseOn + config.delta;
    } 
    if (incomingByte == 'l') {
      config.pumpSteamPulseOn = config.pumpSteamPulseOn - config.delta;
    }
    
  }  

}
