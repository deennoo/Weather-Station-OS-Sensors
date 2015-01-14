/*
 Manchester Decoding, (reading by delay rather than interrupt)
 Rob Ward 2014
 This example code is in the public domain.
 Use at your own risk, I will take no responsibility for any loss whatsoever its deployment.
 Visit https://github.com/robwlakes/ArduinoWeatherOS for the latest version and
 documentation. Filename: ZZZZZZZ.ino
 
 Main_Version_20, 5 November 2014
 
 Pressure and Humidity added in
 
 The aim of this version is use the new Manchester decoding routine and incorporate detection
 of a Oregon compatible sensor built to cover novel environmental sensors.
 For example we could have remote temperature sampling, bat detectors, lightning detectors,
 solar power detectors, and earthquake detectors, all reporting back to the WWW server.
 The advantage is that by using the Oregon Scientific Protocol a person can incorporate
 their excellent weather sensors into the overall design and largely keep the Arduino Rx software
 design relatively simple, ie also use their Manchester implementation for our own sensors.
 
 It may seem this program is encouraging people to build their own replacements for the
 Oregon Scientific WRMR86 sensors, however it is quite the reverse.  The program is designed
 encourage people to purchase the Oregon WRMR86 gear to get the basic wind, temperature, rain
 and humidity sensors and then extend the range with original designs as suggested above.
 If the Oregon sensors are not used, then it does not make sense to use their protocol,
 and any RF other protocol could be as good, or maybe even better in some circumstances.
 
 Before I discovered the Oregon products, and designed a receiver for it, I was seriously
 considering building my own basic weather sensors. Now I am glad I did not try as the
 quality and price of the Oregon sensors are a very good economic and reliable alternative.
 
 Have we made the Oregon base station with the LCD redundant?  Well no, not at all. I have
 it in the kitchen and it used by members of the family from day to day and does not take
 anything extra to have it performing a useful monitoring job as well.  However if you want to
 build a system that uses weather data, and combines that with other sensors, and then performs
 other tasks such as switching pumps, or moving blinds then adding an Arduino based 433MHz Rx
 makes an incredibly versatile platform to work with.
 
 Have we made the USB to PC models redundant?  Again no, unless you want to take a long time
 writing your own logging and graphing routines.  This program does not even begin to replace
 that level of functionality.
 
 Have we made the Anywhere OS Internet connection system redundant? Finally, again no!
 This simple Arduino system does not report live to the internet and it would also take a lot
 of effort to even go close to include the features of the OS internet connection system.
 
 I would encourage anyone looking for a weather sensor based project to consider the Oregon
 Scientific products as they produce a wide range of economically priced but solidly built
 weather sensors.  The mechanical engineering is very good and hard to replicate by the "Geek
 in the Street".  However the scope to extend the usefulness of this data in our designs, by
 combining it with our own sensors (eg soil moisture, or dew point sensors) is truly amazing.
 
 Have a look here: http://www.oregonscientificstore.com/
 
 So get some OS WRMR86 gear, an Arduino and get into it :-)
 
 NB: This program only works with Protocol 3.0 sensors.
 
 Reference Material:
 Thanks to these authors, they all helped in some way, especially the last one Brian!!!!
 http://wmrx00.sourceforge.net/Arduino/OregonScientific-RF-Protocols.pdf (1)
 http://jeelabs.net/projects/cafe/wiki/Decoding_the_Oregon_Scientific_V2_protocol (2)
 https://github.com/phardy/WeatherStation (3)
 http://lucsmall.com/2012/04/27/weather-station-hacking-part-1/ (4)
 http://lucsmall.com/2012/04/29/weather-station-hacking-part-2/ (5)
 http://lucsmall.com/2012/04/29/weather-station-hacking-part-2/ (6)
 http://www.mattlary.com/2012/06/23/weather-station-project/(7)
 https://github.com/lucsmall/WH2-Weather-Sensor-Library-for-Arduino (8)
 http://www.lostbyte.com/Arduino-OSV3/ (9) brian@lostbyte.com
 
 Most of all thanks to Oregon Scientific, who have produced an affordable, high quality product.
 I can now have my LCD Home Base in the kitchen to enjoy, with the Arduino in the garage also
 capturing data for WWW Weather pages. WRMR86 Lovely!!!!  http://www.oregonscientific.com
 Very Highly recommended equipment. Rob Ward
 */

#include <Wire.h>
#include "DHT.h"
#include "Adafruit_BMP085.h"

#define DHTTYPE DHT22       // DHT 22  (AM2302)
#define DHTPIN 2            // what pin we're connected to

//Adafruit_BMP085 bmp;
//DHT dht(DHTPIN, DHTTYPE);


//Interface Definitions
int     RxPin      = 8;     //The number of signal from the Rx
int     ledPin     = 13;    //The number of the onboard LED pin

// Variables for Manchester Receiver Logic:
word    sDelay     = 250;    //Small Delay about 1/4 of bit duration  try like 250 to 500
word    lDelay     = 500;    //Long Delay about 1/2 of bit duration  try like 500 to 1000, 1/4 + 1/2 = 3/4
byte    polarity   = 1;      //0 for lo->hi==1 or 1 for hi->lo==1 for Polarity, sets tempBit at start
byte    tempBit    = 1;      //Reflects the required transition polarity
byte    discards   = 0;      //how many leading "bits" need to be dumped, usually just a zero if anything eg discards=1
byte    discNos    = 0;      //Counter for the Discards 
boolean firstZero  = false;  //has it processed the first zero yet?  This a "sync" bit.
boolean noErrors   = true;   //flags if signal does not follow Manchester conventions
//variables for Header detection
byte    headerBits = 15;     //The number of ones expected to make a valid header
byte    headerHits = 0;      //Counts the number of "1"s to determine a header
//Variables for Byte storage
byte    dataByte   = 0;      //Accumulates the bit information
byte    nosBits    = 0;      //Counts to 8 bits within a dataByte
byte    maxBytes   = 9;      //Set the bytes collected after each header. NB if set too high, any end noise will cause an error
byte    nosBytes   = 0;      //Counter stays within 0 -> maxBytes
//Bank array for packet (at least one will be needed)
byte    manchester[12];      //Stores manchester pattern decoded on the fly
//Oregon bit pattern, causes nibble reflection, left to right, ABCDabcd becomes DCBAdcba
byte    oregon[]   = {
  16,32,64,128,1,2,4,8};
byte    csIndex    = 0;      //counter for nibbles needed for checksum
//Weather Variables
byte    quadrant   = 0;      //used to look up 16 positions around the compass rose
double  avWindspeed = 0.0;
double  gustWindspeed = 0.0; //now used for general anemometer readings rather than avWindspeed
float   rainTotal = 0.0;
float   rainRate  = 0.0;
double  temperature = 0.0;
int     humidity  = 0;
byte    byte0     = 0;
byte    byte1     = 0;
byte    byte2     = 0;
int     channel = 0;
double  intTemp;
double  intHumi;
double  intPres;
const char windDir[16][4] = {  
  "N  ", "NNE", "NE ", "ENE",  "E  ", "ESE", "SE ", "SSE",  "S  ", "SSW", "SW ", "WSW",  "W  ", "WNW", "NW ", "NNW"};

byte    scan      =0;                 //==7 means that all three sensors has been detected, so it reports all three with meaningful figures first up
byte  seconds     =0; //Counter to trigger the 60 seconds worth of data.

void setup() {
  Serial.begin(115200);
  pinMode(RxPin, INPUT);
  pinMode(ledPin, OUTPUT);

  /*  Enable these if attempting to debug the program or the circuit
   Serial.println("Debug Manchester Version 18");
   Serial.print("Using a delay of 1/4 bitWaveform ");
   Serial.print(sDelay,DEC);
   Serial.print(" uSecs 1/2 bitWaveform ");
   Serial.print(lDelay,DEC);
   Serial.println(" uSecs ");
   if (polarity){
   Serial.println("Negative Polarity hi->lo=1"); 
   }
   else{
   Serial.println("Positive Polarity lo->hi=1"); 
   }
   Serial.print(headerBits,DEC); 
   Serial.println(" bits expected for a valid header"); 
   if (discards){
   Serial.print(discards,DEC);
   Serial.println(" leading bits discarded from Packet"); 
   }
   else{
   Serial.println("All bits inside the Packet"); 
   }
   Serial.println("D 00 00001111 01 22223333 02 44445555 03 66667777 04 88889999 05 AAAABBBB 06 CCCCDDDD 07 EEEEFFFF 08 00001111 09 22223333 0A 44445555"); 
   */

  //Tutorial on using the BMP05 Press/Temp transducer https://www.sparkfun.com/tutorials/253
  //bmp.begin();    //start the barometer and temp packages

  // Initialize Timer1 for a 1 second interrupt
  // Thanks to http://www.engblaze.com/ for this section, see their Interrupt Tutorial
  cli();          // disable global interrupts
  TCCR1A = 0;     // set entire TCCR1A register to 0
  TCCR1B = 0;     // same for TCCR1B
  // set compare match register to desired timer count:
  OCR1A = 15624;
  // turn on CTC mode:
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler:
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);
  // enable timer compare interrupt:
  TIMSK1 |= (1 << OCIE1A);
  // enable global interrupts:
  sei();
}

//Routine Driven by Interrupt, trap 1 second interrupts, and output every minute
ISR(TIMER1_COMPA_vect){
  seconds++;
  if (seconds == 60){//make 60 for each output
    seconds = 0;
    usbData();//comment this out to temporarily disable data every minute for debug
  }
} //end of interrupt routine

// Main routines, find header, then sync in with it, get a packet, and decode data in it, plus report any errors.
void loop(){
  tempBit=polarity^1;
  noErrors=true;
  firstZero=false;
  headerHits=0;
  nosBits=0;
  maxBytes=15;//too big for any known OS signal
  nosBytes=0;
  discNos = discards;
  manchester[0]=0;
  digitalWrite(ledPin,0); 
  while (noErrors && (nosBytes<maxBytes)){
    while(digitalRead(RxPin)!=tempBit){
    }
    delayMicroseconds(sDelay);
    if (digitalRead(RxPin)!=tempBit){
      noErrors=false;
    }
    else{
      byte bitState = tempBit ^ polarity;
      delayMicroseconds(lDelay);
      if(digitalRead(RxPin)==tempBit){ 
        tempBit = tempBit^1;
      }
      if(bitState==1){ 
        if(!firstZero){
          headerHits++;
          if (headerHits==headerBits){
            //digitalWrite(ledPin,1);
          }
        }
        else{
          add(bitState);
        }
      }
      else{
        if(headerHits<headerBits){
          noErrors=false;
        }
        else{
          if ((!firstZero)&&(headerHits>=headerBits)){
            firstZero=true;
            //digitalWrite(ledPin,1); 

          }
          add(bitState);
        }
      }
    }
  }
  digitalWrite(ledPin,0);
}

void add(byte bitData){
  if (discNos>0){ 
    discNos--;//discard bits before real data
  }
  else{
    if (bitData){
      //if it is a '1' OR it in, others leave at a '0'
      manchester[nosBytes]|=oregon[nosBits];
    }
    //Oregon Scientific sensors have specific packet lengths
    //Maximum bytes for each sensor must set once the sensor has been detected.
    if(manchester[0]==0xA2){
      maxBytes=11;//rain
      csIndex =19;
    }
    if(manchester[0]==0xA1){
      maxBytes=10;//wind
      csIndex=18;
    }
    if(manchester[0]==0xAF){
      maxBytes=9;//temp
      csIndex=16;
    }
    if(manchester[0]==0xA3){
      maxBytes=10;//experimental, 9 data bytes and 1 byte CS
      csIndex =18;//CS byte begins 18 nibble
    }
    nosBits++;
    //Pack the bits into 8bit bytes
    if (nosBits==8){
      nosBits=0;
      nosBytes++;
      manchester[nosBytes]=0; //next byte to 0 to accumulate data
    }
    //Check the bytes for a valid packet once maxBytes received
    if(nosBytes==maxBytes){
      //hexBinDump();
      digitalWrite(ledPin,1);
      //Check Checksum first
      if (ValidCS(csIndex)){
        //Process the byte array into Human readable numbers
        analyseData();
      }
      noErrors=false;//make it begin again from the start
      //Throw in our own Arduino sensors as well (normally in the OS Base Station).
      //intHumi=(double)dht.readHumidity();        //DHT22 readings %Humidity
      //intTemp=(double)bmp.readTemperature();     //internal temperature
      //intPres=(double)bmp.readPressure()/100.0;  //Pa reduced to mBar 
    }
  }
}

//Useful to invoke to debug the byte Array
void hexBinDump(){
  //Serial.println("T A3 10100011 07 00000111 02 00000010 AA 10101010 F0 11110000 06 00000110 FF 11111111 07 00000111 33 00110011 60 01100000"); 
  Serial.print("D ");
  for( int i=0; i < maxBytes; i++){ 
    byte mask = B10000000;
    if (manchester[i]<16){
      Serial.print("0");
    }
    Serial.print(manchester[i],HEX);
    Serial.print(" ");
    for (int k=0; k<8; k++){
      if (manchester[i] & mask){
        Serial.print("1");
      }
      else{
        Serial.print("0");
      }
      mask = mask >> 1;
    }
    Serial.print(" ");
  }
  Serial.println();
}

//Support Routines for Nybbles and CheckSum

// http://www.lostbyte.com/Arduino-OSV3/ (9) brian@lostbyte.com
// Directly lifted, then modified from Brian's work. Now nybble's bits are pre-processed into standard order, ie MSNybble + LSNybble
// CS = the sum of nybbles, 1 to (CSpos-1), then compared to CSpos nybble (LSNybble) and CSpos+1 nybble (MSNybble);
// This sums the nybbles in the packet and creates a 1 byte number, and this is compared to the two nybbles beginning at CSpos
// Note that Temp 9 bytes and anemometer 10 bytes, but rainfall uses 11 bytes per packet. (NB Rainfall CS spans a byte boundary)
bool ValidCS(int CSPos){
  boolean ok = false;
  byte cs = 0;
  for (int x=1; x<CSPos; x++){
    byte test=nyb(x);
    cs +=test;
  }
  //do it by nybbles as some CS's cross the byte boundaries eg rainfall
  byte check1 = nyb(CSPos);
  byte check2 = nyb(CSPos+1);
  byte check = (check2<<4)+check1;
  /*
  if (manchester[0]==0xA2){
   Serial.print(check1,HEX);  //dump out the LSNybble Checksum
   Serial.print("(LSB), ");
   Serial.print(check2,HEX);  //dump out the MSNybble Checksum
   Serial.print("(MSB), ");
   Serial.print(check,HEX);   //dump out the Rx'ed predicted byte Checksum
   Serial.print("(combined), calculated = ");
   Serial.println(cs,HEX);    //dump out the calculated byte Checksum
   //Serial.print("   ");     //Space it out for the next printout 
   }
   */
  if (cs == check){
    ok = true;
  }
  return ok;
}
// Get a nybble from manchester bytes, short name so equations elsewhere are neater :-)
// Enables the byte array to be indexed as an array of nybbles
byte nyb(int nybble){
  int bite = nybble / 2;       //DIV 2, find the byte
  int nybb  = nybble % 2;      //MOD 2  0=MSB 1=LSB
  byte b = manchester[bite];
  if (nybb == 0){
    b = (byte)((byte)(b) >> 4);
  }
  else{
    b = (byte)((byte)(b) & (byte)(0xf));
  }       
  return b;
}

//enable debug dumps if formatted data required
void analyseData(){
  if (manchester[0]==0xaf){    //detected the Thermometer and Hygrometer (53seconds)
    scan = scan | 1;
    thermom();
    //dumpThermom();
  }
  if (manchester[0]==0xa1){    //detected the Anemometer and Wind Direction (14seconds)
    scan = scan | 2;
    anemom();
    //dumpAnemom();
  }
  if (manchester[0]==0xa2){    //detected the Rain Gauge (47seconds)
    scan = scan | 4;
    rain();
    //dumpRain();
  }
  if (manchester[0]==0xa3){    //detected an original Sensor designed by Us!!!
    //scan = scan | 8;
    //totally experimental
    totExp();
    //dumpExp();
    //This code is not used by the three sensors in the WMR86 product. It may clash with other
    //other OS Sensors and a different value chosen in the Tx and then Rx
  }

}

//Calculation Routines

/*   PCR800 Rain Gauge  Sample Data:
 //  0        1        2        3        4        5        6        7        8        9        A
 //  A2       91       40       50       93       39       33       31       10       08       02
 //  0   1    2   3    4   5    6   7    8   9    A   B    C   D    E   F    0   1    2   3    4   5  
 //  10100010 10010001 01000000 01010000 10010011 00111001 00110011 00110001 00010000 00001000 00000010
 //  -------- -------  bbbb---  RRRRRRRR 88889999 AAAABBBB CCCCDDDD EEEEFFFF 00001111 2222CCCC cccc
 
 // byte(0)_byte(1) = Sensor ID?????
 // bbbb = Battery indicator??? (7)  My investigations on the anemometer would disagree here.  
 // After exhaustive low battery tests these bbbb bits did not change
 // RRRRRRRR = Rolling Code Byte
 // 222211110000.FFFFEEEEDDDD = Total Rain Fall (inches)
 // CCCCBBBB.AAAA99998888 = Current Rain Rate (inches per hour)
 // ccccCCCC = 1 byte Checksum cf. sum of nybbles
 // Message length is 20 nybbles so working in inches
 Three tips caused the following
 1 tip=0.04 inches or 1.1mm (observed off the LCD)
 My experiment
 Personally I don't like this value. I think mult by 30 (I tried a few, 42, then 25) and divide by 1000 IS closer to return mm directly.
 0.127mm per tip??? It looks close to the above. Again can't vouch 100.000% for this, any rigorous assistance would be appreciated.
 */
void rain(){
  rainTotal = float(((nyb(18)*100000)+(nyb(17)*10000)+(nyb(16)*1000)+(nyb(15)*100)+(nyb(14)*10)+nyb(13))*30/1000.0);
  //Serial.println((nyb(18)*100000)+(nyb(17)*10000)+(nyb(16)*1000)+(nyb(15)*100)+(nyb(14)*10)+nyb(13),DEC);
  rainRate = float(((nyb(8)*10000)+(nyb(9)*1000)+(nyb(10)*100)+(nyb(11)*10)+nyb(12))*30/1000.0);
  //Serial.println((nyb(8)*10000)+(nyb(9)*1000)+(nyb(10)*100)+(nyb(11)*10)+nyb(12),DEC);
}
void dumpRain(){
  Serial.print("Total Rain ");
  Serial.print(rainTotal);
  Serial.print(" mm, ");
  Serial.print("Rain Rate ");
  Serial.print(rainRate);
  Serial.println(" mm/hr ");
}

// WGR800 Wind speed sensor
// Sample Data:
// 0        1        2        3        4        5        6        7        8        9
// A1       98       40       8E       00       0C       70       04       00       34
// 0   1    2   3    4   5    6   7    8   9    A   B    C   D    E   F    0   1    2   3
// 10100001 10011000 01000000 10001110 00000000 00001100 01110000 00000100 00000000 00110100
// -------- -------- bbbb---- NRRRRRRR xxxx9999 xxxxxxxx CCCCDDDD xxxxFFFF 0000---- CCCCcccc
// Av Speed 0.4000000000m/s Gusts 0.7000000000m/s  Direction: N  

// byte(0)_byte(1) = Sensor ID?????
// bbbb = Battery indicator??? (7)  My investigations would disagree here.  After exhaustive low battery tests these bits did not change
// NRRRRRRR = Rolling Code Byte, the N bit is set to 1 for 64 cycles to indicate it is reset or new to the Rx box
// 9999 = Direction
// DDDD.CCCC = Gust Speed (m per sec)
// 0000.FFFF = Avg Speed(m per sec)
// multiply by 3600/1000 for km/hr
// ccccCCCC = 1 byte checksum cf. sum of nybbles
// packet length is 20 nybbles

void anemom(){
  //D A1 98 40 8E 08 0C 60 04 00 A4
  avWindspeed = ((nyb(16)*10)+ nyb(15))*3.6/10;
  double gust =((nyb(13)*10)+nyb(12))*3.6/10;
  // after every minute, reset gustWindspeed to avWindspeed and then take the highest gust after that (approx4 readings a minute)
  if (gust>gustWindspeed){
    gustWindspeed = gust;
  }
  quadrant = nyb(9) & 0xF;
}
void dumpAnemom(){
  Serial.print("Av Speed ");
  Serial.print(avWindspeed);
  Serial.print(" km/hr, Gusts ");
  Serial.print(gustWindspeed);
  Serial.print(" km/hr, Direction: ");
  Serial.print(quadrant);
  Serial.print(" -> ");
  Serial.println(windDir[quadrant]);
}

// THGN800 Temperature and Humidity Sensor
// 0        1        2        3        4        5        6        7        8        9          Bytes
// 0   1    2   3    4   5    6   7    8   9    A   B    C   D    E   F    0   1    2   3      nybbles
// 01011111 00010100 01000001 01000000 10001100 10000000 00001100 10100000 10110100 01111001   Bits
// -------- -------- bbbbcccc RRRRRRRR 88889999 AAAABBBB SSSSDDDD EEEE---- CCCCcccc --------   Explanation
// byte(0)_byte(1) = Sensor ID?????
// bbbb = Battery indicator??? (7), My investigations on the anemometer would disagree here.  After exhaustive low battery tests these bits did not change
// RRRRRRRR = Rolling code byte
// nybble(5) is channel selector c (Switch on the sensor to allocate it a number)
// BBBBAAAA.99998888 Temperature in BCD
// SSSS sign for negative (- is !=0)
// EEEEDDDD Humidity in BCD
// ccccCCCC 1 byte checksum cf. sum of nybbles
// Packet length is 18 nybbles and indeterminate after that
// H 00 01 02 03 04 05 06 07 08 09    Byte Sequence
// D AF 82 41 CB 89 42 00 48 85 55    Real example
// Temperature 24.9799995422 degC Humidity 40.0000000000 % rel
void thermom(){
  channel = (nyb(5));
  if (channel == 1){
      temperature = (double)((nyb(11)*100)+(nyb(10)*10)+nyb(9))/10; //accuracy to 0.01 degree seems unlikely
  if(nyb(12)==1){//  Trigger a negative temperature
    temperature = -1.0*temperature;
      }
  humidity = (nyb(14)*10)+nyb(13);
} else if (channel > 1);{}  
}
void dumpThermom(){
         
  Serial.print("Temperature ");
  Serial.print(temperature);
  Serial.print(" C, Humidity ");
  Serial.print(humidity);
  Serial.print("%,");
  Serial.print(" Channel ");
  Serial.println(channel);
  
}
//The novel added extra sensors, extracting and applying any necessary conversions
void totExp(){
  byte0 = manchester[2]; //*190/255; //EG To apply a Solar Cell to the input
  byte1 = manchester[3];
  byte2 = manchester[4];
}

void dumpExp(){
  Serial.print("Solar Percentage =");
  Serial.println(byte0,DEC);
  Serial.print("Lightning Strikes =");
  Serial.println(byte1,DEC);
  Serial.print("Earthquakes =");
  Serial.println(byte2,DEC);
  Serial.print("Incrementing ");   //When testing the Tx, these were made to change every transmission to check reception
  Serial.print(manchester[5],DEC);
  Serial.print(", Decrementing ");
  Serial.println(manchester[6],DEC);
}

// Formating routine for interface to host computer, output once a minute once all three sesnors have been detected
void usbData(){
  // Stn Id, Packet Type, Wind Quadrant, Wind Speed, Rain Tips, Ext temp, Int Temp, Int Pressure, Int Humidity
  if (scan == 7){                    //scan==7 means all readings now have a valid value, output on Serial is no OK 
    Serial.print(byte0);             //A reading from the original design for an Oregon sensor byte[2]
    Serial.print(",");
    Serial.print(byte1);             //A reading from the original design for an Oregon sensor byte[3]  
    Serial.print(",");
    //I have a suspicion they may use bad checksums or some such to detect when the Tx is getting low.  However that idea has a
    //weakness as any other Tx on 433 can scramble a packet, and each WMR86 has three Tx's all transmitting at different intervals
    //and will inevitably cause corrupted/bad packets when they overlap.
    Serial.print(quadrant);          //0-15 in 22.5 degrees steps clockwise
    Serial.print(",");
    Serial.print(gustWindspeed,1);   //Gust windspeed km/hr, not average windspeed (graphing over 10 samples gives Average)
    Serial.print(",");
    gustWindspeed=avWindspeed;       //reset gust to average, then take the larger next reading
    Serial.print(rainTotal,1);       //checked for calibration to mm, appears to be OK
    Serial.print(",");
    Serial.print(temperature,2);    // OS Temperature Centigrade
    Serial.print(",");
    Serial.print(humidity);       // OS Humidity rel %
    //Serial.print(",");
    //The following two sensors are to be added but for simplicity sake they are not included in this version.
    //Only the preset globals are reported here, ie zeroes!!!
    //Serial.print(intTemp,2);         //BMP085 temperature (used for compensation of Pressure reading) Centigrade
    //Serial.print(",");
    //Serial.print(intPres,2);         //BMP085 pressure reading milli-bars
    //Serial.print(",");
    //Serial.print(intHumi);           //Digital DHT22 seems better than the OS in Temp/Hum sensor % relative
    //Serial.println();
  }
}


