#include <msp430F5529.h>
#include <LiquidCrystal.h> // hope it works

using namespace std;

LiquidCrystal lcd(P3_3, P6_6, P3_2, P2_7, P4_2, P4_1);

long globFalseTimer = 15; // The time it takes to send 1 bit. Init value 15 as default/fallthrough.
// This value is initialized at 15 milliseconds per bit over 16 bits, but is dynamically changed depending on initial setup phase, as can be seen in StartFlagTRUE functionality
long globDelayTracker = 0;
long globStorage = 0;
int globInputVal = 2;

long temp = 0;
int DebugCounter = 0;

int globCounterTemp = 0;
float prevVolt[4] = {};

void setup()
{
  WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer
  P1DIR |= BIT3; ; // Set P1.3 as input -> This is the input from the antenna

  sleep(2);

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  lcd.setCursor(0, 1);
  lcd.print("Init...");
  delay(1500);
  lcd.clear();

  bool StartFlag = 1;
  bool StartFlag2 = 1;
  int Counter = 0;

  int PrevVal = 0;
  long FalseTimerStorage[4] = {};
  int StartCounter = 0;

  while (StartFlag) {
    int P_TEMP = P1IN; // Store pin for stability
    int InputVal = (P_TEMP & BIT2) >> 2; // Take value from pin

    // Manual sync with transmitter. When sync is complete, button is pushed on transmitter side.
    // Super simple edge detection, as setup phase is a 1-0 repeating bitstream.
    if (StartFlag2){
      StartFlag2 = 0;
      PrevVal = InputVal;
    }
    else if ((InputVal != PrevVal) && (!StartFlag2)) {
      FalseTimerStorage[StartCounter] = millis();
      globStorage = globStorage + InputVal; // add the value to the storage
      PrevVal = InputVal;
      globStorage = globStorage & 0x0F; // remove excess
      StartCounter++;
      if (globStorage == 0b1010) {
        StartFlag = 0;
        globFalseTimer = (FalseTimerStorage[3] - FalseTimerStorage[2]);
        globFalseTimer = globFalseTimer + (FalseTimerStorage[2] - FalseTimerStorage[1]) ;
        globFalseTimer = globFalseTimer + (FalseTimerStorage[1] - FalseTimerStorage[0]);
        globFalseTimer = globFalseTimer / 3; // Av. of time between edges
      }
      else {
        globStorage = globStorage << 1; // move the storage over to make room for more val
      }
    }

  }
  // globFalseTimer should read as the exact value set in the transmitter. If not, a manual reset here is required, and indicates some level of false positive.
  lcd.clear();
  lcd.print(globStorage,16);
  lcd.setCursor(0,1);
  lcd.print(globFalseTimer, 16);
  delay(1000);

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
  
  // all lcd prints nested in this if statement are purely for debug purposes and can/should be removed later on.
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
        voltage = voltage / 1247.3; // Extract message & convert to voltage

        // Calculate standard deviation of previous 4 + current voltage values
        float mean = (prevVolt[0]+prevVolt[1]+prevVolt[2]+prevVolt[3]+voltage)/5;
        float stdDev = sqrt((sq(prevVolt[0]-mean)+sq(prevVolt[1]-mean)+sq(prevVolt[2]-mean)+sq(prevVolt[3]-mean)+sq(voltage-mean))/5);

        // cycle out obsolete data
        for (int i = 0;i<3;i++){
          prevVolt[i] = prevVolt[i+1];
        }

        // If voltage == 0, it's an incorrect calculation, ignore it. If it is not, save it to prevVolt array.
        if(voltage > 0.04)
          prevVolt[3] = voltage;
        
        // If current volt is too distant from the mean, display prev. voltage instead of current.
        // ALSO, if voltage reads as 0, display prev. In theory this should also be outside std dev range, but better to set up just in case.
        // ALSO, if standard dev. is 0, ignore it, just push the voltage through (as long as it's not 0)
        if((stdDev == 0) && (voltage > 0.04)){ }
        else if((fabs(voltage-mean)>(stdDev/2)) || (voltage < 0.04)){
          lcd.setCursor(5,1);
          lcd.print(stdDev,16);
          lcd.setCursor(10,1);
          lcd.print(" ");
          lcd.setCursor(11,1);
          lcd.print(voltage,16); // Debug- let us know what the actual value is.
          voltage = prevVolt[2];
        }
      }
    }
  }
  if (polarFlag) {
    polarFlag = 0;
    lcd.setCursor(0, 0);
    lcd.print(voltage);
    lcd.print(" Volts");
    lcd.setCursor(0, 1);
    if(voltage < 0.01){
      lcd.print("ERR");
    }
    if(globCounterTemp > 3){
      globCounterTemp = 0;
    }
  }
  globStorage = (globStorage << 1) & 0xFFFF; // Make room for new data, handle overflow
}
