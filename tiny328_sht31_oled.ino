#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
#define OLED_I2C_ADDRESS 0x3C
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define RF69_COMPAT 1
#include <JeeLib.h> // https://github.com/jcw/jeelib
#define myNodeID 30     // RF12 node ID in the range 1-30
#define network 210      // RF12 Network group
#define freq RF12_433MHZ // Frequency of RFM12B module

#define USE_ACK           // Enable ACKs, comment out to disable
#define RETRY_PERIOD 5    // How soon to retry (in seconds) if ACK didn't come in
#define RETRY_LIMIT 5     // Maximum number of times to retry
#define ACK_TIME 10       // Number of milliseconds to wait for an ack

ISR(WDT_vect) { Sleepy::watchdogEvent(); } // interrupt handler for JeeLabs Sleepy power saving

#include <Wire.h>
#include "SHTSensor.h"
SHTSensor sht; 
float temp_c;
float humidity;
char str_temp[6];
char str_humid[6];

//########################################################################################################################
//Data Structure to be sent
//########################################################################################################################

 typedef struct {
      int temp;  // counter
      int humidity;  // Supply voltage
 } Payload;

 Payload tinytx; 

// Wait a few milliseconds for proper ACK
 #ifdef USE_ACK
  static byte waitForAck() {
   MilliTimer ackTimer;
   while (!ackTimer.poll(ACK_TIME)) {
     if (rf12_recvDone() && rf12_crc == 0 &&
        rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | myNodeID))
        return 1;
     }
   return 0;
  }
 #endif

//--------------------------------------------------------------------------------------------------
// Send payload data via RF
//-------------------------------------------------------------------------------------------------
 static void rfwrite(){
  #ifdef USE_ACK
   for (byte i = 0; i <= RETRY_LIMIT; ++i) {  // tx and wait for ack up to RETRY_LIMIT times
     rf12_sleep(-1);              // Wake up RF module
      while (!rf12_canSend())
      rf12_recvDone();
      rf12_sendStart(RF12_HDR_ACK, &tinytx, sizeof tinytx); 
      rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
      byte acked = waitForAck();  // Wait for ACK
      rf12_sleep(0);              // Put RF module to sleep
      if (acked) { return; }      // Return if ACK received
  
   Sleepy::loseSomeTime(RETRY_PERIOD * 1000);     // If no ack received wait and try again
   }
  #else
     rf12_sleep(-1);              // Wake up RF module
     while (!rf12_canSend())
     rf12_recvDone();
     rf12_sendStart(0, &tinytx, sizeof tinytx); 
     rf12_sendWait(2);           // Wait for RF to finish sending while in standby mode
     rf12_sleep(0);              // Put RF module to sleep
     return;
  #endif
 }

int getTemp() {
  int intTemp;
  float temp;
  temp = sht.getTemperature() * 100;
  intTemp = (int) temp;
  Serial.println(intTemp);
  return intTemp;
}

int getHumidity() {
  int intHumid;
  float humid;
  humid = sht.getHumidity() * 100;
  intHumid = (int) humid;
  Serial.println(intHumid);
  return intHumid;
}
void setup ()
{

  Serial.begin(115200);
  
  rf12_initialize(myNodeID,freq,network); // Initialize RFM12 with settings defined above 
  rf12_sleep(0);                          // Put the RFM12 to sleep

  PRR = bit(PRTIM1); // only keep timer 0 going  
  ADCSRA &= ~ bit(ADEN); bitSet(PRR, PRADC); // Disable the ADC to save power

// SHT 3x only
    Wire.begin();
    if (sht.init()) {
      Serial.print("SHTinit(): success\n");
  } else {
      Serial.print("SHTinit(): failed\n");
  }
  sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM); // only supported by SHT3x

  display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS); // initialize with the I2C addr 0x3c
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(10,20);
  display.print("Ready!");
  display.display();
}

void loop() {
  
  // counter++;

  tinytx.temp = getTemp(); // Get temperature reading and convert to integer, reversed at receiving end
    
  tinytx.humidity = getHumidity(); // Get supply voltage

  rfwrite(); // Send data via RF 

  Sleepy::loseSomeTime(10000); //JeeLabs power save function: enter low power mode for 10 seconds (valid range 16-65000 ms)
    
}
