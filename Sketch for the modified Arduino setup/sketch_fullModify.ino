#include <SimpleDHT.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// #include <SoftwareSerial.h>
#include <SD.h>
#include <RTClib.h>
// #include <MQ135.h>
//#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
// #include <NDIRZ16.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD address to 0x27 for a 16 chars and 2 line display
RTC_DS1307 rtc;
SimpleDHT11 dht11;

#define ONE_WIRE_BUS_1 2 //DS18B20 sensor 1 -- Inside Chamber -- Above Soil
#define ONE_WIRE_BUS_2 3 //DS18B20 sensor 2 -- Inside Chamber -- In Soil
#define ONE_WIRE_BUS_3 4 //DS18B20 sensor 3 -- Outside Chamber -- In Soil
// #define ONE_WIRE_BUS_4 5 //DS18B20 sensor 4 -- Outside Chamber -- Above Soil
OneWire oneWire_1(ONE_WIRE_BUS_1);
OneWire oneWire_2(ONE_WIRE_BUS_2);
OneWire oneWire_3(ONE_WIRE_BUS_3);
// OneWire oneWire_4(ONE_WIRE_BUS_4);
DallasTemperature sensor_1(&oneWire_1);
DallasTemperature sensor_2(&oneWire_2);
DallasTemperature sensor_3(&oneWire_3);
// DallasTemperature sensor_4(&oneWire_4);

float celcius_1 = 0, celcius_2 = 0, celcius_3 = 0;
// celcius_4 = 0; // for DS18B20
int pinDHT11 = 6; // DHT11
byte temperature = 0, humidity = 0; // for DHT11

float kPa[]= {0, 0, 0, 0}; // for MPX5100DP
float hPa[]= {0, 0, 0, 0}; // for MPX5100DP
const int powerpin[] = {0, 37, 39, 41}; // for MPX5100DP
const int tensiopin[] = {0, A11, A12, A13}; // for MPX5100DP
int rawADC_hPa[]= {0, 0, 0, 0}; // for MPX5100DP
float voltage_hPa[]= {0, 0, 0, 0}; // for MPX5100DP


float slope_MPX5100DP[] = {0, 1.094947308, 1.091102129, 1.083876913}; // for MPX5100DP
float intercept_MPX5100DP[] = {0, -5.301696878, -4.538585086, -3.34233489}; // for MPX5100DP

File myFile;

void setup() {
//Wire.begin(7);
Serial.begin(9600);
// mySerial.begin(9600);
// Serial.println("Wait 60 seconds for the NDIR sensor to starup");
// delay(60000);

sensor_1.begin();
sensor_2.begin();
sensor_3.begin();
// sensor_4.begin();
for( int tensioSet = 1; tensioSet < 4; tensioSet++) {
pinMode(powerpin[tensioSet], OUTPUT);
}

while (!Serial) {
// wait for serial port to connect. Needed for native USB port only
}
Serial.print("Initializing SD card...");
if (!SD.begin(10, 11, 12, 13)) {
  Serial.println("initialization failed!");
  return;
}
Serial.println("initialization done.");
rtc.begin();
Serial.println( "rtc.isrunning() " + rtc.isrunning());
//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
if (! rtc.isrunning()) {
// following line sets the RTC to the date & time this sketch was compiled
rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}
Serial.println( " ==> " + print_time(rtc.now()));

//lcd.begin(16, 2);
lcd.init();
lcd.setCursor(0, 0);
lcd.print("Waiting....");
}

void loop() {
//---------------------------------- DHT11 Temperature sensors-----------------------------------------//
byte data[40] = {0};
if (dht11.read(pinDHT11, &temperature, &humidity, data)) {
temperature = 0;
humidity = 0;
}

//---------------------------------- MPX5100DP Pressure sensors-----------------------------------------//
for( int tensioSet = 1; tensioSet < 4; tensioSet++) {
getHPA(tensioSet);
}

//---------------------------------- DS18B20 Temperature sensors-----------------------------------------//
sensor_1.requestTemperatures();
sensor_2.requestTemperatures();
sensor_3.requestTemperatures();
// sensor_4.requestTemperatures();
celcius_1 = sensor_1.getTempCByIndex(0);
celcius_2 = sensor_2.getTempCByIndex(0);
celcius_3 = sensor_3.getTempCByIndex(0);
// celcius_4 = sensor_4.getTempCByIndex(0);


//---------------------------------- Serial.print()-------------------------------------------//
monitorSerialPrintMaster();

//---------------------------------- Date & Time RTC-------------------------------------------//
DateTime now = rtc.now();
String file_name = print_day(now);
String current_time = print_time(now);

//---------------------------------- Data-Logging to SD Card-------------------------------------------//
String m_file_name = file_name;
m_file_name += "-M2.txt";
masterDataLog(m_file_name, current_time);
delay(1000);

//---------------------------------- LCD Display-------------------------------------//
goToLCDDisplay(print_day_time(now));
//goToLCDDisplay(hPa1, hPa2, hPa3, getCorrectedPPM, rawADC_A3, print_day_time(now), celcius_1);

delay(595550); // 1150*3 for MPX5100DP, 1000 for Master Log = 4450 // 10 minutes 1reading

}


/**
read Tensiometers (MPX5100DP)
*/

void getHPA(int tensioSet) {
digitalWrite(powerpin[tensioSet], HIGH); //power up sensor
delay(50); //warming up of MPX5100DP
rawADC_hPa[tensioSet] = readAnalogOutput(tensiopin[tensioSet]);
digitalWrite(powerpin[tensioSet], LOW); //power off
voltage_hPa[tensioSet] = ((float) rawADC_hPa[tensioSet] / 1024.0) * 5.0; // "5.0"V for ratiometric sensor & A0 range is "0 - 1023"
kPa[tensioSet] = (voltage_hPa[tensioSet] - 0.2) / 45 * 1000; // Sensitivity = V/P= 45 mV/kPa (data sheet)
kPa[tensioSet] = slope_MPX5100DP[tensioSet]*kPa[tensioSet] +intercept_MPX5100DP[tensioSet]; //y=mx+c 
hPa[tensioSet] = kPa[tensioSet] / 10;
}


/**
read analogSensorOutput for 10 values and calculate mean
*/
int readAnalogOutput(int anologPin) {
digitalWrite(anologPin, HIGH); //power up sensor MPX5100DP
delay(100); //warming up of
int rawADC = 0;
for (int j = 0; j <= 9; j++) { //take 10 readings and calculate mean value
rawADC = rawADC + analogRead(anologPin);
delay(100);
}
digitalWrite(anologPin, LOW);
rawADC = rawADC / 10; //mean
return rawADC;
}

/**
Data Logging for Master
*/
void masterDataLog(String m_file_name, String current_time) {
int n = m_file_name.length();
char m_char_array[n + 1];
strcpy(m_char_array, m_file_name.c_str());
if(!SD.exists(m_char_array)) {
addDataHeader(m_file_name); // Add data header in the new file created
}
addData(m_file_name, current_time); // Add data
}

/**
Data Logging for Master :: Add data header in the new file created
*/
void addDataHeader(String m_file_name) {
int n = m_file_name.length();
char m_char_array[n + 1];
strcpy(m_char_array, m_file_name.c_str());
myFile = SD.open(m_char_array, FILE_WRITE);
if (myFile) {
Serial.print("Writing Header to " + m_file_name + " ==> ");

myFile.println("Timestamp,hPa_1,hPa_2,hPa_3,T1_air_in,T2_soil_in,T3_soil_out,DHT_temp,DHT_humid");
// myFile.println("Timestamp,hPa_1,hPa_2,hPa_3,PPM_NDIR_UART,T1_air_in,T2_soil_in,T3_soil_out,T4_air_out,DHT_temp,DHT_humid");
myFile.close(); // close the file
Serial.print(" ==> done.");
} 
else {
  Serial.println("error opening " + m_file_name); // if the file didn't open,print an error
}
}

/**
Data Logging for Master :: Add data
*/
void addData(String m_file_name, String current_time) {
int n = m_file_name.length();
char m_char_array[n + 1];
strcpy(m_char_array, m_file_name.c_str());
myFile = SD.open(m_char_array, FILE_WRITE);
if (myFile) {
Serial.print("Writing to " + m_file_name + " ==> ");
myFile.print(current_time);
myFile.print(",");
myFile.print(hPa[1]);
myFile.print(",");
myFile.print(hPa[2]);
myFile.print(",");
myFile.print(hPa[3]);
// myFile.print(",");
// myFile.print(ppm_NDIR_UART);
// Check for loose connection of Temp sensors
// float t [] = {0, celcius_1, celcius_2, celcius_3, celcius_4};
float t [] = {0, celcius_1, celcius_2, celcius_3};
myFile.print(",");
myFile.print(ifTempLoose( String(t[1]) ));
myFile.print(",");
myFile.print(ifTempLoose( String(t[2]) ));
myFile.print(",");
myFile.print(ifTempLoose( String(t[3]) ));
myFile.print(",");
myFile.print(ifTempLoose( String(t[4]) ));
myFile.print(",");
myFile.print(ifDHTLoose( String(temperature) ));
myFile.print(",");
myFile.print(ifDHTLoose( String(humidity) ));

myFile.println(); // close the file:
myFile.close();
Serial.print(" ==> done.");
} else {
Serial.println("error opening " + m_file_name); // if the file didn't open,print an error
}
Serial.println( " ==> " + current_time + " <== Master");
}

/**
check for losse DS18B20 connections
*/
String ifTempLoose (String temp) {
if (temp == "-127.00" || temp == "85.00") {
return "0";
} else {
return temp;
}
}

/**
check for losse DHT11 connections
*/
String ifDHTLoose (String dht) {
if (dht == "132" || dht == "141" || dht == "135") {
return "0";
} else {
return dht;
}
}


/**
Disply on LCD
*/
void goToLCDDisplay(String day_time) 
//void goToLCDDisplay(float hPa1, float hPa2, float hPa3, float getCorrectedPPM, float rawADC_A3, String day_time, float celcius_1) 
{
celcius_1 =celcius_1*100;
float arrayOfFloatValues[] = {0, hPa[1], hPa[2], hPa[3], celcius_1};
int arrayOfIntValues[5];
for (int a =0 ; a < 5 ; a++) {
arrayOfIntValues[a] = convertingToInt(arrayOfFloatValues[a]);
}
lcd.clear();
lcd.setCursor(0, 0);
lcd.print(arrayOfIntValues[1]);
lcd.setCursor(3, 0);
lcd.print(";");
lcd.setCursor(4, 0);
lcd.print(arrayOfIntValues[2]);
lcd.setCursor(7, 0);
lcd.print(";");
lcd.setCursor(8, 0);
lcd.print(arrayOfIntValues[3]);
lcd.setCursor(11, 0);
lcd.print(";");
// lcd.setCursor(12, 0);
// lcd.print(ppm_NDIR_UART);

lcd.setCursor(0, 1);
 lcd.print(arrayOfIntValues[4]);
 lcd.setCursor(4, 1);
 lcd.print(";");
 lcd.setCursor(5, 1);
 lcd.print(day_time);
 }

/**
 Converts to Int to display on LCD
 */
 int convertingToInt(float value) {
 int x = (int) value;
 if (value >= (x+0.5) ) {
 x = x+1;
 }
 return x;
 }

 /**
 Serial.print for Master
 */
 void monitorSerialPrintMaster() {
 Serial.print("hPa[1]: ");
 Serial.print(hPa[1]);
 Serial.print("; hPa[2]: ");
 Serial.print(hPa[2]);
 Serial.print("; hPa[3]: ");
 Serial.print(hPa[3]);

//  Serial.print("; ppm_NDIR_UART: ");
//  Serial.print(ppm_NDIR_UART);

 Serial.print("; celcius_1 = ");
 Serial.print(celcius_1);
 Serial.print("; celcius_2 = ");
 Serial.print(celcius_2);
 Serial.print("; celcius_3 = ");
 Serial.print(celcius_3);
//  Serial.print("; celcius_4 = ");
//  Serial.print(celcius_4);

 Serial.print("; DHT T5: ");
 Serial.print(temperature);
 Serial.print("; DHT H: ");
 Serial.println(humidity);
 }

 /**
 Timestamp to store data
 */
 String print_time(DateTime timestamp) {
 char message[120];
 int Year = timestamp.year();
 int Month = timestamp.month();
 int Day = timestamp.day();
 int Hour = timestamp.hour();
 int Minute = timestamp.minute();
 int Second = timestamp.second();
 //sprintf(message, "%d-%02d-%02d %02d:%02d:%02d", Year, Month, Day, Hour, Minute, Second);
 sprintf(message, "%d-%02d-%02d %02d:%02d:%02d", Year, Month, Day, Hour, Minute, Second);
 return message;
 }

 /**
 Day and month to generate files names
 */
 String print_day(DateTime timestamp) {
 char message[120];
 int Month = timestamp.month();
 int Day = timestamp.day();
 sprintf(message, "%02d-%02d", Day, Month);
 return message;
 }

 /**
 Timestamp to disply on LCD
 */
 String print_day_time_year(DateTime timestamp) {
 char message[120];
 int Year = timestamp.year();
 int Month = timestamp.month();
 int Day = timestamp.day();
 int Hour = timestamp.hour();
 int Minute = timestamp.minute();
 int Second = timestamp.second();
 sprintf(message, "%02d-%02d-%d %02d:%02d", Day, Month, Year, Hour, Minute);
 return message;
 }

 /**
 Timestamp to disply on LCD
 */
 String print_day_time(DateTime timestamp) {
 char message[120];
 int Year = timestamp.year();
 int Month = timestamp.month();
 int Day = timestamp.day();
 int Hour = timestamp.hour();
 int Minute = timestamp.minute();
 int Second = timestamp.second();
 sprintf(message, "%02d-%02d %02d:%02d", Day, Month, Hour, Minute);
 return message;
 }
 /**
 Timestamp to disply on LCD
 */
 String print_hour(DateTime timestamp) {
 char message[120];
 int Year = timestamp.year();
 int Month = timestamp.month();
 int Day = timestamp.day();
 int Hour = timestamp.hour();
 int Minute = timestamp.minute();
 int Second = timestamp.second();
 sprintf(message, "%02d:%02d", Hour, Minute);
 return message;
 }

