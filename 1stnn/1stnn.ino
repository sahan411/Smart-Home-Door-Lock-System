#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>

// -------------------------
// OLED Display Definitions
// -------------------------
#define SCREEN_WIDTH 128   // OLED width in pixels
#define SCREEN_HEIGHT 64   // OLED height in pixels
#define OLED_RESET    -1   // Reset pin (set to -1 if not used)
#define OLED_ADDRESS  0x3C // I2C address for OLED display

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------------
// Keypad Definitions
// -------------------------
const byte ROWS = 4;      // number of rows in keypad
const byte COLS = 4;      // number of columns in keypad
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {18, 19, 32, 33};
byte colPins[COLS] = {23, 25, 26, 27};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// -------------------------
// Relay Definition
// -------------------------
#define RELAY_PIN 5  // Connect relay module control pin to GPIO5

// -------------------------
// Fingerprint Sensor Definitions
// -------------------------
#define FINGERPRINT_RX 16  // ESP32 RX (Connect to Fingerprint TX)
#define FINGERPRINT_TX 17  // ESP32 TX (Connect to Fingerprint RX)
HardwareSerial fingerSerial(1); // Use Serial1 for fingerprint sensor
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// -------------------------
// System State and Variables
// -------------------------
enum SystemState {
  STATE_LOCKED,
  STATE_UNLOCKED,
  STATE_ENROLLING
};

SystemState currentState = STATE_LOCKED;
String correctPIN = "1234";
String enteredPIN = "";
bool fingerprintSensorAvailable = false;
unsigned long doorUnlockTime = 0;
const unsigned long DOOR_UNLOCK_DURATION = 30000; // 30 seconds unlock time

// ==========================
// DISPLAY FUNCTIONS
// ==========================

void displayMessage(String message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}

void showMainScreen() {
  String message = "Door Lock System\n\n";
  
  if (currentState == STATE_LOCKED) {
    message += "Enter PIN or scan\nfingerprint\n\n";
    message += "A: Fingerprint Mode\n";
    message += "B: View Help";
  } 
  else if (currentState == STATE_UNLOCKED) {
    message += "DOOR UNLOCKED\n\n";
    message += "B: Enroll Fingerprint\n";
    message += "D: Lock Door Now\n";
    
    // Show remaining time
    int remainingTime = (doorUnlockTime + DOOR_UNLOCK_DURATION - millis()) / 1000;
    message += "Auto-lock in: " + String(remainingTime) + "s";
  }
  else if (currentState == STATE_ENROLLING) {
    message += "ENROLLMENT MODE\n";
    message += "Follow instructions\n";
    message += "*: Cancel enrollment";
  }
  
  displayMessage(message);
}

void showPINEntry() {
  String message = "Enter PIN:\n";
  
  // Show masked PIN
  for (unsigned int i = 0; i < enteredPIN.length(); i++) {
    message += "*";
  }
  
  message += "\n\n";
  message += "*: Clear | #: Submit";
  
  displayMessage(message);
}

void showError(String error) {
  displayMessage("ERROR\n\n" + error);
  delay(2000);
  showMainScreen();
}

// ==========================
// DOOR LOCK FUNCTIONS
// ==========================

void unlockDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  currentState = STATE_UNLOCKED;
  doorUnlockTime = millis();
  
  displayMessage("ACCESS GRANTED\n\nDoor unlocked");
  delay(1500);
  
  showMainScreen();
}

void lockDoor() {
  digitalWrite(RELAY_PIN, LOW);
  currentState = STATE_LOCKED;
  enteredPIN = "";
  
  displayMessage("Door locked");
  delay(1000);
  
  showMainScreen();
}

// ==========================
// AUTHENTICATION FUNCTIONS
// ==========================

void checkPIN() {
  if (enteredPIN == correctPIN) {
    unlockDoor();
  } else {
    displayMessage("ACCESS DENIED\n\nInvalid PIN");
    delay(2000);
    enteredPIN = "";
    showMainScreen();
  }
}

void checkFingerprint() {
  uint8_t p = finger.getImage();
  
  if (p != FINGERPRINT_OK) {
    return; // No finger or error
  }
  
  displayMessage("Processing\nfingerprint...");
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    showError("Image conversion failed");
    return;
  }
  
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Fingerprint match found! ID #");
    Serial.println(finger.fingerID);
    unlockDoor();
  } else {
    displayMessage("Fingerprint not\nrecognized");
    delay(2000);
    showMainScreen();
  }
}

// ==========================
// FINGERPRINT ENROLLMENT
// ==========================

void startEnrollment() {
  if (!fingerprintSensorAvailable) {
    showError("No fingerprint sensor");
    return;
  }
  
  currentState = STATE_ENROLLING;
  showMainScreen();
  delay(1000);
  
  // Find next available ID
  int id = 1;
  while (finger.loadModel(id) == FINGERPRINT_OK) {
    id++;
    if (id >= 127) {
      showError("Fingerprint memory full");
      currentState = STATE_UNLOCKED;
      showMainScreen();
      return;
    }
  }
  
  Serial.print("Enrolling ID #");
  Serial.println(id);
  
  enrollFingerprint(id);
}

void enrollFingerprint(int id) {
  uint8_t p = FINGERPRINT_NOFINGER;
  
  // ----- First Scan -----
  displayMessage("Place finger on\nsensor...");
  
  while (true) {
    p = finger.getImage();
    
    // Check for cancel
    char key = keypad.getKey();
    if (key == '*') {
      displayMessage("Enrollment cancelled");
      delay(1500);
      currentState = STATE_UNLOCKED;
      showMainScreen();
      return;
    }
    
    if (p == FINGERPRINT_OK) {
      break;
    }
    
    delay(100);
  }
  
  displayMessage("Processing...");
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    showError("Processing failed");
    currentState = STATE_UNLOCKED;
    showMainScreen();
    return;
  }
  
  // ----- Wait for finger removal -----
  displayMessage("Remove finger");
  delay(2000);
  
  p = FINGERPRINT_NOFINGER;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    delay(100);
  }
  
  // ----- Second Scan -----
  displayMessage("Place same finger\nagain");
  
  while (true) {
    p = finger.getImage();
    
    // Check for cancel
    char key = keypad.getKey();
    if (key == '*') {
      displayMessage("Enrollment cancelled");
      delay(1500);
      currentState = STATE_UNLOCKED;
      showMainScreen();
      return;
    }
    
    if (p == FINGERPRINT_OK) {
      break;
    }
    
    delay(100);
  }
  
  displayMessage("Processing...");
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    showError("Processing failed");
    currentState = STATE_UNLOCKED;
    showMainScreen();
    return;
  }
  
  // ----- Create Model -----
  displayMessage("Creating model...");
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    showError("Fingerprints did not match");
    currentState = STATE_UNLOCKED;
    showMainScreen();
    return;
  }
  
  // ----- Store Model -----
  displayMessage("Saving as ID #" + String(id));
  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    showError("Failed to save");
    currentState = STATE_UNLOCKED;
    showMainScreen();
    return;
  }
  
  displayMessage("Success!\nNew fingerprint\nsaved as ID #" + String(id));
  delay(3000);
  
  currentState = STATE_UNLOCKED;
  showMainScreen();
}

// ==========================
// KEY HANDLING
// ==========================

void processKey(char key) {
  Serial.print("Key pressed: ");
  Serial.println(key);
  
  // Global key handlers
  if (key == '*' && currentState != STATE_ENROLLING) {
    // Clear PIN
    enteredPIN = "";
    showPINEntry();
    return;
  }
  
  // State-specific key handlers
  switch (currentState) {
    case STATE_LOCKED:
      processLockedStateKey(key);
      break;
      
    case STATE_UNLOCKED:
      processUnlockedStateKey(key);
      break;
      
    case STATE_ENROLLING:
      // Keys for enrollment are handled in the enrollment function
      break;
  }
}

void processLockedStateKey(char key) {
  switch (key) {
    case '#':
      // Submit PIN
      checkPIN();
      break;
      
    case 'A':
      // Fingerprint mode prompt
      if (fingerprintSensorAvailable) {
        displayMessage("Place finger on\nsensor");
        delay(1000);
        showMainScreen();
      } else {
        showError("Fingerprint not available");
      }
      break;
      
    case 'B':
      // Help
      displayMessage("HELP:\n1. Enter PIN or scan finger\n2. * to clear PIN\n3. # to submit PIN\n4. A for fingerprint\n5. B for help");
      delay(5000);
      showMainScreen();
      break;
      
    default:
      // Add to PIN if it's a number
      if (key >= '0' && key <= '9') {
        enteredPIN += key;
        showPINEntry();
        
        // Auto-submit if PIN is complete
        if (enteredPIN.length() >= correctPIN.length()) {
          delay(500);
          checkPIN();
        }
      }
      break;
  }
}

void processUnlockedStateKey(char key) {
  switch (key) {
    case 'B':
      // Enroll new fingerprint
      startEnrollment();
      break;
      
    case 'D':
      // Lock door manually
      lockDoor();
      break;
  }
}

// ==========================
// SYSTEM FUNCTIONS
// ==========================

void checkDoorTimeout() {
  if (currentState == STATE_UNLOCKED) {
    if (millis() - doorUnlockTime >= DOOR_UNLOCK_DURATION) {
      displayMessage("Auto-locking door...");
      delay(1000);
      lockDoor();
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize Display
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while (true);
  }
  
  displayMessage("Initializing...");
  
  // Initialize Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Initialize Fingerprint Sensor
  fingerSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  finger.begin(57600);
  
  delay(1000);
  
  // Check fingerprint sensor
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found!");
    displayMessage("Fingerprint sensor\ndetected");
    fingerprintSensorAvailable = true;
  } else {
    Serial.println("Fingerprint sensor not found");
    displayMessage("No fingerprint sensor\nPIN mode only");
    fingerprintSensorAvailable = false;
  }
  
  delay(1500);
  showMainScreen();
}

void loop() {
  // 1. Check for key presses
  char key = keypad.getKey();
  if (key) {
    processKey(key);
  }
  
  // 2. Check fingerprint sensor if door is locked
  if (currentState == STATE_LOCKED && fingerprintSensorAvailable) {
    checkFingerprint();
  }
  
  // 3. Check if door needs to be auto-locked
  checkDoorTimeout();
  
  // Small delay for stability
  delay(50);
}
