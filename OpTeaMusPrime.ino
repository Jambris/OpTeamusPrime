// External includes
#include <Servo.h> // Arduino default servo motor library
#include <AccelStepper.h> // Arduino non-default stepper motor library with acceleration and deceleration methods
#include <LiquidCrystal.h> // Arduino default LCD display library
#include <Bounce2.h> // Button debouncing library
#include <NewPing.h> // Ultrasonic sensor library
#include "EncoderStepCounter.h" // Rotary encoder library

#define ENCODER_PIN1 3
#define ENCODER_INT1 digitalPinToInterrupt(ENCODER_PIN1)
#define ENCODER_PIN2 2
#define ENCODER_INT2 digitalPinToInterrupt(ENCODER_PIN2)

// Define Digital IO pin numbers - Skip Pin 13 if possible

	// Servo pins
const int pivotServoPin = 4; // PWM Pin
const int grabberServoPin = 5; // PWM Pin

	// User Input
const int loadButton = 22; // Pulled up and debounced in setup
const int nextButton = 23; // Pulled up and debounced in setup

	// Bool Inputs & Sensors
const int ultrasonicTrig = 45; // Ultrasonic sensor trigger pin
const int ultrasonicEcho = 46; // Ultrasonic sensor echo pin

	// Bool Outputs
const int heatingCoil = 47;
const int waterPump = 48;
const int airPump = 49;

// Creates an LCD object. Parameters: (rs, enable, d4, d5, d6, d7) - See appendix below for LCD Pin wiring and assignments
LiquidCrystal lcd(30, 31, 32, 33, 34, 35);

// Define Analog IO Pins
const int temperatureSensor = A0; // NTC Thermistor on voltage divider
const int waterReservoir = A1;  // Water level probes inside cold water reservoir tank (outside tube 2 wires)
const int waterFill = A2;  // Water level probe inside boiler for regular size
const int waterFillMax = A3; // Water level probe inside boiler for max size

// NTC Thermistor constants
// Thermistor red to 5v, black to junction of 5k resistor, junction to A0, 5k resistor to ground
const float Rref = 5000.0;  // Reference resistance
const float nominal_temeprature = 25.0;  // Nominal temperature in Celsius
const float nominal_resistance = 50000.0;  // Nominal resistance at nominal temperature (ohms)
const float beta = 3950.0;  // Beta value of the NTC thermistor

// Calculate temp C from NTC Thermistor
float calculateTemperature(int temperatureSensor) {
  float R = Rref * (1023.0 / (float)temperatureSensor - 1.0); // Calculate NTC resistance
  float T = 1.0 / (1.0 / (nominal_temeprature + 273.15) + log(R / nominal_resistance) / beta);
  T -= 273.15; // convert absolute temp to C
  return T;
}

// Define other constants
const int dispenseDuration = 10000;  // Air pump avtivation time to dispense all hot water from boiler to cup (in milliseconds)
const int generalDelay = 15; // Update this to adjust the general delay time for all functions
const int servoDelay = 20; // Update this to adjust the slow movement of servos
const int steepTimeAdjustInterval = 30000; // Adjust steep time by this increment in +/- milliseconds
const int shortWait = 100; // Some delay variables
const int medWait = 1000; // Some delay variables
const int longWait = 5000; // Some delay variables


// Create Servo and Stepper objects
Servo pivotServo; // Create pivotServo object using Servo library class
Servo grabberServo; // Create grabberServo object using Servo library class
AccelStepper elevatorRack(AccelStepper::FULL4WIRE, 6, 7, 8, 9); // Create AccelStepper object called "elevatorRack"
NewPing sonar(ultrasonicTrig, ultrasonicEcho); // Create a NewPing object for ultrasonic sensor

const int elevatorRackLimitSwitch = 10; // Limit switch pulled high in setup
const float ballscrewPitch = 8.0; // ballscrewPitch in millimeters
const float stepsPerRevolution = 200.0; // Number of steps per full revolution

// Calculate steps required for given distance in centimeters (hence why x10)
int calculateSteps(float distanceCm) {
  return static_cast<int>((distanceCm * 10 / ballscrewPitch) * stepsPerRevolution);
}

// Word substitutions for pivotServo positions
const int stupidOffset = -10; // pivotServo gets out of alignment. This is a stupid fix for that globally.
const int NORTH = 33+stupidOffset; // For pivotServo pointing straight up
const int EAST = 75+stupidOffset; // For pivotServo pointing to the right
const int SOUTH = 120+stupidOffset; // For pivotServo pointing straight down - This is the expected initial position and index pivot arm to be pointing down
const int SEAST = 98+stupidOffset; // For pivotServo down and right
const int NEAST = 56+stupidOffset; // For pivotServo up and right

// Word substitutions for grabberServo positions
const int CLOSE = 90; // Grabber servo closed position (90 deg)
const int OPEN = 0; // Grabber servo open position (0 deg)


// Define Tea Types
struct teaRecipe {
  const char* name;
  unsigned long time;
  int temp;
};

teaRecipe teaParams[] = {
  {"White Tea", 270000, 79},  // Index 0
  {"Green Tea", 240000, 79},  // Index 1
  {"Black Tea", 210000, 91},  // Index 2
  {"Oolong Tea", 210000, 91}, // Index 3
  {"Herbal Tea", 800000, 99}, // Index 4
};

int finalSteepTime = 0;  // Variable to store new steeping time from progAdjust function
const int numTeas = sizeof(teaParams) / sizeof(teaParams[0]);  // Calculate size of teaRecipe array index
unsigned int selectedTeaTime = 0;
unsigned int selectedTeaTemp = 0;
char selectedTeaName[15];
int currentRecipeIndex = 0;
// Create instance for one full step encoder
EncoderStepCounter encoder(ENCODER_PIN1, ENCODER_PIN2, HALF_STEP);

const int debounceInterval = 15; // Button debounce interval in milliseconds
int cupSizeSelection = 0;  // Variable to store whcih size the user selects (0 is small, 1 is large)
Bounce loadButtonDebouncer = Bounce();  // Create bounce object for button
Bounce nextButtonDebouncer = Bounce();  // Create bounce object for button




//					MAIN CODE STARTS HERE
//			MAIN CODE STARTS HERE
//	MAIN CODE STARTS HERE
//			MAIN CODE STARTS HERE
//					MAIN CODE STARTS HERE




void setup() {
  Serial.begin(9600);
  Serial.println("Void setup is running");

  pinMode(elevatorRackLimitSwitch, INPUT_PULLUP);
  pinMode(heatingCoil, OUTPUT);
  pinMode(airPump, OUTPUT);
  pinMode(waterPump, OUTPUT);
  lcd.begin(16, 2);
  delay(generalDelay);
  lcd.clear();
  delay(generalDelay);
  lcd.print("OpTeaMus Prime");
  delay(generalDelay);

  grabberServo.attach(grabberServoPin);
  pivotServo.attach(pivotServoPin);
  elevatorRack.setMaxSpeed(800);
  elevatorRack.setAcceleration(500);

  // Initialize encoder
  encoder.begin();
  // Initialize interrupts
  attachInterrupt(ENCODER_INT1, interrupt, CHANGE);
  attachInterrupt(ENCODER_INT2, interrupt, CHANGE);
  
  pinMode(loadButton, INPUT_PULLUP);
  pinMode(nextButton, INPUT_PULLUP);
  loadButtonDebouncer.attach(loadButton, INPUT_PULLUP);
  loadButtonDebouncer.interval(debounceInterval);
  nextButtonDebouncer.attach(nextButton, INPUT_PULLUP);
  nextButtonDebouncer.interval(debounceInterval);

  pinMode(ultrasonicTrig, OUTPUT);
  pinMode(ultrasonicEcho, INPUT);

  digitalWrite(heatingCoil, LOW); // SSR set to low until needed.
  digitalWrite(airPump, HIGH);  // For some reason relay module has NO closed when low. So set to HIGH at setup. LOW to activate.
  digitalWrite(waterPump, HIGH); // For some reason relay module has NO closed when low. So set to HIGH at setup. LOW to activate.

}



void loop() {
// Main loop calls functions declared below
  Serial.println("The main loop function is starting");
  delay(medWait);
//  debugStartup();
//  startupInit();
  loadGrabber();
  teaSelection();
  progAdjust();
  whereAmI();
// selectCupSize();
// preFlight();
// pumpColdWater();
// heatWater();
// pumpHotWater();
// steepFunction();
// shutDown();
}

void whereAmI() {
  Serial.println("Code has moved to the next phase correctly");
  lcd.clear();
  lcd.print("WhereAmI");
  Serial.println("Current variables are: ");
  Serial.print("Tea Index: ");
  Serial.println(currentRecipeIndex);
  Serial.print("Tea Name: ");
  Serial.println(teaParams[currentRecipeIndex].name);
  Serial.print("Tea Time: ");
  Serial.println(teaParams[currentRecipeIndex].time);
  Serial.print("Tea Temp: ");
  Serial.println(teaParams[currentRecipeIndex].temp);
  delay(500000);
}

void debugStartup() {
  Serial.println("debugStartup is running!");
  // Allows the user to tweak or adjust teaTime variable by using the rotary encoder (rotaryInput)
  // to add or subtract time from teaTime variable in increments of 30 seconds (30000ms)

  // Clear the LCD display
  lcd.clear();
  lcd.print("Press start");

  // While the nextButton is not pressed, allow the user to adjust the steep time
  while (nextButtonDebouncer.read() == HIGH) {

    // Check and debounce nextButton
    nextButtonDebouncer.update();

    // Exit the function if the nextButton is pressed
    if (nextButtonDebouncer.fell()) {
      return;
    }

    // Delay to avoid rapid changes due to noise
    delay(generalDelay);
  }
}

void interrupt() {  // Used for encoder library to track interrupts
  encoder.tick();
}

void rotateServoSlowly(Servo servo, int targetPosition) {
  // Function to rotate the servo slowly to the specified position
  int currentPosition = servo.read();
  int step = (targetPosition > currentPosition) ? 1 : -1;

  while (currentPosition != targetPosition) {
    currentPosition += step;
    servo.writeMicroseconds(map(currentPosition, 0, 180, 1000, 2000));
    delay(servoDelay);  // Adjust the delay for the desired rotation speed
  }
}

void startupInit() {
  Serial.println("startupInit function is running");

  // Print messages to the LCD
  lcd.clear();
  lcd.print("OpTeaMus Prime");
  lcd.setCursor(0, 1);
  lcd.print("Getting ready..");
  delay(generalDelay);
  //Trying to get initial rotation to be slower
  for (int i = pivotServo.read(); i < SOUTH; i++) {
    pivotServo.write(i);
    delay(servoDelay);  // Adjust the delay for the desired movement speed
  }
  // Perform homing procedure
  Serial.println("Homing procedure started...");
  delay(shortWait);
  
  lcd.clear();
  lcd.print("Rack Indexing...");
  delay(generalDelay);
  
  while (digitalRead(elevatorRackLimitSwitch) == HIGH) {
    elevatorRack.moveTo(-12000); // Adjust the distance as needed  //-12000 steps is up (negative) 12cm
    elevatorRack.run();
  }

  elevatorRack.stop();
  elevatorRack.setCurrentPosition(0);
  lcd.clear();
  lcd.print("Indexing Done.");

  delay(shortWait);

  // Move away from the limit switch, 2cm
  elevatorRack.moveTo(calculateSteps(2));  // Move down 2cm
  while (elevatorRack.distanceToGo() != 0) {
    elevatorRack.run();
  }
  elevatorRack.stop();
  
  Serial.println("Homing procedure completed.");
  delay(medWait);

  rotateServoSlowly(pivotServo, SOUTH); // Now that the elevator is at the top position, rotate grabber to SOUTH

}


void loadGrabber() {
  Serial.println("loadGrabber function is running");

  // Print instructions to the LCD
  lcd.clear();
  lcd.print("Hold load btn");
  lcd.setCursor(0, 1);
  lcd.print("Then next");
  delay(generalDelay);
  grabberServo.write(CLOSE);  // Start by closing the grabber
  nextButtonDebouncer.update();

  while (digitalRead(nextButton) == HIGH) {
    // Update debouncer for loadButton
    loadButtonDebouncer.update();
    nextButtonDebouncer.update();
    // Check debounced button state
    if (loadButtonDebouncer.fell()) {
      // Load button pressed
      grabberServo.write(OPEN);
    } else if (loadButtonDebouncer.rose()) {
      // Load button released
      grabberServo.write(CLOSE);
    }
    delay(generalDelay); // Avoid rapid checking
  }
  delay(generalDelay);
}


void teaSelection() {
  Serial.println("teaSelection function is running");
  lcd.clear();
  lcd.print("Select Tea");
  // Print initial value of currentRecipeIndex
  lcd.setCursor(0, 1);
  lcd.print(teaParams[currentRecipeIndex].name);
  nextButtonDebouncer.update();

  while (nextButtonDebouncer.read() == HIGH) {
    signed char pos = encoder.getPosition();
    if (pos != 0) {
      if (pos > 0) {
        currentRecipeIndex++;  // Clockwise rotation
        if (currentRecipeIndex >= numTeas) {
          currentRecipeIndex = 0;  // Loop back to the beginning
        }
      } else {
        currentRecipeIndex--;  // Counterclockwise rotation
        if (currentRecipeIndex < 0) {
          currentRecipeIndex = numTeas - 1;  // Loop to the end
        }
      }
      encoder.reset();
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(teaParams[currentRecipeIndex].name);
    }

    // Check if the button is pressed
    if (nextButtonDebouncer.fell()) {
      delay(generalDelay);
      return;
    }
    // Check and debounce nextButton
    nextButtonDebouncer.update();
  }
}


void progAdjust() {
  Serial.println("progAdjust function is running");
  long adjustedTime = 0;  //Variable to store adjusted time delta
  lcd.clear();
  lcd.print("Adj Steep Time");
  lcd.setCursor(0, 1);
  lcd.print("+0s");
  Serial.print("BeforeWhileStatement: ");
  Serial.println(adjustedTime);
  // While the nextButton is not pressed, allow the user to adjust the steep time
  while (nextButtonDebouncer.read() == HIGH) {
    signed char pos = encoder.getPosition();
    if (pos != 0) {
      if (pos > 0) {
        adjustedTime = adjustedTime + steepTimeAdjustInterval;  // Clockwise rotation
        Serial.print("ifPos>0: ");
        Serial.println(adjustedTime);
      } else {
        adjustedTime = adjustedTime - steepTimeAdjustInterval;  // Counter-clockwise rotation
        Serial.print("elsePos<0: ");
        Serial.println(adjustedTime);
      }
      encoder.reset();
      Serial.print("InsideWhileStatement: ");
      Serial.println(adjustedTime);
      // Display plus or minus and the total time dynamically on the second line
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(adjustedTime >= 0 ? "+" : "-");
      lcd.print(abs(adjustedTime)/1000);
      lcd.print("s");

      // Calculate adjusted steep time
      //int finalSteepTime = teaParams[currentRecipeIndex].time + adjustedTime;
    }

    // Exit the function if the nextButton is pressed
    if (nextButtonDebouncer.fell()) {
      delay(generalDelay);
      return;
    }
    // Check and debounce nextButton
    nextButtonDebouncer.update();
  }
}

/*
void progAdjustOld() {
  Serial.println("progAdjust function is running");
  // Allows the user to tweak or adjust teaTime variable by using the rotary encoder (rotaryInput)
  // to add or subtract time from teaTime variable in increments of 30 seconds (30000ms)

  // Clear the LCD display
  lcd.clear();
  lcd.print("Adjust steep");

  // Initialize the encoder value and last value
  int encoderValue = 0;
  int encoderLastValue = 0;

  // While the nextButton is not pressed, allow the user to adjust the steep time
  while (nextButtonDebouncer.read() == HIGH) {
    // Read changes from the selectorKnob
    int selectorKnobChange = selectorKnob.read();

    // Update selectorKnobValue based on the change
    int selectorKnobValue = selectorKnobChange;

    // Ensure the selectorKnob value stays within valid bounds
    selectorKnobValue = constrain(selectorKnobValue, -1, 1);

    // If the selectorKnob value changes, update the LCD display and encoder value
    if (selectorKnobChange != 0) {
      encoderValue += selectorKnobValue;

      // Calculate adjusted steep time
      int adjustedTime = selectedTeaTime + encoderValue * steepTimeAdjustInterval;

      // Display plus or minus and the total time dynamically on the second line
      lcd.setCursor(0, 1);
      lcd.print(encoderValue > 0 ? "+" : "-");
      lcd.print(abs(adjustedTime) / 1000);
      lcd.print("s");

      // Update the last encoder value
      encoderLastValue = encoderValue;
    }

    // Check and debounce nextButton
    nextButtonDebouncer.update();

    // Exit the function if the nextButton is pressed
    if (nextButtonDebouncer.fell()) {
      return;
    }

    // Delay to avoid rapid changes due to noise
    delay(generalDelay);
  }
}


void selectCupSize() {
  Serial.println("selectCupSize fucnction is running");
  // Clear the LCD display
  lcd.clear();

  // Display the cup size selection prompt on the first row
  lcd.print("Select Cup Size");

  // Display the current cup size on the second row
  lcd.setCursor(0, 1);
  lcd.print("Small   Large");

  // Rotary encoder variables
  int encoderValue = 0;
  int encoderLastValue = 0;

  // While the nextButton is not pressed, allow the user to scroll through cup size options
  while (nextButtonDebouncer.read() == HIGH) {
    // Read changes from the selectorKnob
    int selectorKnobChange = selectorKnob.read();

    // Update selectorKnobValue based on the change
    int selectorKnobValue = selectorKnobChange;

    // Ensure the selectorKnob value stays within valid bounds (0 for Small, 1 for Large)
    selectorKnobValue = constrain(selectorKnobValue, 0, 1);

    // If the selectorKnob value changes, update the LCD display
    if (selectorKnobChange != 0) {
      lcd.setCursor(0, 1);
      lcd.print(selectorKnobValue == 0 ? ">Small   Large" : " Small   >Large");

      // Update the last encoder value
      encoderLastValue = encoderValue;
    }

    // Check and debounce nextButton
    nextButtonDebouncer.update();

    if (nextButtonDebouncer.fell()) {
      // User made a selection, update the global variable
      cupSizeSelection = selectorKnobValue;

      // Exit the function
      return;
    }
  }
}

*/

/*

void preFlight() {
  Serial.println("preFlight function is running");

  // Check ultrasonic sensor to ensure a cup is placed for dispensing hot water into it
  int distance = sonar.ping_cm();

  if (distance > 0 && distance <= 10) {
    // Cup is present within 10cm
    lcd.clear();
    lcd.print("Cup Detected");
    delay(medWait); // Display the message for 1 second
  } else {
    // Cup is not present or out of range
    lcd.clear();
    lcd.print("Place a cup");
    while (sonar.ping_cm() > 10 || sonar.ping_cm() == 0); // Wait here until a cup is placed
  }

  // Check boolean sensor to ensure the water reservoir has enough water to perform water heating/brewing/steeping
  if (digitalRead(waterReservoir) == LOW) {
    // Water reservoir is empty
    lcd.clear();
    lcd.print("Add water");
    while (digitalRead(waterReservoir) == LOW); // Wait here until water is refilled
  }

  // Wait for the next button to be pressed
  lcd.clear();
  lcd.print("Press Next");
  lcd.setCursor(0, 1); // Move cursor to the beginning of the second row
  lcd.print("to Start");

  // Update the debouncer before checking the button state
  nextButtonDebouncer.update();

  while (nextButtonDebouncer.read() == HIGH) {
    delay(generalDelay);
  }
}


void pumpColdWater() {  // Function to pump water from the cold reservoir to the boiler
  Serial.println("pumpColdWater fucnction is running");
  digitalWrite(waterPump, LOW); // Activate the water pump

  // Check which water fill probe to use based on cup size selection
  int waterFillProbe;

  if (cupSizeSelection == 0) {
    waterFillProbe = waterFill;
  } else {
    waterFillProbe = waterFillMax;
  }

  // Wait until the water reaches the selected water fill probe level
  while (digitalRead(waterFillProbe) == LOW) {
    // Delay to avoid rapid checking
    delay(generalDelay);
  }

  digitalWrite(waterPump, HIGH); // Stop the water pump
}


void heatWater() { // Function to heat water in the boiler
  Serial.println("heatWater fucnction is running");
  digitalWrite(heatingCoil, LOW); // Activate the heating coil

  // Wait for the heating coil to warm up (adjust as needed)
  delay(medWait);

  lcd.clear();
  lcd.print("Heating Water");

  while (true) {
    int temperatureReading = analogRead(temperatureSensor);
    float temperatureCelsius = calculateTemperature(temperatureReading);

    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.print(temperatureCelsius);
    lcd.print(" C   ");

    if (temperatureCelsius >= selectedTeaTemp) {  // Continuously check if the current temperature is below the target temperature, otherwise, turn off heatingCoil
      digitalWrite(heatingCoil, HIGH);  // Turn off the heatingCoil because selectedTeaTemp has been reached
      break;
    }
    delay(generalDelay);  // Delay to avoid rapid checking
  }
}


void pumpHotWater() {
  Serial.println("pumpHotWater fucnction is running");
  unsigned long startTime = millis();

  while (millis() - startTime < dispenseDuration) {
    digitalWrite(airPump, HIGH);
  }

  digitalWrite(airPump, LOW);
}


void steepFunction() {
  Serial.println("steepFunction is running");

  // Lower the Stepper "elevatorRack" 150mm down into the cup
  int lowerDistance = 150;  // Distance to move down in mm
  int lowerSteps = lowerDistance * pitchToDistance;  // Convert distance to steps
  elevatorRack.move(lowerSteps);
  elevatorRack.runToPosition();  // Wait for the move to complete

  // Show a countdown on the LCD display as the tea is being steeped
  lcd.clear();
  lcd.print("Steeping...");

  // Variables for periodic movement
  int dunkDistance = 20;  // Distance to move up and down during dunking in mm
  int dunkSteps = dunkDistance * pitchToDistance;  // Convert distance to steps
  unsigned long startTime = millis();
  int dunkInterval = 20000;  // Dunking interval in milliseconds (20 seconds)

  int secondsRemaining = selectedTeaTime / 1000;

  while (secondsRemaining >= 0) {
    lcd.setCursor(0, 1);
    lcd.print("Time left: ");
    lcd.print(secondsRemaining);
    lcd.print("s  ");

    // Check if it's time to perform a dunk
    if (millis() - startTime >= dunkInterval) {
      // Move up for dunking
      elevatorRack.move(dunkSteps);
      elevatorRack.runToPosition();  // Wait for the move to complete

      // Move down for dunking
      elevatorRack.move(-dunkSteps);
      elevatorRack.runToPosition();  // Wait for the move to complete

      startTime = millis();  // Reset the timer for the next dunk
    }

    // Update time remaining every second
    if (millis() % 1000 == 0) {
      secondsRemaining--;
    }

    // Add a small delay to avoid rapid checking
    delay(generalDelay);
  }

  // Raise the elevatorRack back up to its starting position
  elevatorRack.move(referenceOffset);  // Return to home position (named referenceOffset)
  elevatorRack.runToPosition();  // Wait for the move to complete
}


void disposeBag() {
  // Print to LCD first row "please remove". Print to LCD second line "cup".
  lcd.clear();
  lcd.print("Please remove");
  lcd.setCursor(0, 1);
  lcd.print("cup");

  // Wait 1 second (non-blocking delay)
  delay(medWait);

  // Check ultrasonic sensor that the cup has indeed been removed and there is no object detected within 3 cm of sensor.
  while (sonar.ping_cm() <= 3) {
    // Wait here until the cup is removed
    delay(generalDelay);
  }

  // Clear LCD and print "Resetting"
  lcd.clear();
  lcd.print("Resetting");

  // Move elevatorRack down to 200mm position (down 200mm away from "referenceOffset" position).
  elevatorRack.move(200 * pitchToDistance);
  elevatorRack.runToPosition();

  // Rotate grabber to SEAST position while the elevatorRack is moving down.
  pivotServo.write(SEAST);

  // Once elevatorRack and grabberArm reach their final positions, open the grabberServo (OPEN).
  while (elevatorRack.isRunning()) {
    delay(generalDelay);
  }
  grabberServo.write(OPEN);
  delay(medWait); // Wait for a second

  // Move the elevatorRack back to "referenceOffset" position
  elevatorRack.move(referenceOffset);
  elevatorRack.runToPosition();

  // Reset grabberArm to SOUTH.
  pivotServo.write(SOUTH);
  
  lcd.clear();
  lcd.print("Please");
  lcd.setCursor(0, 1);
  lcd.print("Power off");

// Wave goodbye?

}

*/

void shutDown() {
  lcd.clear();
  lcd.print("Please");
  lcd.setCursor(0, 1);
  lcd.print("Power off");
  delay(500000); // Long delay for now so it doesnt loop the functions
  // Code for shutting down the unit
  // Activate latching circuit
}


/*

APPENDIX:

	LCD display

		LCD Pins               Arduino Pins
		---------------------------------------
		1.  LCD VCC ---------------> 5V
		2.  LCD GND ---------------> GND
		3.  LCD Vo  ---------------> Connect to 10k Potentiometer center (for contrast control)
							    ---> 5V  (Potentiometer)
							    ---> GND (Potentiometer)
		4.  LCD RS  ---------------> D30
		5.  LCD RW  ---------------> GND
		6.  LCD EN  ---------------> D31
		11. LCD D4  ---------------> D32
		12. LCD D5  ---------------> D33
		13. LCD D6  ---------------> D34
		14. LCD D7  ---------------> D35
		15. LCD Backlight Anode ---> 5V (with Resistor (220 ohm))
		16. LCD Backlight Cathode-> GND

		LCD power consumption is 1.25mA
	
	
*/



/* Note for water sensor VD
const int waterProbePin = A0;  // Analog input pin for water probe

void setup() {
  Serial.begin(9600);  // Initialize serial communication
}

void loop() {
  // Read the analog value from the water probe
  int waterValue = analogRead(waterProbePin);

  // Print the analog reading to the Serial Monitor
  //Serial.print("Analog Reading: ");
  Serial.println(waterValue);

  // Add a delay to control the rate of readings (adjust as needed)
  delay(1000);
}
*/

/*
Grabber servo rotation arm degree calibration
  pivotServo.write(120);  // South grabber facing down
  Serial.println("We're pointing South.");
  delay(1000);
  pivotServo.write(98);  // SouthEast
  Serial.println("We're pointing SouthEast.");
  delay(1000);
  pivotServo.write(75);  // East
  Serial.println("We're pointing East.");
  delay(1000);
  pivotServo.write(56);  // NorthEast
  Serial.println("We're pointing NorthEast.");
  delay(1000);
  pivotServo.write(33);  // North
  Serial.println("We're pointing North.");
  delay(5000);
  */
