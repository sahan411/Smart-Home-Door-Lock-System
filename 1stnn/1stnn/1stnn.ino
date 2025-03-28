#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <Arduino.h>

// Buzzer Definition
#define BUZZER_PIN 4  // Connect buzzer to GPIO4

// OLED Display Definitions
#define SCREEN_WIDTH 128   // OLED width in pixels
#define SCREEN_HEIGHT 64   // OLED height in pixels
#define OLED_RESET    -1   // Reset pin (set to -1 if not used)
#define OLED_ADDRESS  0x3C // I2C address for OLED display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// RFID Reader Definitions
#define RFID_RX 3  // ESP32 RX (Connect to RFID TX)
#define RFID_TX 1  // ESP32 TX (Connect to RFID RX)
HardwareSerial rfidSerial(2);  // Use Serial2 for RFID (ESP32 has 3 hardware serial ports)

// Add before SystemState enum if it's not already there
// Array of authorized RFID card IDs (you can add more)
// Update this in your code with the actual ID you recorded
const String authorizedCards[] = {
  "1A2B3C4D",  // Example card 1
  "5E6F7G8H",  // Example card 2
  "YOUR_NEW_CARD_ID_HERE"  // Your learned card
};
const int numAuthorizedCards = 3;  // Update this count

bool rfidSensorAvailable = false;
String lastRFIDRead = "";
// RFID definitions
bool rfidReading = false;
String rfidBuffer = "";

// Keypad Definitions
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

// Relay Definition
#define RELAY_PIN 5  // Connect relay module control pin to GPIO5

// Fingerprint Sensor Definitions
#define FINGERPRINT_RX 16  // ESP32 RX (Connect to Fingerprint TX)
#define FINGERPRINT_TX 17  // ESP32 TX (Connect to Fingerprint RX)
HardwareSerial fingerSerial(1); // Use Serial1 for fingerprint sensor
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

// System State and Variables
enum SystemState {
  STATE_LOCKED,
  STATE_UNLOCKED,
  STATE_ENROLLING,
  STATE_LEARNING_CARD
};

SystemState currentState = STATE_LOCKED;
String correctPIN = "1234";
String enteredPIN = "";
bool fingerprintSensorAvailable = false;
unsigned long doorUnlockTime = 0;
const unsigned long DOOR_UNLOCK_DURATION = 30000; // 30 seconds unlock time

// DISPLAY FUNCTIONS
// DISPLAY FUNCTIONS - Enhanced
// DISPLAY FUNCTIONS - Enhanced
void displayMessage(String message, bool showBorder = false) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (showBorder) {
    // Draw a border around the display
    display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.setCursor(3, 3);
  } else {
    display.setCursor(0, 0);
  }

  display.println(message);
  display.display();
}

// Add a centered text function
void displayCenteredText(String message, int yPosition = 16) {
  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(1);
  display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);

  // Calculate center position
  int x = (SCREEN_WIDTH - w) / 2;

  display.setCursor(x, yPosition);
  display.println(message);
}

// Enhanced main screen with battery and icons
void showMainScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Draw header with system name and divider
  display.setCursor(0, 0);
  display.println("Smart Door Lock");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  // Draw system status
  display.setCursor(0, 14);

  if (currentState == STATE_LOCKED) {
    // Draw lock icon
    display.drawRect(110, 0, 18, 8, SSD1306_WHITE);
    display.fillRect(114, 0, 10, 3, SSD1306_WHITE);

    display.println("Status: LOCKED");
    display.println("\nEnter PIN, scan");
    display.println("fingerprint or RFID");
    display.println("\nA: Fingerprint   B: Help");

    // Draw keypad hint at bottom
    display.drawLine(0, 53, SCREEN_WIDTH, 53, SSD1306_WHITE);
    display.setCursor(0, 55);
    display.print("* Clear      # Submit");
  }
  else if (currentState == STATE_UNLOCKED) {
    // Draw unlock icon
    display.drawRect(110, 0, 18, 8, SSD1306_WHITE);
    display.drawLine(114, 3, 124, 3, SSD1306_WHITE);

    display.println("Status: UNLOCKED");
    display.println("\nB: Enroll Fingerprint");
    display.println("C: Learn New Card");
    display.println("D: Lock Door Now");

    // Show remaining time with progress bar
    int remainingTime = (doorUnlockTime + DOOR_UNLOCK_DURATION - millis()) / 1000;
    int barWidth = map(remainingTime, 0, DOOR_UNLOCK_DURATION / 1000, 0, SCREEN_WIDTH - 2);

    display.drawLine(0, 53, SCREEN_WIDTH, 53, SSD1306_WHITE);
    display.setCursor(0, 55);
    display.print("Auto-lock: ");
    display.print(remainingTime);
    display.print("s");

    display.drawRect(0, 49, SCREEN_WIDTH, 3, SSD1306_WHITE);
    display.fillRect(1, 50, barWidth, 1, SSD1306_WHITE);
  }
  else if (currentState == STATE_ENROLLING) {
    // Draw fingerprint icon
    display.drawCircle(116, 4, 4, SSD1306_WHITE);
    display.drawLine(116, 4, 116, 7, SSD1306_WHITE);

    display.println("ENROLLMENT MODE");
    display.println("\nFollow instructions");
    display.println("on screen");
    display.println("\n* Cancel enrollment");
  }
  else if (currentState == STATE_LEARNING_CARD) {
    // Draw card icon
    display.drawRect(108, 1, 20, 8, SSD1306_WHITE);
    display.drawLine(108, 5, 128, 5, SSD1306_WHITE);

    display.println("CARD LEARNING MODE");
    display.println("\nPlace card on reader");
    display.println("\n* Cancel");
  }

  display.display();
}

// Enhanced PIN entry screen
void showPINEntry() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setCursor(0, 0);
  display.println("PIN Entry");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  // Draw PIN entry box
  display.drawRect(25, 20, 80, 15, SSD1306_WHITE);
  display.setCursor(30, 24);

  // Show masked PIN
  for (unsigned int i = 0; i < enteredPIN.length(); i++) {
    display.print("*");
    display.print(" ");
  }

  // Draw keypad hint at bottom
  display.drawLine(0, 53, SCREEN_WIDTH, 53, SSD1306_WHITE);
  display.setCursor(0, 55);
  display.print("* Clear      # Submit");

  display.display();
}

// Enhanced error display
void showError(String error) {
  display.clearDisplay();

  // Draw error icon (X)
  display.drawLine(5, 5, 15, 15, SSD1306_WHITE);
  display.drawLine(15, 5, 5, 15, SSD1306_WHITE);

  // Draw header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 5);
  display.println("ERROR");
  display.drawLine(0, 18, SCREEN_WIDTH, 18, SSD1306_WHITE);

  // Draw error message
  display.setCursor(5, 25);
  display.println(error);

  display.display();
  errorBeep();
  delay(2000);
  showMainScreen();
}

// Processing animation
void showProcessing(String operation, int duration = 2000) {
  unsigned long startTime = millis();
  const char spinner[] = {'|', '/', '-', '\\'};
  int spinnerPos = 0;

  while (millis() - startTime < duration) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Header
    display.setCursor(0, 0);
    display.println("Processing...");
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    // Operation text
    displayCenteredText(operation, 25);

    // Spinner animation
    display.setCursor(64, 40);
    display.print(spinner[spinnerPos]);
    spinnerPos = (spinnerPos + 1) % 4;

    display.display();
    delay(120);
  }
}

// Progress bar animation
void showProgress(String operation, int duration = 2000) {
  unsigned long startTime = millis();

  while (millis() - startTime < duration) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Header
    display.setCursor(0, 0);
    display.println(operation);
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    // Progress bar
    int progress = map(millis() - startTime, 0, duration, 0, SCREEN_WIDTH - 20);
    display.drawRect(10, 30, SCREEN_WIDTH - 20, 10, SSD1306_WHITE);
    display.fillRect(10, 30, progress, 10, SSD1306_WHITE);

    // Percentage
    int percent = map(millis() - startTime, 0, duration, 0, 100);
    display.setCursor(58, 45);
    display.print(percent);
    display.print("%");

    display.display();
    delay(50);
  }
}


// BUZZER FUNCTIONS - Improved
void shortBeep(int freq = 2000, int duration = 80) {
  // Softer, shorter beep
  tone(BUZZER_PIN, freq, duration);
  delay(duration);
  noTone(BUZZER_PIN);
}

void doubleBeep() {
  // More pleasant double beep
  tone(BUZZER_PIN, 1800, 50);
  delay(70);
  tone(BUZZER_PIN, 2200, 50);
  delay(50);
  noTone(BUZZER_PIN);
}

void successBeep() {
  // More melodic success sound (ascending)
  tone(BUZZER_PIN, 1300, 80);
  delay(100);
  tone(BUZZER_PIN, 1600, 80);
  delay(100);
  tone(BUZZER_PIN, 2000, 160);
  delay(160);
  noTone(BUZZER_PIN);
}

void errorBeep() {
  // Less harsh error sound (descending)
  tone(BUZZER_PIN, 1800, 100);
  delay(120);
  tone(BUZZER_PIN, 1200, 200);
  delay(200);
  noTone(BUZZER_PIN);
}

void warningBeep() {
  // New function for warnings
  tone(BUZZER_PIN, 1500, 80);
  delay(100);
  tone(BUZZER_PIN, 1500, 80);
  delay(100);
  tone(BUZZER_PIN, 1500, 80);
  delay(80);
  noTone(BUZZER_PIN);
}

void alarmBeep(int duration) {
  // More effective alarm sound
  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    tone(BUZZER_PIN, 2200);
    delay(80);
    tone(BUZZER_PIN, 1800);
    delay(80);
  }
  noTone(BUZZER_PIN);
}

// RFID FUNCTIONS
void initializeRFID() {
  rfidSerial.begin(9600, SERIAL_8N1, RFID_RX, RFID_TX);
  delay(100); // Give the RFID reader time to initialize

  // Flush any initial data
  while (rfidSerial.available()) {
    rfidSerial.read();
  }

  rfidSensorAvailable = true;
  Serial.println("RFID reader initialized");
  displayMessage("RFID reader\ninitialized");
  delay(1000);
}

String readRFIDCard() {
  String cardID = "";

  // Check if there's data available from the RFID reader
  if (rfidSerial.available() > 0) {
    // Debug message
    Serial.println("RFID data detected!");

    // Clear any previous data
    rfidBuffer = "";
    unsigned long startTime = millis();

    // Read all available bytes with timeout
    while ((millis() - startTime < 500) && (rfidBuffer.length() < 30)) {
      if (rfidSerial.available() > 0) {
        byte inByte = rfidSerial.read();

        // Log each byte in HEX format for debugging
        Serial.print("Received byte: 0x");
        if (inByte < 16) Serial.print("0"); // Add leading zero for single digit hex
        Serial.print(inByte, HEX);
        Serial.print(" (");
        if (inByte >= 32 && inByte <= 126) { // Printable ASCII
          Serial.print((char)inByte);
        } else {
          Serial.print(".");
        }
        Serial.println(")");

        // Add byte to buffer
        rfidBuffer += (char)inByte;
        startTime = millis(); // Reset timeout if we're receiving data
      }
      delay(5); // Small delay to prevent tight loop
    }

    // Print complete buffer for debugging
    Serial.print("Complete RFID buffer (HEX): ");
    for (unsigned int i = 0; i < rfidBuffer.length(); i++) {
      byte b = rfidBuffer[i];
      if (b < 16) Serial.print("0"); // Add leading zero for single digit hex
      Serial.print(b, HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Try different parsing methods to extract card ID

    // Method 1: Extract hexadecimal characters
    String hexID = "";
    for (unsigned int i = 0; i < rfidBuffer.length(); i++) {
      char c = rfidBuffer[i];
      if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        hexID += c;
      }
    }

    // Method 2: Try to extract last 8 bytes as card ID
    String rawID = "";
    if (rfidBuffer.length() >= 8) {
      for (int i = rfidBuffer.length() - 8; i < rfidBuffer.length(); i++) {
        if (i >= 0) {
          byte b = rfidBuffer[i];
          if (b < 16) rawID += "0"; // Add leading zero
          rawID += String(b, HEX);
        }
      }
    }

    // Method 3: Check for common RFID formats
    // Many RFID readers start with 0x02 and end with 0x03
    int startIdx = -1;
    int endIdx = -1;
    for (unsigned int i = 0; i < rfidBuffer.length(); i++) {
      if (rfidBuffer[i] == 0x02) startIdx = i;
      if (rfidBuffer[i] == 0x03 && startIdx != -1) {
        endIdx = i;
        break;
      }
    }

    String formatID = "";
    if (startIdx != -1 && endIdx != -1 && endIdx > startIdx) {
      for (int i = startIdx + 1; i < endIdx; i++) {
        byte b = rfidBuffer[i];
        if (b < 16) formatID += "0"; // Add leading zero
        formatID += String(b, HEX);
      }
    }

    // Choose the best ID from our methods
    if (hexID.length() >= 8) {
      cardID = hexID.substring(0, 8);
    } else if (rawID.length() >= 8) {
      cardID = rawID.substring(0, 8);
    } else if (formatID.length() >= 8) {
      cardID = formatID.substring(0, 8);
    } else if (rfidBuffer.length() >= 4) {
      // Last resort: just convert the raw buffer to hex
      cardID = "";
      for (unsigned int i = 0; i < min(8u, rfidBuffer.length()); i++) {
        byte b = rfidBuffer[i];
        if (b < 16) cardID += "0"; // Add leading zero
        cardID += String(b, HEX);
      }
    }

    // Ensure the card ID is uppercase
    cardID.toUpperCase();

    // If we found something that could be an ID
    if (cardID.length() >= 4) {
      Serial.println("Extracted card ID: " + cardID);
      return cardID;
    } else {
      Serial.println("No valid card ID extracted");
    }
  }

  return "";
}

void learnNewCard() {
  currentState = STATE_LEARNING_CARD;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("CARD LEARNING MODE");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
  display.setCursor(0, 15);
  display.println("Place your card on");
  display.println("the reader");

  // Draw card icon
  display.drawRect(90, 20, 30, 20, SSD1306_WHITE);
  display.drawLine(90, 30, 120, 30, SSD1306_WHITE);

  display.drawLine(0, 53, SCREEN_WIDTH, 53, SSD1306_WHITE);
  display.setCursor(0, 55);
  display.print("* Cancel");

  display.display();

  doubleBeep();
  Serial.println("Entered card learning mode");

  unsigned long startTime = millis();
  unsigned long lastUpdateTime = 0;
  int animationState = 0;

  while (millis() - startTime < 30000) { // 30 second timeout
    // Check for keypad cancel
    char key = keypad.getKey();
    if (key == '*') {
      displayMessage("Card learning\ncancelled");
      shortBeep();
      delay(1500);
      currentState = STATE_UNLOCKED;
      showMainScreen();
      return;
    }

    // Update animation every 500ms
    if (millis() - lastUpdateTime > 500) {
      display.fillRect(0, 40, SCREEN_WIDTH, 10, SSD1306_BLACK); // Clear animation area
      display.setCursor(5, 40);

      // Show waiting animation
      String waitingText = "Waiting";
      for (int i = 0; i < (animationState % 4); i++) {
        waitingText += ".";
      }
      display.println(waitingText);

      // Show elapsed time
      display.setCursor(90, 40);
      display.print((millis() - startTime) / 1000);
      display.print("s");

      display.display();

      animationState++;
      lastUpdateTime = millis();
    }

    // Ensure the serial buffer is clear
    while (rfidSerial.available()) {
      rfidSerial.read();
      delay(5);
    }

    // Try to read a card
    String cardID = readRFIDCard();

    if (cardID.length() >= 4) {
      // Check if card is already authorized
      bool alreadyAuthorized = false;
      for (int i = 0; i < numAuthorizedCards; i++) {
        if (cardID == authorizedCards[i]) {
          alreadyAuthorized = true;
          break;
        }
      }

      if (alreadyAuthorized) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("CARD ALREADY KNOWN");
        display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

        displayCenteredText("This card is already", 25);
        displayCenteredText("in the system", 35);
        displayCenteredText("ID: " + cardID, 45);

        display.display();

        warningBeep();
        delay(2000);
      } else {
        // Success animation
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.println("NEW CARD LEARNED!");
        display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

        displayCenteredText("Card ID:", 20);

        // Display card ID in a box
        display.drawRect(20, 25, SCREEN_WIDTH - 40, 15, SSD1306_WHITE);
        displayCenteredText(cardID, 32);

        displayCenteredText("Add to authorizedCards[]", 45);

        display.display();

        successBeep();
        Serial.println("New card to add: " + cardID);
        Serial.println("Add this card ID to your authorizedCards array");
        delay(3000);
      }

      currentState = STATE_UNLOCKED;
      showMainScreen();
      return;
    }

    delay(100);
  }

  // Timeout
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TIMEOUT");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  displayCenteredText("No card detected", 25);
  displayCenteredText("Please try again", 35);

  display.display();

  errorBeep();
  delay(1500);
  currentState = STATE_UNLOCKED;
  showMainScreen();
}

bool checkRFIDAuthorized(String cardID) {
  if (cardID.length() < 4) return false;  // Invalid ID

  // Check against authorized cards
  for (int i = 0; i < numAuthorizedCards; i++) {
    if (cardID == authorizedCards[i]) {
      return true;
    }
  }

  return false;
}

// Add to your checkRFID() function
void checkRFID() {
  String cardID = readRFIDCard();

  if (cardID.length() > 0) {
    Serial.println("RFID Card ID detected: " + cardID);
    shortBeep();  // Gentle beep when card is detected

    // Show animated processing
    showProcessing("Reading RFID Card", 1000);

    if (checkRFIDAuthorized(cardID)) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      // Draw card icon
      display.drawRect(54, 12, 20, 15, SSD1306_WHITE);
      display.drawLine(54, 18, 74, 18, SSD1306_WHITE);

      displayCenteredText("Card Authorized", 35);

      display.display();

      successBeep();
      delay(1000);
      unlockDoor();
    } else {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      // Draw X icon
      display.drawLine(55, 15, 65, 25, SSD1306_WHITE);
      display.drawLine(65, 15, 55, 25, SSD1306_WHITE);

      displayCenteredText("Unauthorized Card", 35);
      displayCenteredText("ID: " + cardID, 45);

      display.display();

      errorBeep();
      delay(2000);
      showMainScreen();
    }
  }
}

// DOOR LOCK FUNCTIONS
void unlockDoor() {
  // Smooth animation for door unlocking
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Draw unlocking animation
  for (int i = 0; i < 5; i++) {
    display.clearDisplay();

    // Lock icon that opens
    int offset = i * 2;
    display.drawRect(55, 15, 18, 15, SSD1306_WHITE);
    display.drawRect(59 + offset, 15, 10 - (offset * 2), 3, SSD1306_WHITE);

    displayCenteredText("ACCESS GRANTED", 35);
    display.display();
    delay(100);
  }

  // Activate the relay to unlock
  digitalWrite(RELAY_PIN, HIGH);
  currentState = STATE_UNLOCKED;
  doorUnlockTime = millis();

  // Final unlocked confirmation
  display.clearDisplay();
  displayCenteredText("DOOR UNLOCKED", 20);

  // Draw unlocked icon
  display.drawRect(55, 30, 18, 15, SSD1306_WHITE);
  display.drawLine(59, 33, 69, 33, SSD1306_WHITE);

  display.display();
  successBeep();
  delay(1500);

  showMainScreen();
}

void lockDoor() {
  // Smooth animation for door locking
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Draw locking animation
  for (int i = 0; i < 5; i++) {
    display.clearDisplay();

    // Lock icon that closes
    int offset = (4 - i) * 2;
    display.drawRect(55, 15, 18, 15, SSD1306_WHITE);
    display.drawRect(59 + offset, 15, 10 - (offset * 2), 3, SSD1306_WHITE);

    displayCenteredText("LOCKING...", 35);
    display.display();
    delay(100);
  }

  // Activate the relay to lock
  digitalWrite(RELAY_PIN, LOW);
  currentState = STATE_LOCKED;
  enteredPIN = "";

  // Final locked confirmation
  display.clearDisplay();
  displayCenteredText("DOOR LOCKED", 20);

  // Draw locked icon
  display.drawRect(55, 30, 18, 15, SSD1306_WHITE);
  display.fillRect(59, 30, 10, 3, SSD1306_WHITE);

  display.display();
  shortBeep();
  delay(1000);

  showMainScreen();
}

// AUTHENTICATION FUNCTIONS
void checkPIN() {
  if (enteredPIN == correctPIN) {
    unlockDoor();
  } else {
    displayMessage("ACCESS DENIED\n\nInvalid PIN");
    errorBeep();
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

  shortBeep();  // Gentle feedback when finger detected

  // Show animated processing
  showProcessing("Fingerprint scan", 1500);

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    showError("Image conversion failed");
    return;
  }

  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Fingerprint match found! ID #");
    Serial.println(finger.fingerID);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Draw check mark icon
    display.drawLine(50, 20, 55, 25, SSD1306_WHITE);
    display.drawLine(55, 25, 65, 15, SSD1306_WHITE);

    displayCenteredText("Fingerprint Match", 35);
    displayCenteredText("ID: " + String(finger.fingerID), 45);

    display.display();

    successBeep();
    delay(1000);
    unlockDoor();
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Draw X icon
    display.drawLine(55, 15, 65, 25, SSD1306_WHITE);
    display.drawLine(65, 15, 55, 25, SSD1306_WHITE);

    displayCenteredText("No Match Found", 35);

    display.display();

    errorBeep();
    delay(2000);
    showMainScreen();
  }
}
// FINGERPRINT ENROLLMENT

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
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ENROLLMENT - STEP 1");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  displayCenteredText("Place finger on", 20);
  displayCenteredText("the sensor", 30);

  // Draw fingerprint icon
  display.drawCircle(64, 45, 5, SSD1306_WHITE);
  display.drawLine(64, 45, 64, 50, SSD1306_WHITE);

  display.display();

  unsigned long startTime = millis();
  unsigned long lastUpdateTime = 0;
  int animationState = 0;

  while (true) {
    p = finger.getImage();

    // Check for cancel
    char key = keypad.getKey();
    if (key == '*') {
      displayMessage("Enrollment cancelled");
      shortBeep();
      delay(1500);
      currentState = STATE_UNLOCKED;
      showMainScreen();
      return;
    }

    // Update waiting animation
    if (millis() - lastUpdateTime > 500) {
      display.fillRect(0, 55, SCREEN_WIDTH, 10, SSD1306_BLACK);
      display.setCursor(5, 55);

      String waitingText = "Waiting";
      for (int i = 0; i < (animationState % 4); i++) {
        waitingText += ".";
      }
      display.println(waitingText);
      display.display();

      animationState++;
      lastUpdateTime = millis();
    }

    if (p == FINGERPRINT_OK) {
      break;
    }

    delay(100);
  }

  shortBeep();
  showProcessing("Processing fingerprint", 1000);

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    showError("Processing failed");
    currentState = STATE_UNLOCKED;
    showMainScreen();
    return;
  }

  // ----- Wait for finger removal -----
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ENROLLMENT - STEP 2");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  displayCenteredText("Remove finger", 25);
  displayCenteredText("from sensor", 35);

  display.display();

  doubleBeep();
  delay(1000);

  p = FINGERPRINT_NOFINGER;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
    delay(100);
  }

  // ----- Second Scan -----
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ENROLLMENT - STEP 3");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  displayCenteredText("Place same finger", 20);
  displayCenteredText("again", 30);

  // Draw fingerprint icon
  display.drawCircle(64, 45, 5, SSD1306_WHITE);
  display.drawLine(64, 45, 64, 50, SSD1306_WHITE);

  display.display();

  startTime = millis();
  lastUpdateTime = 0;
  animationState = 0;

  while (true) {
    p = finger.getImage();

    // Check for cancel
    char key = keypad.getKey();
    if (key == '*') {
      displayMessage("Enrollment cancelled");
      shortBeep();
      delay(1500);
      currentState = STATE_UNLOCKED;
      showMainScreen();
      return;
    }

    // Update waiting animation
    if (millis() - lastUpdateTime > 500) {
      display.fillRect(0, 55, SCREEN_WIDTH, 10, SSD1306_BLACK);
      display.setCursor(5, 55);

      String waitingText = "Waiting";
      for (int i = 0; i < (animationState % 4); i++) {
        waitingText += ".";
      }
      display.println(waitingText);
      display.display();

      animationState++;
      lastUpdateTime = millis();
    }

    if (p == FINGERPRINT_OK) {
      break;
    }

    delay(100);
  }

  shortBeep();
  showProcessing("Processing fingerprint", 1000);

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    showError("Processing failed");
    currentState = STATE_UNLOCKED;
    showMainScreen();
    return;
  }

  // ----- Create Model -----
  showProgress("Creating fingerprint model", 1500);

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("ENROLLMENT ERROR");
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    displayCenteredText("Fingerprints", 20);
    displayCenteredText("did not match", 30);
    displayCenteredText("Please try again", 40);

    display.display();

    errorBeep();
    delay(2000);
    currentState = STATE_UNLOCKED;
    showMainScreen();
    return;
  }

  // ----- Store Model -----
  showProgress("Saving fingerprint", 1000);

  p = finger.storeModel(id);
  if (p != FINGERPRINT_OK) {
    showError("Failed to save");
    currentState = STATE_UNLOCKED;
    showMainScreen();
    return;
  }

  // Success!
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ENROLLMENT SUCCESS");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  // Draw check mark
  display.drawLine(55, 25, 60, 30, SSD1306_WHITE);
  display.drawLine(60, 30, 70, 20, SSD1306_WHITE);

  displayCenteredText("Fingerprint saved", 40);
  displayCenteredText("as ID #" + String(id), 50);

  display.display();

  successBeep();
  delay(3000);

  currentState = STATE_UNLOCKED;
  showMainScreen();
}

// KEY HANDLING
void processUnlockedStateKey(char key) {
  switch (key) {
    case 'B':
      // Start fingerprint enrollment
      if (fingerprintSensorAvailable) {
        startEnrollment();
      } else {
        showError("Fingerprint not available");
      }
      break;

    case 'C':
      // Start card learning
      Serial.println("C key pressed - Starting card learning mode");
      if (rfidSensorAvailable) {
        learnNewCard();
      } else {
        showError("RFID not available");
      }
      break;

    case 'D':
      // Lock door immediately
      Serial.println("D key pressed - Locking door");
      displayMessage("Locking door...");
      delay(1000);
      lockDoor();
      break;

    default:
      // Print for debugging
      Serial.print("Unrecognized key pressed in unlocked state: ");
      Serial.println(key);
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
      displayMessage("HELP:\n1. Enter PIN, use finger\n   or RFID card\n2. * to clear PIN\n3. # to submit PIN\n4. A for fingerprint\n5. B for help");
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

void processKey(char key) {
  Serial.print("Key pressed: ");
  Serial.println(key);

  shortBeep();

  // Global key handlers
  if (key == '*' && currentState != STATE_ENROLLING) {
    // Clear PIN
    enteredPIN = "";
    showPINEntry();
    return;
  }

  // Debug current state
  Serial.print("Current state: ");
  if (currentState == STATE_LOCKED) Serial.println("LOCKED");
  else if (currentState == STATE_UNLOCKED) Serial.println("UNLOCKED");
  else if (currentState == STATE_ENROLLING) Serial.println("ENROLLING");
  else if (currentState == STATE_LEARNING_CARD) Serial.println("LEARNING_CARD");

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

    case STATE_LEARNING_CARD:
      // Keys for card learning are handled in the learnNewCard function
      break;
  }
}

// SYSTEM FUNCTIONS

void checkDoorTimeout() {
  if (currentState == STATE_UNLOCKED) {
    if (millis() - doorUnlockTime >= DOOR_UNLOCK_DURATION) {
      displayMessage("Auto-locking door...");
      doubleBeep();
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

  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  shortBeep();  // Initial beep to indicate system is starting

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

  // Initialize RFID Reader
  displayMessage("Checking RFID...");
  initializeRFID();

  showMainScreen();
}

void loop() {
  // 1. Check for key presses
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key detected in loop: ");
    Serial.println(key);
    processKey(key);
  }

  // 2. Check fingerprint sensor if door is locked
  if (currentState == STATE_LOCKED && fingerprintSensorAvailable) {
    checkFingerprint();
  }

  // 3. Check RFID reader if door is locked or in card learning mode
  if ((currentState == STATE_LOCKED || currentState == STATE_LEARNING_CARD) && rfidSensorAvailable) {
    checkRFID();
  }

  // 4. Check if door needs to be auto-locked
  checkDoorTimeout();

  // Small delay for stability
  delay(50);
}
