/*
  Track your finger's locations on a capacitive touch panel (CTP)
  By: Owen Lyke
  SparkFun Electronics
  Date: October 30 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).
  Example1_BasicReadings
  To connect the sensor to an Arduino:
  This library supports the sensor using the I2C protocol
  On Qwiic enabled boards simply connnect the sensor with a Qwiic cable and it is set to go
  On non-qwiic boards you will need to connect 4 wires between the sensor and the host board
  (Arduino pin) = (Breakout pin)
      SCL       =       SCL
      SDA       =       SDA
      GND       =       GND
      3.3V      =       3.3V
*/
#include <Wire.h>
#include "SparkFun_TouchInput_Driver_FT5xx6.h"    // Click here to get the library: http://librarymanager/All#SparkFun_TouchInput_Driver_FT5xx6

#define SERIAL_PORT Serial      // Using this #define makes it easy to test this code on boards like the SAMD21 that natively use SerialUSB

FT5316 myCTP;                   // Here we declare an FT5316 object. It is a specific 'kind' of the FT5xx6 class that has a known + verified I2C address.

void setup() {
  SERIAL_PORT.begin(115200);                                              // Start talking on the chosen serial port
  SERIAL_PORT.println("Example1 - FT5316 CTP Driver Simple Sensing");     // Title 
  SERIAL_PORT.println();                                              

  SERIAL_PORT.print("Beginning the CTP. Status: ");                       // By default the begin() statement will start the CTP using the I2C port Wire. If you want to use another port see "Example2_ChoosingWirePort"
  statusDecoder( myCTP.begin() );                                         // Additionally many of the operations you can do with the myCTP object return a status type that can be checked for success/failure.
  SERIAL_PORT.println("\n");
}

void loop() {
  Wire.begin (21, 22);
  myCTP.begin(); // A precaution for hot-swapping (Doesn't seem to be necessary always but it should not hurt for this purpose)
  
  // Polling code:
  FT5xx6_TouchRecord_TypeDef touch;                                     // This is "TouchRecord" variable type. It contains a timestamp, the number of registered points, and the coordinates for the registered points
  myCTP.update();                                                       // 'update()' reads the CTP controller for a TouchRecord. If that touch record different than the last one then the 'newTouch' variable is set to 'true' and the lastTouch member is updated with the latest values
  if(myCTP.newTouch)                                                    // If there is a new touch....
  {
    touch = myCTP.read();                                               // The read() function returns the 'lastTouch' TouchRecord which is saved into the local variable 'touch'
    //printTouchRecord(touch);                                            // We use a custom function to print the touch info to the serial monitor
    printTouchRecord(myCTP.lastTouch);                                  // This line demonstrates another way that you can access the 'lastTouch' data
  }
}


void printTouchRecord(FT5xx6_TouchRecord_TypeDef precord)
{
  SERIAL_PORT.print("Touch record info: ");

  SERIAL_PORT.print("Timestamp: "); 
  SERIAL_PORT.print(precord.timestamp);
  SERIAL_PORT.print(" : ");
  
  SERIAL_PORT.print("Number of touches: "); 
  SERIAL_PORT.print(precord.numTouches);
  SERIAL_PORT.print(" : ");
  
  if(precord.numTouches > 0)
  {
    SERIAL_PORT.print("Touch "); 
    SERIAL_PORT.print(1); 
    SERIAL_PORT.print(", x: ");
    SERIAL_PORT.print(precord.t1x);
    SERIAL_PORT.print(", y: ");
    SERIAL_PORT.print(precord.t1y);
    SERIAL_PORT.print(" : ");
  }
  if(precord.numTouches > 1)
  {
    SERIAL_PORT.print("Touch "); 
    SERIAL_PORT.print(2); 
    SERIAL_PORT.print(", x: ");
    SERIAL_PORT.print(precord.t2x);
    SERIAL_PORT.print(", y: ");
    SERIAL_PORT.print(precord.t2y);
    SERIAL_PORT.print(" : ");
  }
  if(precord.numTouches > 2)
  {
    SERIAL_PORT.print("Touch "); 
    SERIAL_PORT.print(3); 
    SERIAL_PORT.print(", x: ");
    SERIAL_PORT.print(precord.t3x);
    SERIAL_PORT.print(", y: ");
    SERIAL_PORT.print(precord.t3y);
    SERIAL_PORT.print(" : ");
  }
  if(precord.numTouches > 3)
  {
    SERIAL_PORT.print("Touch "); 
    SERIAL_PORT.print(4); 
    SERIAL_PORT.print(", x: ");
    SERIAL_PORT.print(precord.t4x);
    SERIAL_PORT.print(", y: ");
    SERIAL_PORT.print(precord.t4y);
    SERIAL_PORT.print(" : ");
  }
  if(precord.numTouches > 4)
  {
    SERIAL_PORT.print("Touch "); 
    SERIAL_PORT.print(5); 
    SERIAL_PORT.print(", x: ");
    SERIAL_PORT.print(precord.t5x);
    SERIAL_PORT.print(", y: ");
    SERIAL_PORT.print(precord.t5y);
    SERIAL_PORT.print(" : ");
  }
  SERIAL_PORT.println();
  
}

void statusDecoder( FT5xx6_Status_TypeDef status)
{
  switch(status)
  {
    case FT5xx6_Status_Nominal :            SERIAL_PORT.print("Nominal");             break;
    case FT5xx6_Status_Error :              SERIAL_PORT.print("Error");               break;
    case FT5xx6_Status_Not_Enough_Core :    SERIAL_PORT.print("Not Enough Core");     break;
    default :                               SERIAL_PORT.print("Unknown status");      break;
  }
}
