#include <msp430F5529.h>
#include <LiquidCrystal.h> // hope it works

using namespace std;

LiquidCrystal lcd(P3_3, P6_6, P3_2, P2_7, P4_2, P4_1);

unsigned long globFalseTimer = 15; // The time it takes to send 1 bit. Init value 15 as default/fallthrough.
// This value is initialized at 15 milliseconds per bit over 16 bits, but is dynamically changed depending on initial setup phase, as can be seen in StartFlagTRUE functionality
unsigned long globDelayTracker = 0;
unsigned long globStorage = 0;
int globInputVal = 2;

unsigned long temp = 0;
int DebugCounter = 0;

int globCounterTemp = 0;
float prevVolt[4] = {};

void setup()
{
  WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer
  P1DIR |= BIT3; ; // Set P1.3 as input -> This is the input from the antenna

  sleep(1);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  lcd.setCursor(0, 1);
  lcd.print("Init-2...");
  delay(3000);
  lcd.clear();

  bool StartFlag = 1;
  int Counter = 0;

  //StartFlagTRUE functionality
  int PrevVal = 0;
  unsigned long FalseTimerStorage[4] = {};
  int StartCounter = 0;

  while (StartFlag) {
    int P_TEMP = P1IN;
    int InputVal = (P_TEMP & BIT2) >> 2; // Take value from pin

    // super simple edge detection, along with recording the timing of the edges to sync up with received data
    if (InputVal != PrevVal) {
      FalseTimerStorage[StartCounter] = millis();
      globStorage = globStorage + InputVal; // add the value to the storage
      PrevVal = InputVal;
      globStorage = globStorage & 0x0F; // remove excess
      if (++StartCounter > 3) {
        StartCounter = 0;
      }
      if (globStorage == 0b1010) {
        StartFlag = 0;
        globFalseTimer = ((FalseTimerStorage[3] - FalseTimerStorage[2]) + (FalseTimerStorage[2] - FalseTimerStorage[1]) + (FalseTimerStorage[1] - FalseTimerStorage[0])); // Av. of time distance between bits
      }
      else {
        globStorage = globStorage << 1; // move the storage over to make room for more val
      }
    }

  }
  globFalseTimer = globFalseTimer / 3;
  lcd.clear();
  lcd.print(globFalseTimer, 16);
  delay(1000); // Reset here if globFalseTimer is incorrect

}


// For reference: Messages contained in storage in main loop contains the following in bitwise logic:
// 0b(P--- ---- ---- -001)
// Where 1s and 0s are always as such (referred to as 'buffer' bits in code
// P is the polarity of the message
// and - represent bits of the actual 12 bit message (0x7FF8)
// Each of these 3 sections is masked and extracted as needed


void loop()
{
  float voltage = 9999; // calculated value (initially high as placeholder)

  // Polarity check
  int polarity;
  int polarityTrue;
  bool polarFlag = 0;

  while ((millis() - globDelayTracker) < globFalseTimer) { } // Hold arbitrarily until 1 bitlength has passed since the last new value was obtained.
  int P_TEMP = P1IN;
  globInputVal = (P_TEMP & BIT2) >> 2; // Take value from pin
  globDelayTracker = millis(); // Grab time value was recorded.

  globStorage = globStorage + globInputVal; // Insert new value into storage
  
  if (globStorage & 0x0001) {
    lcd.setCursor(12, 0);
    lcd.print("a");
    if ((globStorage & 0x6) == 0) {
      lcd.print("b");
      polarity = (globStorage & 0x8000) >> 15; // Extract the polarity bit from the storage.
      polarityTrue = (globStorage & 0x0008) >> 3; // Calculate the ACTUAL polarity of the message. Compare the two.
      if (polarity == polarityTrue) {
        lcd.print("c");
        polarFlag = 1;
        voltage = (globStorage & 0x7FFF) >> 3;
        voltage = (voltage + 2.0654) / 1253.8; // Extract message & convert to voltage

        // Calculate standard deviation of previous 4 + current voltage values
        float mean = (prevVolt[0]+prevVolt[1]+prevVolt[2]+prevVolt[3]+voltage)/5;
        float stdDev = sqrt((sq(prevVolt[0]-mean)+sq(prevVolt[1]-mean)+sq(prevVolt[2]-mean)+sq(prevVolt[3]-mean)+sq(voltage-mean))/5);

        // cycle out obsolete data
        for (int i = 0;i<3;i++){
          prevVolt[i] = prevVolt[i+1];
        }

        // If voltage == 0, it's an incorrect calculation, ignore it. If it is not, save it to prevVolt array.
        if(voltage != 0)
          prevVolt[3] = voltage;
        
        // If current volt is too distant from the mean, display prev. voltage instead of current.
        // ALSO, if voltage reads as 0, display prev. In theory this should also be outside std dev range, but better to set up just in case.
        // ALSO, if standard dev. is 0, ignore it
        if((stdDev == 0) && (voltage)){
          prevVolt[3] = voltage;
        }
        else if((abs(voltage-mean)>(2*stdDev)) || (voltage == 0))
          lcd.setCursor(11,1);
          lcd.print(voltage,16); // Debug- let us know what the actual value is.
          voltage = prevVolt[2];
      }
    }
  }
  if (polarFlag) {
    polarFlag = 0;
    lcd.setCursor(0, 0);
    lcd.print(voltage);
    lcd.print(" Volts");
    lcd.setCursor(0, 1);
    lcd.print(globCounterTemp++);
    if(globCounterTemp > 3){
      globCounterTemp = 0;
    }
  }
  globStorage = (globStorage << 1) & 0xFFFF; // Make room for new data, handle overflow
}