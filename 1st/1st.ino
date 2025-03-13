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
// Reassign keypad pins to avoid conflict with I2C (OLED uses GPIO21 & GPIO22)
// Here we use different GPIOs: Rows on {18, 19, 32, 33}; Columns on {23, 25, 26, 27}
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
// Authentication Variables
// -------------------------
String correctPIN = "1234";  // Set the correct keypad PIN here
String enteredPIN = "";
bool fingerprintMode = false;  // Toggle between fingerprint and PIN modes

// -------------------------
// Function: Update OLED Display
// -------------------------
void updateDisplay(String message) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(message);
  display.display();
}

// -------------------------
// Function: Unlock Door (Activate Relay)
// -------------------------
void unlockDoor() {
  updateDisplay("Access Granted");
  Serial.println("Access Granted");
  digitalWrite(RELAY_PIN, HIGH); // Activate relay to unlock door
  delay(5000);                   // Keep door unlocked for 5 seconds
  digitalWrite(RELAY_PIN, LOW);  // Deactivate relay (lock door)
  delay(500);
  
  // Return to appropriate mode
  if (fingerprintMode) {
    updateDisplay("Scan Finger");
  } else {
    updateDisplay("Enter PIN");
    enteredPIN = "";
  }
}

// -------------------------
// Function: Check Keypad PIN
// -------------------------
void checkPIN() {
  if (enteredPIN == correctPIN) {
    unlockDoor();
  } else {
    updateDisplay("Access Denied");
    Serial.println("Access Denied");
    delay(3000);
    enteredPIN = "";
    updateDisplay("Enter PIN");
  }
}

// -------------------------
// Function: Fingerprint Authentication
// -------------------------
uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  
  if (p != FINGERPRINT_OK) {
    // Not showing errors on display for better user experience
    return p;  // No finger detected or error
  }
  
  // Display feedback
  updateDisplay("Processing...");
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return p;  // Image conversion failed
  
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Fingerprint match found! ID #");
    Serial.println(finger.fingerID);
    unlockDoor();
  } else {
    updateDisplay("No Match!");
    Serial.println("Fingerprint not recognized");
    delay(2000);
    updateDisplay("Scan Finger");
  }
  
  return p;
}

// -------------------------
// Function: Enroll New Fingerprint
// -------------------------
uint8_t enrollFingerprint() {
  int id = 1;  // ID for the new fingerprint
  
  // Find the next available ID
  while (finger.loadModel(id) == FINGERPRINT_OK) {
    id++;
    if (id >= 127) {
      Serial.println("Fingerprint memory full!");
      return FINGERPRINT_ENROLLMISMATCH;
    }
  }
  
  Serial.print("Enrolling ID #");
  Serial.println(id);
  
  updateDisplay("Place Finger");
  while (finger.getImage() != FINGERPRINT_OK) {
    // Wait for valid finger
    delay(100);
  }
  
  updateDisplay("Processing");
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    updateDisplay("Error!");
    return FINGERPRINT_IMAGEMESS;
  }
  
  updateDisplay("Remove Finger");
  delay(2000);
  
  updateDisplay("Place Again");
  while (finger.getImage() != FINGERPRINT_OK) {
    // Wait for finger again
    delay(100);
  }
  
  updateDisplay("Processing");
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    updateDisplay("Error!");
    return FINGERPRINT_IMAGEMESS;
  }
  
  // Create model
  if (finger.createModel() != FINGERPRINT_OK) {
    updateDisplay("No Match!");
    return FINGERPRINT_ENROLLMISMATCH;
  }
  
  // Store model
  if (finger.storeModel(id) != FINGERPRINT_OK) {
    updateDisplay("Error!");
    return FINGERPRINT_PACKETRECIEVEERR;
  }
  
  updateDisplay("Success! ID:" + String(id));
  delay(2000);
  return FINGERPRINT_OK;
}

// -------------------------
// Setup Function
// -------------------------
void setup() {
  Serial.begin(115200);
  Wire.begin(); // I2C uses default pins: SDA=GPIO21, SCL=GPIO22

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while (true);
  }
  delay(1000);
  
  // Initialize relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is off

  // Initialize Fingerprint Sensor on Serial1
  fingerSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  finger.begin(57600);
  
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor found!");
    updateDisplay("Fingerprint OK");
    delay(1000);
  } else {
    Serial.println("Fingerprint sensor not found :(");
    updateDisplay("No Fingerprint!");
    delay(2000);
    // Continue with keypad-only authentication
    fingerprintMode = false;
  }
  
  updateDisplay("Enter PIN");
}

// -------------------------
// Main Loop
// -------------------------
void loop() {
  // Check for keypad input
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);
    
    // Special key handling
    if (key == '*') {
      // Clear PIN entry
      enteredPIN = "";
      updateDisplay("Enter PIN");
      return;
    }
    else if (key == '#') {
      // Submit entered PIN
      checkPIN();
      return;
    }
    else if (key == 'A') {
      // Toggle between fingerprint and PIN mode
      fingerprintMode = !fingerprintMode;
      if (fingerprintMode) {
        updateDisplay("Scan Finger");
      } else {
        updateDisplay("Enter PIN");
      }
      enteredPIN = "";
      return;
    }
    else if (key == 'B' && !fingerprintMode) {
      // Enroll new fingerprint (only when in PIN mode)
      // First confirm with current PIN for security
      if (enteredPIN == correctPIN) {
        updateDisplay("Enrolling...");
        delay(1000);
        enrollFingerprint();
        enteredPIN = "";
        updateDisplay("Enter PIN");
      } else {
        updateDisplay("Access Denied");
        delay(2000);
        enteredPIN = "";
        updateDisplay("Enter PIN");
      }
      return;
    }
    else if (key == 'C') {
      // Reserved for future use
      return;
    }
    else if (key == 'D') {
      // Reserved for future use
      return;
    }
    else {
      // Add digit to PIN
      enteredPIN += key;
      // Mask the entered PIN with asterisks
      String masked = "";
      for (int i = 0; i < enteredPIN.length(); i++) {
        masked += "*";
      }
      updateDisplay(masked);
      if (enteredPIN.length() >= 4) {
        checkPIN();
      }
    }
  }

  // Check fingerprint sensor if in fingerprint mode
  if (fingerprintMode) {
    getFingerprintID();
    delay(50); // Small delay between fingerprint checks
  }
  
  delay(50); // Small delay for stability
}
