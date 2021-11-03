#include <msp430F5529.h>
#include <LiquidCrystal.h> // hope it works

using namespace std;

// NOTE: This version of the project doesn't rely on external stimulus, and instead roughly approximates it for testing purposes when transmitter is unavailable.

LiquidCrystal lcd(P1_6, P6_6, P3_2, P2_7, P4_2, P4_1);

long globFalseTimer = 10; // The time it takes to send 1 bit; approximated to 10ms for test
// This value is initialized at 15 milliseconds per bit over 16 bits, but is dynamically changed depending on initial setup phase, as can be seen in StartFlagTRUE functionality
unsigned long globDelayTracker = 0;
int globStorage = 0;
int globInputVal = 2;

int globCounterTemp = 0;
int globMidCount = 0;
int globMidStore = 0;

void setup()
{
  WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer
  P1DIR = P1DIR & ~BIT3; // Set P1.3 as input -> This is the input from the antenna

  sleep(1);
  
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  lcd.setCursor(0, 1);
  lcd.print("Init-2...");

  bool StartFlag = 1;
  int Counter = 0;

  while (StartFlag) { // StartFlagTEST functionality
    Counter++;
    int InputVal = (0x1 & Counter);
    globStorage = globStorage + InputVal; //
    globStorage = globStorage & 0xF; // Remove Excess
    if (globStorage == 0b1010) {
      StartFlag = 0;
    }
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("StartTEST-1 ");
  lcd.print(globStorage); // Should always display as 10
  lcd.setCursor(0, 1);
  lcd.print(globFalseTimer);
  delay(3000);
}


// For reference: Messages contained in storage in main loop contains the following in bitwise logic:
// 0b(1P-- ---- ---- --00)
// Where 1s and 0s are always as such (referred to as 'buffer' bits
// P is the polarity of the message
// and - represent bits of the actual 12 bit message
// Each of these 3 sections is masked and extracted as needed


void loop() // Note to self: Energia really doesn't like it when you leave the main loop empty. Completely fails to upload properly.
{
  float voltage = 9999; // calculated value (initially high as placeholder)
  float voltageMid = 9999;

  // Polarity check
  int polarity;
  int polarityTrue;
  bool polarFlag = 0;

  int tempCount = 0;
  
  if (globInputVal > 1) { // Display short message indicating the end of the setup loop
    globStorage = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Begin-1");
    lcd.setCursor(0, 1);
    lcd.print(micros()); // microseconds since init; no real reason, just an arbitrary test of the LCD
    delay(5000);
    lcd.clear();
//    lcd.setCursor(0,0);
//    lcd.print(voltage);
//    lcd.print(" Volts");
//    lcd.setCursor(0,1);
//    lcd.print(globDelayTracker);
//    lcd.print(" ");
//    lcd.print(globFalseTimer);
  }

 // lcd.setCursor(14,0);
 // lcd.print("x");
  while (millis() - globDelayTracker < globFalseTimer) { } // Hold arbitrarily until 1 bitlength has passed since the last new value was obtained.
 // lcd.setCursor(14,0);
 // lcd.print(" ");
  
// TEST generation of InputVal begins here:
  if(!globMidCount)
    globInputVal = 1;
  else if (globMidCount == 1 ){
    globMidStore = 0;
    for(int i = 0;i<12;i++){
      globMidStore = (globMidStore + int(random(2))) << 1;
    }
    globMidStore = globMidStore >> 1;
    globInputVal = 0x0001 & globMidStore;
  }
  else if (globMidCount < 14){
    globInputVal = (0x800 >> ((globMidCount - 2)) & globMidStore) >> (13 - globMidCount);
  }
  else if (globMidCount < 16){
    globInputVal = 0;
  }
  else{
    globMidCount = 0;
    globInputVal = 1;
  }
  globMidCount++;

  // I should explain this, or nobody will understand it afterwards, myself included
  // This generates an artificial, random 12 bit input signal with 4 appropriate buffer and polarity bits.
  // bit 1 is a 1, bits 15 and 16 are 0
  // in bit 2, the actual random message is generated, and its polarity is sampled
  // for bits 3-14 (the 12 bit message), the appropriate bit of the previously generated random message is sampled and sent on
    // Note: past the first loop, globMidCount is never 0- the default case (final else) replaces its functionality.

  // This can be further modified to occasionally give a false buffer or polarity bit, if necessary, for err detection testing.

  
  globDelayTracker = millis(); // Grab time value was recorded.

  globStorage = globStorage + globInputVal; // Insert new value into storage
  
  char buffer2[16];
  if ((globStorage & 0x8003) == 0x8000) { // Extract buffer bits and compare to expected val; else skip
 //   lcd.setCursor(14,1);
 //   lcd.print("a");
    polarity = (globStorage & 0x4000) >> 14; // Extract the polarity bit from the storage.
    polarityTrue = globStorage & 0x0004; // Calculate the ACTUAL polarity of the message. Compare the two.
    if (polarity == polarityTrue) {
 //     lcd.print("b");
      polarFlag = 1;
      itoa(((globStorage&0x3FFF)>>2),buffer2,2);
      voltage = (globStorage & 0x3FFF) >> 2;
      voltage = (voltage - 3.1096) / 1236; // Extract message & convert to voltage
      voltageMid = (globMidStore - 3.1096) / 1236; // TEST: show actual value of the random generation
    }
  }
  
  globStorage = (globStorage << 1) & 0xFFFF; // Make room for new data, handle overflow
  
  if (polarFlag) { // Convert the storage to an actual voltage value when necessary.
    lcd.clear();
//    lcd.print("c");
    polarFlag = 0;
    lcd.setCursor(0,0);
    lcd.print(voltage);
    lcd.print(" Volts");
//    lcd.print(buffer2); // line2 - actual result
//    lcd.print(" r");
    lcd.setCursor(0,1);
    lcd.print(voltageMid);
    lcd.print(" TESTVolts");
    char buffer[16];
    itoa(globMidStore,buffer,2);
//    lcd.print(buffer); // line1 - input
//    lcd.print(" t");
  }

}
