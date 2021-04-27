/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#line 1 "/mnt/space/workspace/particle/DL2_Rev_6/src/DL2_Rev_6.ino"
/* 
 * Project DL_2_Rev_6
 * Description: Particle Boron based datalogger, based off of the venerable R_152 datalogger.
 * Author: Wes Byne
 * Date: Winter, 2020
 * License: Free to all, GPL, in the hope that someone may find it of some use.
 */

//#pragma GCC optimize ("O0") //dont declare unless debugging...

#include "Arduino.h"
#include "Particle.h"
#include "AdafruitDataLoggerRK.h"
#include "SdCardLogHandlerRK.h"
//#include "SdFat.h"

//#define DEBUG  IT APPEARS THAT TOO MANY BREAKPOINTS WILL PREVENT THE DEBUGGER FROM EXECUTING AFTER COMPILE AND UPLOAD 21FEB20_2123
//var types have to agree with what particle uses as primitives: i.e.-probably shouldn't use uints, 
//  etc, because spark_wiring.h will throw an error if its looking for an int and finds a uint8_t
//TODO: Save to eeprom (done Winter 2020)
//TODO: Dump the SD Card
//TODO: find location
//TODO: Add reset button on outside of box for settings or programming
//TODO: change TZS (time zone set) to just check the time and date and set isDST accordingly. Defaulted to DST except for Nov-Feb 13Aug20 
//TODO: Set Time to 1/1/1950 if the network can't set the time.  Default to that value.
//TODO: Verify the If Then on the different sensors: (essentially done 20May20)
//TODO: Add in a voltage read for the 12V supply - cheated and used another divider on 5800
//TODO: add a jumper to toggle 6V/12V
//TODO: Temp adjustment for maxbotix ultrasonic - first cut 23Mayturbidity probe (8)
//  9: paddlewheel velocity probe (9), ... ehhh what a mistake. collected iron filings..Summer 2020
//FIXED: sleep resets to full 5 minutes after tip wake, had to reduce by time slept so far - 13Aug20
//TODO: set to exactly 5 minute increments, waking on the 5's.
void setup();
void loop();
int Configuration_Settings(String command);
int Read_the_Analogs();
void Tip_Int_Handler();
void Figure_the_Time();
void Store_the_DataPnt (int lclRepCnt);
void Clear_the_Variables(int lclRepCnt);
void Save_to_SDCard(int lclRepCnt);
#line 33 "/mnt/space/workspace/particle/DL2_Rev_6/src/DL2_Rev_6.ino"
int PROGRAM = 0; //always set the PROGRAM to 0 so you don't apply voltage to something you shouldn't
int DEPTH_READ = A0;   //the read pin for the depth transducer
int FOURTWENTY_PWR = D3;   //power pin that throws the 12V relay
int VELO_READ = A1;    //the read pin for the velocity sensor
int WTRTMP_READ = A2;    //the pin for the temperature sensor
int TEMP_READ = A3;     //TMP36 external temp sensor
int VOLTAGE_READ = A5; //See the end of the read_the_analogs; basically to read the 12V voltage divider
int TEMP_PWR = D4;     //TMP36 power, 3v3
int REED_PIN = D2;     //rain gauge pin.  D8 originally, but that's wake from deep sleep and reset the board now
int RTC_PIN = D8;      //wake up from RTC.  You can wake on any pin now from interrupt, but I wanted to be consistent
//D5 is the chipselect for the SD Card
//Global reads. lowercase vars are those directly read, Upper_Case/Camel_Case are derived
uint16_t depth=0;
uint16_t velocity=0;
uint16_t wtr_temp=0;
uint16_t air_temp=0;
uint16_t voltage = 0; //needs to be a copy of battery charge or an analog read for voltage
uint16_t calc_var=0;
uint16_t DEPTH_OFFSET = 0;
double realtemp = 0.0;
double calc_var_dm = 0.0; //for calculating distance in meters
uint16_t rtc_temp = 0;
volatile int tip_count = 0; 
volatile int Tip_Count_Copy = 0; 
//
//char Revision[] = "DL2R5_26May20_0815";  
//this version had bad depth values being set to -999, which caused a segfault and board reset (depth is uint16t)
//  Also changed the number check on the program number to be >= 0 and && < 100 rather than || 100 -fwb-11Jun20
//
//char Revision[] = "DL2R5_11Jun20_0705"; //see fixes from previous rev above.
//this version had an error with the sleep_interval-13Aug20
char Revision[] = "DL2R5_13Aug20_0750"; //see fixes from previous rev above.
uint8_t RepCnt = 0;
#define DataPts_to_Store 3 //changed to 3 on 22May17.
long unsigned int DateTimelong = 0;
int i = 0;  
long unsigned int STATION = 9900;               // Station ID, unsigned gives us 1-65; Col Cty starts with 30 (5000)
//need to add a web function / callback to change the station number.
char dataPnt00ago[53]="now";
char dataPnt05ago[53]="five ago";
char dataPnt10ago[53]="ten ago";
//char data_array[DataPts_to_Store][48]={0};
int battCharge = 0; //battery charge, as a percent, should be < 255
char message[6][54] = {0}; 
FuelGauge fuel;
int Raining=0;
int Depth_Saved=0;
int Depth_Flag=-1;
int Keep_Alive=0;
int coffee = 0; //catches the 8 AM wake up call. (see loop)
//Sleep Intervals
int short_Nap = 295;//295; #testing
int long_Nap = 3600;//3600; #testing
int Sleep_Interval = 60;
//EEPROM
uint8_t addr=0;
struct EEPROM_Obj {
  //Station, Keep_Alive, PROGRAM, Time_Zone
  uint8_t version;
  long unsigned int eeStation;//=9900;
  int eeKeep_Alive;//=0;
  int eePROGRAM;//=0;
  int eeTimeZone;// = 4;
};
EEPROM_Obj Settings_Obj;
//SD Card & RTC
// The SD card CS pin on the Adafruit AdaLogger FeatherWing is D5.
const int SD_CHIP_SELECT = D5;
SdFat sd;
//setup the RTC
RTCSynchronizer rtcSync;
SdCardPrintHandler printToCard(sd, SD_CHIP_SELECT, SPI_FULL_SPEED);
//fix sleep 13Aug20
unsigned long Time_Wake = 0;
unsigned long Time_Sleep = 0;
unsigned long Nap_Length = 0;

void setup()
{
  Particle.variable("dataPnt00ago", dataPnt00ago);  //IF YOU MESS UP THE VARIABLE, IT WONT REGISTER AN ERROR 
  Particle.variable("dataPnt05ago", dataPnt05ago);  //  IT DOESN"T REGISTER, NOR DOES IT THROW AN ERROR IF YOU MAKE
  Particle.variable("dataPnt10ago", dataPnt10ago);  //  UP A VARIABLE NAME....1Feb202117
  Particle.variable("battCharge", &battCharge, INT);
  Particle.variable("Station", &STATION, INT);
  Particle.variable("KeepAlive", &Keep_Alive, INT);  
  Particle.variable("program", &PROGRAM, INT);
  Particle.variable("Revision", Revision);
  rtcSync.setup();
  //don't plan on using these, so I may as well leave them off.
  Mesh.off();
  BLE.off();
  Serial.begin(19200); //kill the serial for production.  need an ifdef...
  //SerialLogHandler logHandler;
  pinMode(DEPTH_READ, INPUT); 
  pinMode(FOURTWENTY_PWR, OUTPUT); 
  pinMode(VELO_READ, INPUT); 
  pinMode(WTRTMP_READ, INPUT); 
  pinMode(TEMP_READ, INPUT); 
  pinMode(TEMP_PWR, OUTPUT); 
  pinMode(REED_PIN,INPUT_PULLUP);
  attachInterrupt(REED_PIN,Tip_Int_Handler,FALLING);
  Particle.function("Settings",Configuration_Settings);
  //22Aug20_Serial.println( fuel.getSoC() );
  //default to DST unless it's Nov-Feb
  if (Time.month() > 10 || Time.month() < 3) {
    Time.zone(-4);
    Settings_Obj.eeTimeZone = 4;
  }
  //Get the saved settings from eeprom
  EEPROM.get(addr, Settings_Obj);
  if(Settings_Obj.version != 1) {
    // EEPROM was empty -> initialize myObj
    EEPROM_Obj defaultObj = { 1, 9900, 0, 0, 5 };
    Settings_Obj = defaultObj;
    EEPROM.put(addr,Settings_Obj);
  }  
  else {
    STATION=Settings_Obj.eeStation;
    Keep_Alive = Settings_Obj.eeKeep_Alive;
    PROGRAM = Settings_Obj.eePROGRAM;
    int tmpint = Settings_Obj.eeTimeZone;
    tmpint=tmpint*-1;
    Time.zone(tmpint);
  }
  battCharge = int(fuel.getSoC()); //grab it early for the publish
  delay(120000); //leave 120 seconds to set up configurations after reset. one minute is not enough in the rain...-fwb-21May20
}

void loop()
{
  //i = i +1;
  //if (i>1000) {
  //  i=0;
  //}
  //22Aug20_Serial.print("i: ");
  //22Aug20_Serial.println(i);
  rtcSync.loop();
  /*if ((Time.hour() == 8) && (Time.minute() >= 0) && (Time.minute()<=10) { //8AM, between 8:00 and 8:10. Pause for updates for 5 minutes.  Should catch once.
    if (coffee == 0) {
      delay(300000);
      coffee = 1;
    }
  }
  else if ((Time.hour() > 8) && (coffee == 1)) {
      coffee = 0; //reset for the next day
  }
  */
  /*
  Rev 1.5.0 Sleep Updates...might not implement yet...
  SystemSleepConfiguration config;
  config.mode(SystemSleepMode::HIBERNATE)
      .gpio(WKP, RISING)
      .duration(60s);
  SystemSleepResult result = System.sleep(config);
*/
//something going on this morning
  //22Aug20_Serial.print("Sleep Interval: ");
  //22Aug20_Serial.println(Sleep_Interval);
  //sleeps the first time for one minute, then goes on to bed for a nap
  Time_Sleep = Time.local();
  if (Raining == 1 || Keep_Alive == 1) {
    System.sleep(REED_PIN,FALLING,Sleep_Interval,SLEEP_NETWORK_STANDBY); //5 min sleep, leaves network subsystem up
  }  
  else {
    System.sleep(REED_PIN,FALLING,Sleep_Interval);//,SLEEP_NETWORK_STANDBY); //leaving the network on standy for testing //hourly sleep,shuts down the network subsystem #testing
  }
  SleepResult result = System.sleepResult();
  int reason = result.reason();
  //22Aug20_Serial.print("sleepresult: ");
  //22Aug20_Serial.println(reason);
  /*reason =  0:  WAKEUP_REASON_NONE
              1:  WAKEUP_REASON_PIN
              2:  WAKEUP_REASON_RTC
              3:  WAKEUP_REASON_PIN_OR_RTC
  */
  if (reason == 0) {
    //you checked the sleepResult and nothing was set (prob because you didnt sleep)
  }
  else if (reason == 1) {
    //pin wakeup; 
    //The ISR handles the wakeup.  
    // actually, its raining, so start the reporting process at 5 minutes
    // set raining=1, and set it as a particle variable so its easy to track.  20 cloud variables can be named
    // then set the sleep mode to network standby and the sleep cycle to 5 minutes; this should ensure alarming
    // and we can update the rain data every 5 minutes, along with depth
    // when its not raining, then we will set the sleep time to 15 minutes or one hour and set the sleep mode to 
    // network sleep, and then only update the latest dataPt00ago. 
    //  just thinking: 3 dataPts (strings) and rain, depth, vel, temp, wtr_temp, turbidity, 
    //  batt could be posted in real-time, and need a revision number
    //  also, from the API, you can add notes to the station such as "Rae's Creek Sta 10"
    //13Aug20
    // added a section to figure out which pin woke us up.  Prelude to a push-button status tool for the 
    //  landfill raingage or any other similar tool
    pin_t WAKE_PIN = result.pin();
    if (WAKE_PIN == REED_PIN) {
      Raining = 1;
      if (Depth_Flag == -1) {
        Depth_Flag = 0;
      }
      Tip_Count_Copy = tip_count;
      Time_Wake = Time.local();
      //13Aug20: recalculate the sleep interval based on how long we've slept already.  Because the system handles
      //  RTC and PIN wakeup, a reed_switch wakeup was resetting the sleep interval.  This resulted in rain totals
      //  only being available after it had finished raining (for 5 minutes). 
      //23Aug20_long int time_asleep = (Time_Wake - Time_Sleep);
      //22Aug20_Serial.print("time_asleep: ");
      //22Aug20_Serial.println(time_asleep);
      if ((Sleep_Interval - (Time_Wake - Time_Sleep)) < 0) {
        Serial.print ("just a placeholder");
        Sleep_Interval = 10;
        Figure_the_Time();
      }
      Sleep_Interval = Sleep_Interval - (Time_Wake - Time_Sleep); //Sleep_Interval reduced by seconds its already slept
      //22Aug20_Serial.print("Sleep Interval: ");
      //22Aug20_Serial.println(Sleep_Interval);
      //testing to allow the console time to communicate: 
      //22Aug20_delay(3000);
      //This was/is (I believe) the problem: this used to read < 0, but if/when it actually was 0,
      //  the system sleeps with no wakeup, except by the pin, which is then gets reset here to a 
      //  negative number and resets the sleep_interval.  That's my best guess based on looking at 
      //  the data and trying to debug this issue where it would sleep for 10-15 minutes, but count
      //  all of the rainfall, which was being generated by a photon on a counter.  Changed to
      //  Sleep_Interval <=0 and then set Sleep_Interval to 1 to force the system to wakeup for pin.
      //  --fwb-24Aug20_0815
      if (Sleep_Interval <= 0) {
        //22Aug20_Serial.print("Sleep Interval LT 0: ");
        //22Aug20_Serial.println(Sleep_Interval);
        //Sleep_Interval = short_Nap;//295; //little less than 5 minutes to allow for wakeup, connect, etc.
        Sleep_Interval = 1;
      }
      else if (Sleep_Interval > short_Nap) { //not sure why, but it's early today
        //22Aug20_Serial.print("Sleep Interval GT SN: ");
        //22Aug20_Serial.println(Sleep_Interval);
        Sleep_Interval = short_Nap;//295; //little less than 5 minutes to allow for wakeup, connect, etc.
      }
      //Sleep_Interval = short_Nap;//295; //little less than 5 minutes to allow for wakeup, connect, etc.
    }
    //else wake pin was push button or something similar
  }
  else if (reason == 2) {
    Read_the_Analogs();
    Figure_the_Time();
    
    //no rain, hourly read
    //raining, measure in 5 min increments, network on standby, vars updated every 5
    //rain has ended, keepalive is set, when does depth drop so we can go back to hourly logging
    //not raining, but Keep_Alive is set to force the system to stay awake and post every 5 minutes
    if (Raining == 1 || Keep_Alive == 1) {
      Store_the_DataPnt(RepCnt); //moved here to store based on RepCnt
      //if (Keep_Alive == 1) {
        //used to set Sleep_Interval based on Keep_Alive, but it really should be reset here for 
        //  Raining or Keep_Alive.  The Sleep_Interval is shortened in the wakeup_reason = 1 above
        //  which is Reed_Pin wakeup.  If I don't reset here during raining, then it'll never get reset.
        //  Basically, it gets reset every time the alarm goes off if its raining or keep_alive is set.
        //  fwb-15Aug20
      //22Aug20_Serial.print("Sleep Interval Set in Raining: ");
      //22Aug20_Serial.println(Sleep_Interval);
      Sleep_Interval = short_Nap; //295; //little less than 5 minutes to allow for wakeup, connect, etc.
      //}
      if (Depth_Flag == 0) {
        Depth_Saved = depth;
        Depth_Flag = 1;
      }
      if (RepCnt == 2) { 
        //copy the data string out to variables so the system will post the variables.  
        // inverted the publish order so they appear correct when received
        Particle.publish("Data10", dataPnt10ago, PRIVATE);
        Particle.publish("Data05", dataPnt05ago, PRIVATE);
        Particle.publish("Data00", dataPnt00ago, PRIVATE);
        Save_to_SDCard(2);
        if (Depth_Flag > 0) {
          Depth_Flag += 1;
        }
        if (Depth_Flag > 8) { //8 15 minute periods, or 2 hours
          if (abs(Depth_Saved-depth)<=8){ //depth is within 5 hundredths of its original value.  switched to 8 b/c maxbotix tends to bounce around a bit with normal readings.  
             //I believe the maxbotix has a wide target area and tends to get a lot of readings that vary by +/- 4-5.  lengthened the power up time and samples to try to counter.
            Depth_Flag = -1;
            Clear_the_Variables(0); //clears all including Raining and RepCnt
            //22Aug20_Serial.print("Sleep Interval SET by DEPTHFLAG: ");
            //22Aug20_Serial.println(Sleep_Interval);
            if (Keep_Alive == 1) {
              Sleep_Interval = short_Nap; //295;
            }
            else {
              Sleep_Interval = long_Nap; //3600;
            }
          }
        }
        Clear_the_Variables(1); //doesn't clear Raining BUT DOES CLEAR RepCnt.  have to clear after the Depth_Saved-depth check in Depth_Flag>8
        // moved to Clear_the_Variables(1)RepCnt = 0; //reset the 5/15 min increment counter
      }
      else {
        RepCnt = RepCnt+1;
        Clear_the_Variables(99); //doesn't clear Raining and RepCnt
      }
    }
    else { //hourly reporting
      RepCnt = 0;
      Store_the_DataPnt(2); //only store RepCnt as 00ago
      /*either move the analog reads or copy the 10 ago to 00 ago here 
      need to save enough time to allow the logger to post the variables.
      Particle.publish pushes the value out of the device at a time controlled by the device firmware.
      Particle.variable allows the value to be pulled from the device when requested from the cloud side.
      Store_the_DataPnt(0);
      Clear_the_Variables(); */
      Particle.publish("Data00", dataPnt00ago, PRIVATE);
      Clear_the_Variables(0); //clears all including Raining and RepCnt
      Save_to_SDCard(0);
      //22Aug20_Serial.print("Sleep Interval SET IN HOURLY: ");
      //22Aug20_Serial.println(Sleep_Interval);
      Sleep_Interval = long_Nap; //3600;
    }
  }
  else if (reason == 3) {
    //not sure how this one works either
  }
  else {
    //error
  }
}

int Configuration_Settings(String command) {
    /* Particle.functions always take a string as an argument and return an integer.
    Since we can pass a string, it means that we can give the program commands on how the function should be used.
    In this case, telling the function "on" will turn the LED on and telling it "off" will turn the LED off.
    Then, the function returns a value to us to let us know what happened.
    In this case, it will return 1 for the LEDs turning on, 0 for the LEDs turning off,
    and -1 if we received a totally bogus command that didn't do anything to the LEDs.
    memset(tmpstr,0,sizeof(tmpstr));
	  strncpy(tmpstr,message+6,10);
	  DateTimelong[RepCnt2] = atol(tmpstr);
    */
  //char tmpstr[11] = {0};
  if (command.startsWith("STA")) {
	  command = command.substring(4,9);
    if ((atol(command) > 0) || (atol(command)<10000)) {
      STATION = atol(command);
      Settings_Obj.eeStation = STATION;
      EEPROM.put(addr,Settings_Obj);
      return (STATION);
    }
    else {
      STATION=9900;
      return -1;
    }
  }
  if (command.startsWith("VKA")) {
	  command = command.substring(4,6);
    if (atol(command) == 0) {
      Keep_Alive = atol(command);
      Sleep_Interval = long_Nap;//3600;
      Settings_Obj.eeKeep_Alive = Keep_Alive;
      EEPROM.put(addr,Settings_Obj);
      return (0);
    }
    else if (atol(command) == 1) {
      Keep_Alive = atol(command);
      Sleep_Interval = short_Nap;//295;
      Settings_Obj.eeKeep_Alive = Keep_Alive;
      EEPROM.put(addr,Settings_Obj);
      ////EEPROM.put(addr,value);
      return (1);
    }
    else {
      return -1;
    }
  }
  else if (command.startsWith("TZS")) {
    command = command.substring(4,6);
	  if ((atol(command) == 4)) {
      Time.zone(-4); //EDT is -4, EST is -5
      Settings_Obj.eeTimeZone = 4;
      EEPROM.put(addr,Settings_Obj.eeTimeZone);
      return (4);
    }
    if ((atol(command) == 5)) {
      Time.zone(-5); //EDT is -4, EST is -5
      Settings_Obj.eeTimeZone = 5;
      EEPROM.put(addr,Settings_Obj.eeTimeZone);
      return (5);
    }
    else {
      return -1;
    }
  }
  else if (command == "CTV") {
    Clear_the_Variables(0);
  }
  else if (command.startsWith("PGR")) {
	  command = command.substring(4,7);
    //22Aug20_Serial.print("Command: ");
    //22Aug20_Serial.println(command);
    //strncpy(tmpstr,command+4,2);
	  if ((atol(command) >= 0) && (atol(command)<100)) { //changed to and equal 0 and LT 100 rather than OR LT 100
      PROGRAM = atol(command);
      Settings_Obj.eePROGRAM = PROGRAM;
      EEPROM.put(addr,Settings_Obj.eePROGRAM);
      return (PROGRAM);
      //22Aug20_Serial.print("PROGRAM: ");
      //22Aug20_Serial.println(PROGRAM);
    }
    else {
      return -1;
    }
  }
  else {
    return -1;
  }
  //memset(tmpstr,0,sizeof(tmpstr));
  return 0;
}

int Read_the_Analogs() { 
//No Program (0): set all values to 9999
//  1: 4-20  Bigfoot depth and velocity (1)
//  2: 4-20  AST depth and water temp (2)
//  3: 4-20  Stevens depth (3)
//  4: 4-20  Banner depth ultrasonic (4)
//  5: 3V3   Maxbotix depth ultrasonic (5)
//  6: 3V3   Maxbotix depth ultrasonic with temp correction (6)
//  7: 3V3   AST 0-2.5V depth pressure transducer (7)
//  8: Campbell turbidity probe (8)
//  9: paddlewheel velocity probe (9)
/*
int PROGRAM = 0;
int DEPTH_READ = A0;   //the read pin for the depth transducer
int FOURTWENTY_PWR = D3;   //power pin that throws the 12V relay
int VELO_READ = A1;    //the read pin for the velocity sensor
int WTRTMP_READ = A2;    //the pin for the temperature sensor
int TEMP_READ = A3;     //TMP36 external temp sensor
int TEMP_PWR = D4;     //TMP36 power, 3v3
int REED_PIN = D2;     //rain gauge interrupt pin.  D8 originally, but that's wake from deep sleep and reset the board now
int VELO_PIN = D7;     //paddlewheel velocity probe interrupt; can use REED for now.-fwb-9May20
*/
/*
https://docs.particle.io/reference/device-os/firmware/boron/#analogread-adc-
Reads the value from the specified analog pin.
The Gen 3 Feather devices (Argon, Boron, Xenon) have 6 channels (A0 to A5) with a 12-bit resolution. 
This means that it will map input voltages between 0 and 3.3 volts into integer values between 0 and 4095. 
This yields a resolution between readings of: 3.3 volts / 4096 units or, 0.0008 volts (0.8 mV) per unit.
The sample time to read one analog value is 10 microseconds.
The Boron SoM has 8 channels, A0 to A7.
// SYNTAX
analogRead(pin);
analogRead() takes one argument pin: the number of the analog input pin to read from (A0 - A5)
analogRead() returns an integer value ranging from 0 to 4095.
*/
if (PROGRAM == 0) {
  depth = 9999;
  velocity = 9999;
  wtr_temp = 9999;
  air_temp = 9999;
  return (0);
}
if (PROGRAM == 1) {
  //Greyline Depth-Vel Sensor, reads (4-20) 0.5-2.5V, 0-12ft, 0.5V = 620, 2.5V = 3100
  // website says  0.25 to 9.84 ft/s, manual says 0.0-9.84 ft/s
  //just another hack...
	//it turns out the bigfoot has some logic on-board, and it does some averaging of the readings and is quite delayed once it 
	// starts reading.  For that reason, we're going to slow down and take a reading every second for 20 seconds and then
	// average those.  A full 775*10*2 = 15,500, so we still shouldn't overflow anything. -fwb-3May20
  short_Nap = 280; //shorten the sleep time since we're lengthening the awake time
  digitalWrite(FOURTWENTY_PWR, HIGH); 
  delay(10000); //let everyone stabilize.  The velocity probe takes 8 seconds per the manual; I upped it to 10.
  for (int j = 0; j<9 ; j++) { //uint16_t max is 65535, max ADC is 4095*5 = ~20000
    depth += analogRead(DEPTH_READ);
    delay(100); //
    velocity += analogRead(VELO_READ); 
    // extended this to 900 milliseconds because this unit averages its own readings and takes a bit to adjust
    delay(900); 
  }
  depth = int(depth/10);
  velocity = int(velocity/10);
  if (depth < 620) {
	  if(depth == 0) {
		  depth = 9999;  //this is basically an error reading.  I had also set this to -999 which was causing a buffer overflow.
	  }
	  else {
		  depth = 620;  //if the read value is less than 620, it reads less than 0, which ends up giving values around 65530
  	}
  }
  calc_var = map(depth, 620, 3100, 0, 1200); 
  depth = calc_var;
  calc_var = map(velocity, 620, 3100, 0, 984); 
  velocity = calc_var; 
  calc_var = 0;
  digitalWrite(FOURTWENTY_PWR, LOW); 
  }
if (PROGRAM == 2) {
  //AST45PT Sensor, reads (4-20) 0.5-2.5V, 0-5 psi (1163 ft), 0.5V = 620, 2.5V = 3100
  // 32-140 deg-F
  digitalWrite(FOURTWENTY_PWR, HIGH); 
  delay(500); //let everyone stabilize.  
  for (int j = 0; j<5 ; j++) { //uint16_t max is 65535, max ADC is 4095*5 = ~20000
    depth += analogRead(DEPTH_READ);
    delay(100); //
    wtr_temp += analogRead(WTRTMP_READ); 
    delay(100); //
  }
  depth = int(depth/5);
  wtr_temp = int(wtr_temp/5);
  if (depth < 620) {
	  if(depth == 0) {
		  depth = 9999;  //there is an error reading
	  }
	  else {
		  depth = 620;  //if the read value is less than 620, it reads less than 0, which ends up giving values around 65530
  	}
  }
  calc_var = map(depth, 620, 3100, 0, 1163); 
  depth = calc_var;
  calc_var = map(wtr_temp, 620, 3100, 32, 140); 
  wtr_temp = calc_var; 
  calc_var = 0;
  digitalWrite(FOURTWENTY_PWR, LOW); 
}
if (PROGRAM == 3) {
  //Stevens 4-20 sensor, reads from 0.5-2.5V, 0-10ft, 0.5V = 620, 2.5V = 3100
 digitalWrite(FOURTWENTY_PWR, HIGH); 
  delay(500); //let everyone stabilize.  
  for (int j = 0; j<5 ; j++) { //uint16_t max is 65535, max ADC is 4095*5 = ~20000
    depth += analogRead(DEPTH_READ);
    delay(100); //
  }
  depth = int(depth/5);
  if (depth < 620) {
	  if(depth == 0) {
		  depth = 9999;  //there is an error reading
	  }
	  else {
		  depth = 620;  //if the read value is less than 620, it reads less than 0, which ends up giving values around 65530
  	}
  }
  calc_var = map(depth, 620, 3100, 0, 1000); 
  depth = calc_var;
  calc_var = 0;
  digitalWrite(FOURTWENTY_PWR, LOW); 
}
if (PROGRAM == 4) {
//Banner 4-20 ultrasonic sensor, reads from 0.5-2.5V, 0-14ft*/
// I think I trained it to 1-15', so 0-14' is the range i used.
// after some thought, I trained it backwards so that it was reading depth correctly.
// so there is no need to subtract the depth from a base depth.
  digitalWrite(FOURTWENTY_PWR, HIGH); 
  delay(1000); //let everyone stabilize.  
  for (int j = 0; j<5 ; j++) { //uint16_t max is 65535, max ADC is 4095*5 = ~20000
    depth += analogRead(DEPTH_READ);
    delay(100); //
  }
  depth = int(depth/5);
  if (depth < 620) {
	  if(depth == 0) {
		  depth = 9999;  //there is an error reading
	  }
	  else {
		  depth = 620;  //if the read value is less than 620, it reads less than 0, which ends up giving values around 65530
  	}
  }
  calc_var = map(depth, 620, 3100, 0, 1400); 
  depth = calc_var;
  calc_var = 0;
  digitalWrite(FOURTWENTY_PWR, LOW); 
}
if (PROGRAM == 5) {
  //Maxbotix 3V3/5V ultrasonic.  Datasheet states that the analog is limited to 0-~600cm
  // which is 0-19.685' at 0-3V3 supply.
  // okay, actually this guy reads 3.2mV/cm at 3V3, and maxes out at 600cm (19.685') at 3V3 supply
  // so, the actual range is 600cm(3.2mV/cm)=1920mV. there's about ~1241 units/V on the 
  // ADC (4095/3.3)*1.92 which is 0-2382 for 0-19.68'.
  // Going to need a basedepth to subtract from or just invert the readings...
  // Got a lot of conflicting info in my brain about whether this should read to 765cm or 600cm
  // going to establish a base value to subtract the reading from rather than guessing and reading
  // this one backwards.  
  // ahhh, well, we'll figure it out.  For now just reversing the reading. -fwb-19feb20_1943
  // ADD A DECOUPLING CAP ON THE POWER CONNECTOR TO PREVENT POWER FLUCTUATIONS AFFECTING THE READING - per the datasheet 100uF
  // got an email from maxbotix support, yes the unit maxes out at 3V3 at about 1.92V. -fwb-15May20
  // 2200/4095*3V3= 1.77V = 1770mV / 3.2mV/cm = 553 cm = 217 inches = 18.14' (within 1.5' of the probe)
  // TODO: Add temp compensation per their website
  // App note: 15May20: running the feather relay pulls about 50 mA (per the website) and prevents
  //   the ultrasonic from getting more than about 2.6V, and it won't work.  So, you get 
  //   perpetually low readings and it reads full all the time. 
  //   after thinking about it, i believe this is because there is no power on the relay, and 
  //   so all of the current has to come from the MCU and not from the external power supply. duh...
  short_Nap = 290; //shorten the sleep time since we're lengthening the awake time
  digitalWrite(FOURTWENTY_PWR, HIGH); //connect directly to pin instead of the relay
  delay(5000); //let everyone stabilize.  trying to get the first few datapoints to stabilize. 
  depth = analogRead(DEPTH_READ);
  int jj = 0;
  while ( depth < 100) { //see above.  unit keeps getting stuck in high reads.  See note about feather relay
    jj++;
    delay(100);//delay 1/10 second
    depth = analogRead(DEPTH_READ);
    //22Aug20_Serial.print("depth: ");
    //22Aug20_Serial.println(depth);
    delay(500); //for debugging
    if (jj>100) { //give up.  It's been 10 seconds... 
      break;
    }
  }
  depth=0; //reset from the test reads above.  We were picking up an extra read and introducing some error in our data -wb-25May20
  for (int j = 0; j<8 ; j++) { //uint16_t max is 65535, max ADC is 4095*5 = ~20000, *8 is ~40000
    depth += analogRead(DEPTH_READ);
    //22Aug20_Serial.print("j : ");
    //22Aug20_Serial.print(j);
    //22Aug20_Serial.print(" depth : ");
    //22Aug20_Serial.println(depth);
    delay(100); //
  }
  depth = int(depth/8); //changed to 8 to get more samples, hoping for a better average
  calc_var = map(depth, 0, 2392, 1968, 0); //swapped this to read the opposite.  
  // if you're close, then it should read a greater value, and this reads 0-3V3.
  //  see note at the top on this map calculation
  depth = calc_var;
  calc_var = 0;
  digitalWrite(FOURTWENTY_PWR, LOW); 
}
if (PROGRAM == 6) {
  //Maxbotix 3V3/5V ultrasonic.  Datasheet states that the analog is limited to 0-~600cm
  // See notes above for overall setup
  // Per Maxbotix Tech Doc, Temp Adjustment on the Analog read for the XL-Maxsonar (our 7092)
  //  is: Dm=(Vm/(Vcc/1024)*(58e-06microSeconds))*(20.05*SQRT(Tc+273.15)/2)
  //    where Dm = distance measured, meters
  //    Vm = Voltage output from device, measured by user
  //    Vcc = supply voltage (3V3)
  //    Tc = temperature in Celsius
  short_Nap = 290; //shorten the sleep time since we're lengthening the awake time
  digitalWrite(FOURTWENTY_PWR, HIGH); //connect directly to pin instead of the relay
  delay(5000); //let everyone stabilize.  trying to get the first few datapoints to stabilize. 
  depth = analogRead(DEPTH_READ); 
  int jj = 0;
  while ( depth < 100) { //see above.  unit keeps getting stuck in high reads.  See note about feather relay
    jj++;
    delay(100);//delay 1/10 second
    depth = analogRead(DEPTH_READ);
    //22Aug20_Serial.print("depth: ");
    //22Aug20_Serial.println(depth);
    delay(500); //for debugging
    if (jj>100) { //give up.  It's been 10 seconds... 
      break;
    }
  }
  depth=0; //reset from the test reads above.  We were picking up an extra read and introducing some error in our data -wb-25May20
  for (int j = 0; j<8 ; j++) { //uint16_t max is 65535, max ADC is 4095*5 = ~20000, *8 is ~40000
    depth += analogRead(DEPTH_READ);
    //22Aug20_Serial.print("j : ");
    //22Aug20_Serial.print(j);
    //22Aug20_Serial.print(" depth : ");
    //22Aug20_Serial.println(depth);
    delay(100); //
  }
  depth = int(depth/8); //changed to 8 to get more samples, hoping for a better average
  //22Aug20_Serial.print("Depth reading: ");
  //22Aug20_Serial.println(depth);
  calc_var = map(depth, 0, 2392, 1968, 0);
  //22Aug20_Serial.print("Depth uncorrected: ");
  //22Aug20_Serial.println(calc_var);
  calc_var=0;
  calc_var = map(depth, 0, 2392, 0, 1920); //converting reading back to voltage; see app note on section 5 above
  depth = calc_var;
  //22Aug20_Serial.print("Depth voltage: ");
  //22Aug20_Serial.println(depth);
  //grab the temperature
  digitalWrite(TEMP_PWR, HIGH); 
  delay(100);
  air_temp = analogRead(TEMP_READ); 
  calc_var = map(air_temp,0,4095,0,3300);
  realtemp = ((calc_var / 10.)) - 50; //temp in C.  the formula is 100*temp2 - 50, but we're reading to 3V3 (3300), which is the same as /10 
  //22Aug20_Serial.print("realtemp: ");
  //22Aug20_Serial.println(realtemp);
  //calculate the new depth based on temp
  //I think there's an error here.  If I use integers in the math, it truncates the 3300/1024 to an int of 3, and 
  //  that throws the calculation off since it's really about 3.22
  //  Further research indicates that C assumes integer math should be returned as an integer.  Makes sense; but 
  //  just another item on the learning curve.
  //calc_var_dm = (depth/(3300/1024));
  calc_var_dm = (float(depth)/float(3300./1024.));
  //22Aug20_Serial.print("cv_dm_2: ");
  //22Aug20_Serial.println(calc_var_dm);
  calc_var_dm = calc_var_dm * 0.000058; //wow, read that 5.8e-6 but its 58e-6
  //22Aug20_Serial.print("cv_dm_3: ");
  //22Aug20_Serial.println(calc_var_dm);
  calc_var_dm = calc_var_dm*(20.05*sqrt(realtemp+273.15)/2); //depth in meters
  //22Aug20_Serial.print("cv_dm_4: ");
  //22Aug20_Serial.println(calc_var_dm);
  calc_var_dm = calc_var_dm/0.3048; //depth in feet
  //22Aug20_Serial.print("cv_dm_5: ");
  //22Aug20_Serial.println(calc_var_dm);
  depth = int(calc_var_dm*100); //depth in hundredths
  //22Aug20_Serial.print("cv_dm_6: ");
  //22Aug20_Serial.println(calc_var_dm);
  calc_var = map(depth, 0, 1968, 1968, 0); //reverse the calc to measure depth, not distance to target
  //22Aug20_Serial.print("cv_dm_7: ");
  //22Aug20_Serial.println(calc_var);
  depth = calc_var;
  //22Aug20_Serial.print("depth: ");
  //22Aug20_Serial.println(depth);
  calc_var = 0;
  calc_var_dm = 0;
  digitalWrite(FOURTWENTY_PWR, LOW); 
}
if (PROGRAM == 7) { //old AST low voltage transducer, reads from 0.5-2.5V, 0-5 psi (1163 ft), 0.5V = 620, 2.5V = 3100
  digitalWrite(FOURTWENTY_PWR, HIGH); 
  delay(500); //let everyone stabilize.  
  for (int j = 0; j<5 ; j++) { //uint16_t max is 65535, max ADC is 4095*5 = ~20000
    depth += analogRead(DEPTH_READ);
    delay(100); //
  }
  depth = int(depth/5);
  if (depth < 620) {
	  if(depth == 0) {
		  depth = 9999;  //there is an error reading
	  }
	  else {
		  depth = 620;  //if the read value is less than 620, it reads less than 0, which ends up giving values around 65530
  	}
  }
  calc_var = map(depth, 620, 3100, 0, 1163); 
  depth = calc_var;
  calc_var = 0;
  digitalWrite(FOURTWENTY_PWR, LOW); 
}
//read all the other vars
  digitalWrite(TEMP_PWR, HIGH); 
  delay(100);
  air_temp = analogRead(TEMP_READ); 
  calc_var = map(air_temp,0,4095,0,3300);
  realtemp = ((calc_var / 10.)) - 50; //temp in C.  the formula is 100*temp2 - 50, but we're reading to 3V3 (3300), which is the same as /10 
  realtemp = ((realtemp * 9. / 5.) + 32.); //temp in F
  air_temp = int(realtemp);
  //turn everything off
  digitalWrite(FOURTWENTY_PWR, LOW); 
  digitalWrite(TEMP_PWR, LOW); 
  battCharge = int(fuel.getSoC());
  realtemp = (battCharge*3.7);
  voltage = int(realtemp);
  realtemp = 0;
  //22Aug20_Serial.print("voltage SoC: ");
  //22Aug20_Serial.println(voltage);
  //a hack to read the 12V battery voltage off of the R152 power board.  That board has a voltage divider
  //  to read the voltage, which is read on A0.  So, wire A0 to A5 since it's closest to that pin sort of...
  //  that divider peaks at 3V3 when the battery is floating over 13V.  The 328P could accept higher voltages
  //  but the boron says 3V3 max.  Going to add a small resistor in series to knock down the voltage.  
  if (voltage < 20) {
    realtemp = analogRead(VOLTAGE_READ);
    voltage = int(realtemp);
    //22Aug20_Serial.print("voltage splitter: ");
    //22Aug20_Serial.println(voltage);
  }
  //last attempt to catch problems before they overflow the dataPnts
  //velocity, air_temp, depth, voltage, rtc_temp, wtr_temp
  if (velocity > 1500) {
    velocity = 9999;
  } 
  if (depth > 3000) {
    depth = 9999;
  } 
  if (wtr_temp > 150) {
    wtr_temp = 9999;
  }
  if (air_temp > 150) {
    air_temp = 9999;
  }
  if (voltage > 1500) {
    voltage = 9999;
  }
  if ((velocity > 1500)||(depth > 3000)||(wtr_temp > 150)||(air_temp > 150)||(voltage > 1500) ) {
    return (9);
  }
  return(0);
}

//   An INT_0 interrupt is generated by the rain gauge reed switch.
void Tip_Int_Handler() {
  int level = 0;
  tip_count += 1;
  //  digitalWrite(13, HIGH);
  //delay(100); can't use delays in an ISR on the boron because they rely on interrupts.  it redlights the board. -fwb-31Jan20_2255
  //delayMicroseconds(1000);
  while (level == 0) {
    level = digitalRead(REED_PIN);
  }
  //  digitalWrite(13, LOW);
}

void Figure_the_Time() {
  //Time.zone(-5); //Eastern Std Time; EDT is -4: moved to setup to grab from EEPROM
  DateTimelong = Time.month() * 100000000;
  DateTimelong = DateTimelong + Time.day() * 1000000;
  unsigned long int shortyear = Time.year() - 2000;
  DateTimelong = DateTimelong + shortyear * 10000;
  DateTimelong = DateTimelong + Time.hour() * 100;
  DateTimelong = DateTimelong + Time.minute() * 1;

}

void Store_the_DataPnt (int lclRepCnt) {
  // WAS ORIGINALLY SETTING THE dataPnt00ago here
  //char message[6][48] = {0}; 
  // Format the message: D1,A0,A1,A2,A3, Rain, Light, Temp, Depth, A3 (reserved)
  // sprintf(message, "%5lu:%010lu:%04d:%04u:%04u:%04u:%04u:%04u\n", STATION, DateTimelong[lclRepCnt], Tip_Count_Copy[lclRepCnt], light[lclRepCnt], temp[lclRepCnt], depth[lclRepCnt], voltage[lclRepCnt], temp_RTC[lclRepCnt]);
  if (lclRepCnt == 0) {
  sprintf(dataPnt10ago, "%5lu:%010lu:%04d:%04u:%04u:%04u:%04u:%04u:%04u\n", STATION, DateTimelong, Tip_Count_Copy, velocity, air_temp, depth, voltage, rtc_temp, wtr_temp);
  //sprintf(message, "%5lu:%010lu:%04d:%04u:%04u:%04u:%04u:%04u\n", STATION, DateTimelong[lclRepCnt], Tip_Count_Copy[lclRepCnt], velocity[lclRepCnt], air_temp[lclRepCnt], depth[lclRepCnt], wtr_temp[lclRepCnt]);
  }  
  if (lclRepCnt == 1) {
  sprintf(dataPnt05ago, "%5lu:%010lu:%04d:%04u:%04u:%04u:%04u:%04u:%04u\n", STATION, DateTimelong, Tip_Count_Copy, velocity, air_temp, depth, voltage, rtc_temp, wtr_temp);
  //sprintf(message, "%5lu:%010lu:%04d:%04u:%04u:%04u:%04u:%04u\n", STATION, DateTimelong[lclRepCnt], Tip_Count_Copy[lclRepCnt], velocity[lclRepCnt], air_temp[lclRepCnt], depth[lclRepCnt], wtr_temp[lclRepCnt]);
  }  
  if (lclRepCnt == 2) {
  sprintf(dataPnt00ago, "%5lu:%010lu:%04d:%04u:%04u:%04u:%04u:%04u:%04u\n", STATION, DateTimelong, Tip_Count_Copy, velocity, air_temp, depth, voltage, rtc_temp, wtr_temp);
  //sprintf(message, "%5lu:%010lu:%04d:%04u:%04u:%04u:%04u:%04u\n", STATION, DateTimelong[lclRepCnt], Tip_Count_Copy[lclRepCnt], velocity[lclRepCnt], air_temp[lclRepCnt], depth[lclRepCnt], wtr_temp[lclRepCnt]);
  }  
  /* this is broken and needs to be fixed.  it's only moving the same string down the line rather than backing up.  fwb-23Aug20_1700
  for (int j = 0; j<6 ; j++) { //watch the lengths on these strcpy's. I had set message to [48] but the strcpy threw in a \n at the end and segfaulted, so need to be careful.
    if (j==0) {
      strcpy(message[j],dataPnt00ago); //copy the latest point to the array
    }
    else { 
      strcpy(message[j],message[j-1]); //backup the remaining data
    }
  }
  */
}

void Clear_the_Variables(int lclRepCnt) {
  if (lclRepCnt == 0) {
    RepCnt = 0;
    Raining = 0;
  }
  if (lclRepCnt == 1) { //only clear RepCnt, not Raining
    RepCnt = 0;
  }
  tip_count = 0;
  Tip_Count_Copy = 0;
  depth = 0;
  velocity = 0; 
  wtr_temp = 0;
  air_temp = 0;
  rtc_temp = 0;
  voltage = 0;
  realtemp = 0;
  calc_var = 0;
}

void Save_to_SDCard(int lclRepCnt) {
  if (lclRepCnt == 0) {
    printToCard.print(dataPnt00ago);
    delay(500);
  }
  else if (lclRepCnt == 2) {
    printToCard.print(dataPnt10ago);
    delay(500);
    printToCard.print(dataPnt05ago);
    delay(500);
    printToCard.print(dataPnt00ago);
    delay(500);
  }
/*
Strategy for reading the logs to send back over the air
Open and confirm the file is open for reading
read a line at a time, parse the date
compare the date to the required date

*/

/*	File myFile;

	// Initialize the library
	if (!sd.begin(SD_CHIP_SELECT, SPI_FULL_SPEED)) {
		Serial.println("failed to open card");
		return;
	}

	// open the file for write at end like the "Native SD library"
	if (!myFile.open("test.txt", O_RDWR | O_CREAT | O_AT_END)) {
		Serial.println("opening test.txt for write failed");
		return;
	}
	// if the file opened okay, write to it:
	Serial.print("Writing to test.txt...");
	myFile.println("testing 1, 2, 3.");
	myFile.printf("fileSize: %d\n", myFile.fileSize());

	// close the file:
	myFile.close();
	Serial.println("done.");

	// re-open the file for reading:
	if (!myFile.open("test.txt", O_READ)) {
		Serial.println("opening test.txt for read failed");
		return;
	}
	Serial.println("test.txt content:");

	// read from the file until there's nothing else in it:
	int data;
	while ((data = myFile.read()) >= 0) {
		Serial.write(data);
	}
	// close the file:
	myFile.close();
  */
}
