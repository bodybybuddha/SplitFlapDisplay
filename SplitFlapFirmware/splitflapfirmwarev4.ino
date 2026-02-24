#include <SoftwareSerial.h>

// ==========================================
//               CONFIGURATION
// ==========================================
// !!! CHANGE THIS FOR EACH MODULE !!!
const String MODULE_ID = "m44"; 

const String FLAP_CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$&()-+=;q:%'.,/?*roygbpw";

// Hardware Settings
// TUNABLE VARIABLES (Can be updated via RS485)
// Note: These are now in HALF-STEPS. 
// Old value 354 * 8 = 2832
int stepsFromHallToZero = 2832; 
// Old value 512 * 8 = 4096 half-steps per revolution
int totalStepsPerRev = 4096; 

// ==========================================
//              PIN DEFINITIONS
// ==========================================
const int RS485_RX = 3;
const int RS485_TX = 1;
const int RS485_DE = 2;

#define IN1 9
#define IN2 8
#define IN3 7
#define IN4 6
#define HALL_PIN 4

// ==========================================
//               GLOBALS
// ==========================================
SoftwareSerial rs485(RS485_RX, RS485_TX);

long currentStepPos = 0; // Absolute position in half-steps from the sensor trigger
int currentPhase = 0;    // Tracks the current electrical phase (0-7)

int parseState = 0; 
int currentFlapIndex = -1;

const int stepDelay = 1; 
String buffer = "";
unsigned long lastSerialTime = 0;

const uint8_t halfStepSequence[8][4] = {
  {1, 0, 0, 0}, {1, 1, 0, 0}, {0, 1, 0, 0}, {0, 1, 1, 0},
  {0, 0, 1, 0}, {0, 0, 1, 1}, {0, 0, 0, 1}, {1, 0, 0, 1}
};

// ==========================================
//             MOTOR FUNCTIONS
// ==========================================

void applyStep(const uint8_t *step) {
  digitalWrite(IN1, step[0]);
  digitalWrite(IN2, step[1]);
  digitalWrite(IN3, step[2]); 
  digitalWrite(IN4, step[3]);
}

void stepBackward(int steps) {
  for (int k = 0; k < steps; k++) {
    // 1. Check for drift correction (Falling Edge Detection)
    bool hallNow = hallActive();
    static bool lastHallState = false;

    // If Hall sensor just went from HIGH to LOW (Magnet just arrived)
    if (hallNow && !lastHallState) {
      // Self-Healing moment
      currentStepPos = totalStepsPerRev - stepsFromHallToZero;
    }
    lastHallState = hallNow;

    // 2. Perform ONE phase of the half-step sequence
    currentPhase--;
    if (currentPhase < 0) {
      currentPhase = 7; // Wrap around to keep the backward rotation
    }
    
    applyStep(halfStepSequence[currentPhase]);
    delay(stepDelay);
    
    // 3. Increment our internal counter (now counting half-steps)
    currentStepPos++;
    
    // 4. Wrap around if we exceed the calibrated total
    if (currentStepPos >= totalStepsPerRev) {
      currentStepPos = 0;
    }
  }
}

void releaseMotor() {
  digitalWrite(IN1,0); digitalWrite(IN2,0); digitalWrite(IN3,0); digitalWrite(IN4,0);
}

bool hallActive() {
  return (digitalRead(HALL_PIN) == LOW); // LOW = Magnet Detected
}

// ==========================================
//             LOGIC FUNCTIONS
// ==========================================

void homeModule() {
  long safety = 0;
  
  // 1. Find magnet (Increased safety limit for half-steps)
  while (!hallActive() && safety < (totalStepsPerRev + 500)) {
    stepBackward(1);
    safety++;
  }
  
  // 2. We are exactly at the trigger point (0)
  
  // 3. Move to Space
  stepBackward(stepsFromHallToZero);
  currentStepPos = 0; // currentStepPos is now automatically 0
  
  currentFlapIndex = 0;
  releaseMotor();
}

void calibrateModule() {
  // 1. First, make sure we are NOT on the magnet.
  long safety = 0;
  while (hallActive() && safety < 4000) {
    stepBackward(1);
    safety++;
    delay(5); // Slow down for accuracy
  }

  // 2. Now spin until we HIT the magnet (Falling Edge)
  safety = 0;
  while (!hallActive() && safety < 5000) {
    stepBackward(1);
    safety++;
  }

  // 3. We just hit the magnet. Now move until we LEAVE the magnet.
  while (hallActive()) {
    stepBackward(1);
  }

  // 4. START COUNTING
  int measuredSteps = 0;
  
  // 5. Spin until we hit the magnet again
  while (!hallActive() && measuredSteps < 5000) {
    stepBackward(1);
    measuredSteps++;
  }

  // 6. Spin until we leave the magnet again (Full Circle Completed)
  while (hallActive()) {
    stepBackward(1);
    measuredSteps++;
  }

  // 7. BROADCAST DATA
  digitalWrite(RS485_DE, HIGH);
  delay(5); 
  
  rs485.print("m");
  rs485.print(MODULE_ID.substring(1));
  rs485.print(":");
  rs485.println(measuredSteps);
  
  rs485.flush(); 
  delay(5);
  digitalWrite(RS485_DE, LOW);

  // 8. Finalize
  totalStepsPerRev = measuredSteps;
  homeModule();
}

void moveToChar(char targetChar) {
  int targetIndex = FLAP_CHARS.indexOf(targetChar);
  if (targetIndex == -1) return;
  
  if (currentFlapIndex == -1) {
    homeModule();
  }

  // Calculate where we WANT to be in absolute steps
  // Because totalStepsPerRev is now ~4096, this automatically yields half-steps.
  long targetStepPos = ((long)targetIndex * (long)totalStepsPerRev) / 64;
  
  // Move forward until we hit the target position
  while(currentStepPos != targetStepPos){
    stepBackward(1);
  }

  releaseMotor();
  currentFlapIndex = targetIndex; 
}

// ==========================================
//              MAIN LOOP
// ==========================================

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); 
  pinMode(IN4, OUTPUT);
  pinMode(HALL_PIN, INPUT_PULLUP);
  
  pinMode(RS485_DE, OUTPUT);
  digitalWrite(RS485_DE, LOW); 
  
  rs485.begin(9600);
  homeModule();
}

void loop() {
  while (rs485.available()) {
    char c = rs485.read();
    lastSerialTime = millis();
    
switch (parseState) {
      case 0: if (c == 'm') parseState = 1; break;
      case 1: if (c == MODULE_ID.charAt(1)) parseState = 2; else parseState = 0; break;
      case 2: if (c == MODULE_ID.charAt(2)) parseState = 3; else parseState = 0; break;
      case 3: 
        if (c == '-') parseState = 4; // Move
        else if (c == 'h') { homeModule(); parseState = 0; } // Home
        else if (c == 'c') { calibrateModule(); parseState = 0; } // Calibrate
        else if (c == 'o') { buffer = ""; parseState = 5; } // Offset
        else if (c == 't') { buffer = ""; parseState = 6; } // Set Total Steps
        else if (c == 's') { buffer = ""; parseState = 7; } // NEW: Step Nudge
        else parseState = 0;
        break;
      case 4: moveToChar(c); parseState = 0; break;
      case 5: // Read Offset
        if (isDigit(c)) buffer += c;
        else { if (buffer.length()>0) stepsFromHallToZero = buffer.toInt(); parseState=0; }
        break;
      case 6: // Read Total Steps
        if (isDigit(c)) buffer += c;
        else { if (buffer.length()>0) totalStepsPerRev = buffer.toInt(); parseState=0; }
        break;
      case 7: // NEW: Read Step Nudge Amount
        if (isDigit(c)) buffer += c;
        else { 
          if (buffer.length()>0) {
            int stepsToMove = buffer.toInt();
            stepBackward(stepsToMove);
            releaseMotor();
            // Automatically update the offset variable in memory
            stepsFromHallToZero += stepsToMove; 
          }
          parseState=0; 
        }
        break;
    }
  }

  // Timeouts for number parsing
    if ((parseState == 5 || parseState == 6 || parseState == 7) && (millis() - lastSerialTime > 50)) {
      if (buffer.length() > 0) {
        if (parseState == 5) stepsFromHallToZero = buffer.toInt();
        if (parseState == 6) totalStepsPerRev = buffer.toInt();
        if (parseState == 7) {
          int stepsToMove = buffer.toInt();
          stepBackward(stepsToMove);
          releaseMotor();
          stepsFromHallToZero += stepsToMove;
        }
      }
      parseState = 0;
    }
}