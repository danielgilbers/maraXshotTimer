/* Copyright (C) 2024 Daniel Gilbers

*/

//Includes
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include "bitmaps.h"

//Defines
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define BUFFER_SIZE 32

#define D5 (5)              // D5 is Rx Pin
#define D6 (6)              // D6 is Tx Pin
#define INVERSE_LOGIC true  // Use inverse logic for MaraX V2

//Internals
unsigned long timerStartMillis = 0;
unsigned long timerStopMillis = 0;
unsigned long timerPumpDelay = 0;
int timerCount = 0;
char formattedTimer[3];
bool timerStarted = false;

unsigned long previousMillis = 0;
const long interval = 1000;

unsigned long serialTimeout = 0;
char buffer[BUFFER_SIZE];
int bufferIndex = 0;
bool isMaraOff = false;
bool HeatDisplayToggle = false;
int tt = 1;

//Mara Data
char* maraData[7];

//Instances
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SoftwareSerial mySerial(D5, D6, INVERSE_LOGIC);  // Rx, Tx, Inverse_Logic

void setup() {
  /*
  // Mock Data
  maraData[0] = "C1.12";  // Coffee Mode (C) or SteamMode (V) & Software Version // "+" in case of Mara X V2 Steam Mode
  maraData[1] = "116";    // current steam temperature (Celsisus)
  maraData[2] = "124";    // target steam temperature (Celsisus)
  maraData[3] = "093";    // current hx temperature (Celsisus)
  maraData[4] = "0840";   // countdown for 'boost-mode'
  maraData[5] = "1";      // heating element on or off
  maraData[6] = "1";      // pump on or off
*/
  // Setup Serial
  Serial.begin(9600);    // Serial Monitor
  mySerial.begin(9600);  // MaraX Serial Interface

  //Wire.setWireTimeout(3000, true);  //timeout value in uSec - SBWire uses 100 uSec, so 1000 should be OK

  // Setup Display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;  // Don't proceed, loop forever
  }

  pinMode(LED_BUILTIN, OUTPUT);
  delay(100);
}
// -----------------------------------------------------------
void getMaraData() {
  /*
    Example Data: C1.12,116,124,093,0840,1,05\n every ~400-500ms
    Length: 27
    [Pos] [Data] [Describtion]
    0)      C     Coffee Mode (C) or SteamMode (V) // "+" in case of Mara X V2 Steam Mode
    -       1.12  Software Version
    1)      116   current steam temperature (Celsisus)
    2)      124   target steam temperature (Celsisus)
    3)      093   current hx temperature (Celsisus)
    4)      0840  countdown for 'boost-mode'
    5)      1     heating element on or off
    6)      0     pump on or off
  */
  while (mySerial.available()) {    // true, as long there are chrs in the Rx Buffer
    isMaraOff = false;              // Mara is not off
    serialTimeout = millis();       // save current time
    char rcv = mySerial.read();     // read next chr
    if (rcv != '\n')                // test if not CR
      buffer[bufferIndex++] = rcv;  // add to buffer and increase counter
    else {                          // CR received = EOM
      bufferIndex = 0;              // set buffer index to 0
      char* rest = buffer;
      Serial.println(rest);  // print buffer on serial monitor
      char* ptr;  // = strtok_r(buffer, ",");  // Split String into Tokens with ',' as delimiter
      int idx = 0;
      //Serial.println("RAW:");
      while ((ptr = strtok_r(rest, ",", &rest))) {
        maraData[idx++] = ptr;
      }
    }
  }

  if (millis() - serialTimeout > 999) {  // are there 1000ms passed after last chr rexeived?
    isMaraOff = true;                    // Mara is off
    serialTimeout = millis();
    //mySerial.write(0x11);
  }
}

// -----------------------------------------------------------
void detectChanges() {
  if (maraData[6][0] == '0') {  // [6] == 0 is Flag for Pump OFF
    if (timerStarted) {            // Check if Timer started Flag is set
      if (timerStopMillis == 0) {  // Save current time
        timerStopMillis = millis();
      }
      if (millis() - timerStopMillis >= 500) {  // this will be executed 500ms after Pump has been switched off
        timerStarted = false;                   // Clear Timer was started Flag
        timerStopMillis = 0;                    // Clear Stop Timer
        Serial.println("Pump OFF");             // Message on serial Port
        tt = 1;

        timerPumpDelay = millis();                  // Save current time
        while (millis() - timerPumpDelay < 1000) {  // wait 1 second
          delay(200);
        }
      }
    }
  } else {
    if (!timerStarted) {            // Timer is not started
      timerStartMillis = millis();  // Save current time
      timerStarted = true;          // Set Timer was started Flag
      Serial.println("Pump ON");    // Message for Serial Monitor
    }
    timerStopMillis = 0;
  }
}
// -----------------------------------------------------------
void updateView() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  //On or Off
  if (isMaraOff) {  // Mara OFF Message
    display.setCursor(30, 6);
    display.setTextSize(2);
    display.print("MARA X");
    display.setCursor(30, 28);
    display.setTextSize(4);
    display.print("OFF");
  } else {  // Mara is NOT off

    //Timer
    if (timerStarted) {  // If timer was started
      // draw the timer on the right
      display.setTextSize(5);
      display.setCursor(68, 20);

      timerCount = (millis() - timerStartMillis) / 1000;  // timerCount is (Now - Timer StartedTime)  in seconds
      if (timerCount > 99) {
        display.print("99");  // Display Seconds on screen
      } else {
        sprintf(formattedTimer, "%02d", timerCount);  // Format output String
        display.print(formattedTimer);                // Display Seconds on screen
      }

      //CoffeCup
      display.drawBitmap(17, 14, coffeeCups30[tt], 30, 30, WHITE);
      Serial.println(tt);

      if (tt == 8) {  // Loop through coffee cup bitmaps
        tt = 1;
      } else {
        tt++;
      }

      //HX temperature [3]
      displayModule(3);
    } else {
      //Coffee temperature [3] and bitmap
      display.drawBitmap(17, 14, coffeeCups30[0], 30, 30, WHITE);
      displayModule(3);

      //Steam temperature [1] and bitmap
      display.drawBitmap(83, 14, steam30, 30, 30, WHITE);
      displayModule(1);

      //Draw Line
      display.drawLine(66, 14, 66, 64, WHITE);

      //Boiler
      if (maraData[5][0] == '1') {  // Heater Flag is set
        display.setCursor(13, 0);
        display.setTextSize(1);
        display.print("Heatup");  // Print Message on Screen

        display.drawCircle(3, 3, 3, WHITE);

        HeatDisplayToggle = !HeatDisplayToggle;
        if (HeatDisplayToggle) {
          display.fillCircle(3, 3, 2, WHITE);
        }
      } else {
        display.print("");  // Clear Heater Message
        display.fillCircle(3, 3, 3, BLACK);
      }

      //Draw machine mode
      if (maraData[0][0] == 'C') {  // [0] = Mode & Version number - Check first Character if "C"
        // Coffee mode
        display.drawBitmap(115, 0, coffeeCup12, 12, 12, WHITE);  // Draw Coffee Cup Icon upper right
      } else {
        // Steam mode
        display.drawBitmap(115, 0, steam12, 12, 12, WHITE);  // Draw Steam Icon upper right corner
      }
    }
  }
  display.display();
}
// -----------------------------------------------------------
void displayModule(unsigned int data) {
  int position = 9;  // left [9] and right [78] position
  if (data == 1) {
    position = 78;
  }
  if (atoi(maraData[data]) >= 100) {  // [3] = Mara Hx temperature ... taking care of 2 or 3 digit value display
    display.setCursor(position, 50);
  } else {
    display.setCursor(position + 10, 50);
  }
  display.setTextSize(2);
  display.print(atoi(maraData[data]));
  display.setTextSize(1);
  display.print((char)247);
  display.print("C");
}
// -----------------------------------------------------------
// Main Loop
void loop() {
  // refresh every interval
  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    getMaraData();
    detectChanges();
    updateView();
  }
}