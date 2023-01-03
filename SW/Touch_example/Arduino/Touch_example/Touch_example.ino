/* Touch_example 
 * 
 * Email:podpora@laskakit.cz
 * Web:laskarduino.cz
 */

#include <Arduino.h>
/*
* Chip used in board is FT5436
* Library used: https://github.com/DustinWatts/FT6236
* Just changed CHIPID and VENDID
* Library is included in the project so it does not need to be downloaded
 */
#include <FT6236.h>

FT6236 ts = FT6236();

void setup(void)
{
    Serial.begin(115200);
    if (!ts.begin(40)) //40 in this case represents the sensitivity. Try higer or lower for better response. 
    {
        Serial.println("Unable to start the capacitive touchscreen.");
    }
}

void loop(void)
{

    if (ts.touched())
    {
        // Retrieve a point
        TS_Point p = ts.getPoint();

        // Print coordinates to the serial output
        Serial.print("X Coordinate: ");
        Serial.println(p.x);
        Serial.print("Y Coordinate: ");
        Serial.println(p.y);
    }

    //Debouncing. To avoid returning the same touch multiple times you can play with this delay.
    delay(50);
}