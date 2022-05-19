#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// OLED SETTINGS
#define SCREEN_WIDTH  128 
#define SCREEN_HEIGHT  64 
#define OLED_RESET     -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// PB SETTINGS
#define PB 10

// WHEATSTONE BRIDGE SETTINGS
#define FAN 9
#define LED 8
#define TARGETTEMP 20 // CELSIUS
#define R1 30
#define R2 30
#define R3 30
#define REFVOL 5

// AC VOLTAGE SETTINGS





// OHMMETER SETTINGS
#define NUM_REF_RESISTORS 6
#define NUM_SELECT_PINS   3
#define MAX_ANALOG_VALUE  960
#define SWITCH_RESISTANCE 4.5
// Reference Resistor values      x0    x1   x2     x3     x4       x5  
float rRef[NUM_REF_RESISTORS] = {47.6, 99.3, 1002, 10040, 99300, 1017000};
const byte rSelPins[NUM_SELECT_PINS] = {5, 4, 3};
const byte enableMux = 2; // 1 = no connection, 0 = one of eight signals connected


void displayNames() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("Group Names:");
  display.setCursor(0, 20);
  display.println("Omar Mahmoud Shebl");
  display.setCursor(0, 30);
  display.println("Ziad Al Amir");
  display.setCursor(0, 40);
  display.println("Khaled Basha");
  display.display();   
  delay(10000);
}

bool checkButtonPress() {
  if (digitalRead(PB)) {
    return false;
  }
  return true;
}

void setup()
{

  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // BUTTON SETUP
  pinMode(PB, INPUT_PULLUP);
  

  // FAN & LED SETUP
  pinMode(LED, OUTPUT);
  pinMode(FAN, OUTPUT);

  // OHMMETER SETUP
  pinMode(enableMux, OUTPUT);
  digitalWrite(enableMux, HIGH);      // disable all switches
  
  for (int i = 0; i < NUM_SELECT_PINS; i++)
  {
    pinMode(rSelPins[i], OUTPUT);     // Mux select pins configured as outputs
    digitalWrite(rSelPins[i], HIGH);  // select the highest Rref
  }

   // DISPLAY NAMES FOR FIRST TIME
   displayNames();
}

char ScaleToMetricUnits(float *prVal, char fStr[]) {
  char unit;
  if (*prVal < 1000)
  {
    unit = ' ';
  }
  else if (*prVal >= 1000 && *prVal < 1000000)
  {
    *prVal /= 1000;
    unit = 'K';
  }
  else if (*prVal >= 1000000 && *prVal < 1000000000)
  {
    *prVal /= 1000000;
    unit = 'M';
  }
  else
  {
    *prVal /= 1000000000;
    unit = 'G';
  }

  for (int k=2, s=10; k >= 0; k--, s*=10)
  {
    if ((int)(*prVal) / s == 0)
    {
      dtostrf(*prVal, 4, k, fStr); 
      break;
    }
  }
  return unit;
}

void StartACVoltage() {
  display.clearDisplay();
  display.setCursor(0,10);
  display.println("AC VOLTAGE....");
  display.display();
  delay(2000);
  int reading, voltAC;
  while(true) {
    reading = analogRead(A0);
    voltAC = map(reading, 0, 1023, 0, 230);
    display.clearDisplay();
    display.setCursor(0,10);
    display.print("AC VOLTAGE: "); 
    display.print(voltAC);
    display.display();
    if (checkButtonPress()) break;
    delay(500);
  }
}

void StartOhmmeter() {
  display.clearDisplay();
  display.setCursor(0,10);
  display.println("OHM Meter....");
  display.display();
  delay(2000);
  while(true) {
    int cOut;
    float delta, deltaBest1 = MAX_ANALOG_VALUE, deltaBest2 = MAX_ANALOG_VALUE;
    float rBest1 = -1, rBest2 = -1, rR, rX;
    char unit = 0, fStr[16];
  
    for (byte count = 0; count < NUM_REF_RESISTORS; count++)
    {
      // Set the Mux select pins to switch in one Rref at a time.
      // count=0: Rref0 (49.9 ohms), count=1: Rref1 (100 ohms), etc...
      digitalWrite(rSelPins[0], count & 1); // C: least significant bit
      digitalWrite(rSelPins[1], count & 2); // B:
      digitalWrite(rSelPins[2], count & 4); // A: most significant bit
      
      digitalWrite(enableMux, LOW);       // enable the selected reference resistor
      delay(count + 1);                   // delay 1ms for Rref0, 2ms for Ref1, etc...
      cOut = analogRead(A1);              // convert analog voltage Vx to a digital value
      digitalWrite(enableMux, HIGH);      // disable the selected reference resistor
      delay(NUM_REF_RESISTORS - count);   // delay 8ms for Rref0, 7ms for Ref1, etc...
      // Work only with valid digitized values
      if (cOut < MAX_ANALOG_VALUE)
      {
        // Identify the Rref value being used and compute Rx based on formula #2.
        // Note how Mux's internal switch resistance is added to Rref. 
        rR = rRef[count] + SWITCH_RESISTANCE; 
        rX = (rR * cOut) / (MAX_ANALOG_VALUE - cOut);
        // Compute the delta and track the top two best delta and Rx values
        delta = (MAX_ANALOG_VALUE / 2.0 - cOut);
        if (fabs(delta) < fabs(deltaBest1))
        {
          deltaBest2 = deltaBest1;
          rBest2 = rBest1;
          deltaBest1 = delta;
          rBest1 = rX;
        }
        else if (fabs(deltaBest2) > fabs(delta))
        {
          deltaBest2 = delta;
          rBest2 = rX;
        }
      }
    }
    // Make sure there are at least two good samples to work with
    if (rBest1 >= 0 && rBest2 >= 0)
    {
      // Check to see if need to interpolate between the two data points.
      // Refer to the documentation for details regarding this.
      if (deltaBest1 * deltaBest2 < 0)
      {
        rX = rBest1 - deltaBest1 * (rBest2 - rBest1) / (deltaBest2 - deltaBest1); // Yes
      }
      else
      {
        rX = rBest1;  // No. Just use the best value
      }
      // Convert the scaled float result to string and extract the units
      unit = ScaleToMetricUnits(&rX, fStr);
    }
    display.clearDisplay();
    display.setCursor(0,10);
    display.print("OHM Meter: ");
    display.print(fStr);
    display.print(unit);
    display.display();
    if (checkButtonPress()) break;
    delay(250);
  }
}

void StartTempControl() {
  display.clearDisplay();
  display.setCursor(0,10);
  display.println("Temp Control....");
  display.display();
  delay(2000);
  double r4, eth, ln, t;
  while(true) {
    eth = analogRead(A3) - analogRead(A2);
    eth = eth/1023.0;    
    r4 = (REFVOL*R2*R3) * 1.0;
    r4 = r4 - (eth*(R1*R2+R2*R3));
    r4 = r4/((REFVOL*R1)+(eth*(R3+R1)));
    ln = log(r4 / 10.0);
    t = (1 / ((ln / 3950.0) + (1 / 298.15))); 
    t = t - 273.15;
    display.clearDisplay();
    display.setCursor(0,10);
    display.println("Temp Control:");
    display.print("Curr: ");
    display.print(t);
    display.print(", Tar: ");
    display.println(TARGETTEMP);
    display.print("Fan: ");
    
    if (t < TARGETTEMP) {
      display.println("OFF");
      display.print("LED: ");
      display.println("ON");
      digitalWrite(LED, HIGH);
      digitalWrite(FAN, LOW);
    } else {
      display.println("ON");
      display.print("LED: ");
      display.println("OFF");
      digitalWrite(LED, LOW);
      digitalWrite(FAN, HIGH);
    }
    
    display.display();
    if (checkButtonPress()) {
      digitalWrite(LED, LOW);
      digitalWrite(FAN, LOW);
      break;
    }
    delay(250);
  }
  
}

void loop() {
 StartACVoltage(); 
 StartOhmmeter();
 StartTempControl();
}
