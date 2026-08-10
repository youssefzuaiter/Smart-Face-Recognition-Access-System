// =================================================================
//   SMART ACCESS - OPTION B (face decides) - FULL CODE
//   Sensor -> asks laptop camera -> known opens, unknown denied.
//   Card + remote + all features kept as backups.
// =================================================================
#include <Wire.h>              // I2C communication (used by the LCD)
#include <LiquidCrystal_I2C.h> // controls the 16x2 I2C LCD screen
#include <Servo.h>             // controls the servo motor (the door)
#include <SPI.h>               // SPI communication (used by the NFC reader)
#include <MFRC522.h>           // controls the RC522 NFC card reader
#include <IRremote.hpp>        // reads the infrared remote control

const int TRIG        = 4;   // ultrasonic Trig pin (sends the pulse)
const int ECHO        = 5;   // ultrasonic Echo pin (hears the return)
const int GREEN_LED   = 6;   // green LED pin (access granted)
const int RED_LED     = 7;   // red LED pin (locked / denied)
const int BUZZER      = 8;   // buzzer pin (sound feedback)
const int SERVO_PIN   = 3;   // servo signal pin (door motor)
const int IR_PIN      = 2;   // IR receiver signal pin (remote)
const int RELAY       = A0;  // relay pin (real lock / lamp switch)
const int VIBRATION   = A1;  // vibration sensor pin (tamper alarm)
const int LDR_PIN     = A2;  // light sensor pin (day/night)
#define RST_PIN       9      // NFC reader reset pin
#define SS_PIN        10     // NFC reader chip-select (SDA) pin

const int DETECT_CM   = 15;  // person must be closer than this (cm) to trigger
const int CLEAR_CM    = 22;  // beyond this (cm) the zone counts as clear
const int GATE_OPEN   = 90;  // servo angle when the door is open
const int GATE_CLOSED = 0;   // servo angle when the door is closed
const int NIGHT_LEVEL = 400; // light reading below this = night
const unsigned long FACE_WAIT_MS = 6000;   // how long to wait for laptop's answer

const int BTN_ARM       = 0x1C;   // remote Power button code (arm/disarm)
const int BTN_EMERGENCY = 0x46;   // remote Num2 button code (emergency open)
const int BTN_RESET     = 0x45;   // NUM1 resets visitor counter

byte knownCard[4]     = {0xE3, 0x6C, 0x9B, 0x38};  // saved UID of the allowed card
byte knownKeychain[4] = {0xC2, 0x20, 0xBD, 0xAB};  // saved UID of the allowed keychain

LiquidCrystal_I2C lcd(0x27, 16, 2);   // LCD object: I2C address 0x27, 16 cols, 2 rows
Servo gateServo;                      // servo object that moves the door
MFRC522 rfid(SS_PIN, RST_PIN);        // NFC reader object on its SS and RST pins

bool personWasHere   = false;   // true while someone is standing in the detect zone
bool systemArmed     = true;    // true = system active, false = frozen/off
unsigned long lastPrint      = 0;   // timer for printing status to Serial
unsigned long lastCardCheck  = 0;   // timer for how often we check the NFC reader
unsigned long lastTamper     = 0;   // timer for the tamper-alarm cooldown
unsigned long disarmTime     = 0;   // time when the system was disarmed (for auto-rearm)
unsigned long beamStartTime  = 0;   // time the person first appeared (gate-timeout check)
unsigned long lcdAltTimer    = 0;   // timer for the alternating idle LCD display
bool lcdShowClock            = true; // toggles LCD between clock and last-visitor view
int visitorCount  = 0;   // how many people have entered (running total)
int lastVisitor   = 0;   // the number of the most recent visitor

void setup() {                       // runs once when the Arduino powers on
  Serial.begin(9600);                // start serial link to the laptop at 9600 baud
  pinMode(TRIG,      OUTPUT);        // Trig sends a signal out
  pinMode(ECHO,      INPUT);         // Echo listens for a signal in
  pinMode(GREEN_LED, OUTPUT);        // green LED is an output
  pinMode(RED_LED,   OUTPUT);        // red LED is an output
  pinMode(BUZZER,    OUTPUT);        // buzzer is an output
  pinMode(RELAY,     OUTPUT);        // relay is an output
  pinMode(VIBRATION, INPUT);         // vibration sensor is an input

  digitalWrite(RELAY,     LOW);      // start with the lock/lamp off
  digitalWrite(RED_LED,   HIGH);     // red LED on = system is locked/idle
  digitalWrite(GREEN_LED, LOW);      // green LED off at start

  gateServo.attach(SERVO_PIN);       // connect the servo object to its pin
  gateServo.write(GATE_CLOSED);      // move the door to the closed position
  delay(500);                        // give the servo time to reach position

  SPI.begin();                       // start the SPI bus for the NFC reader
  rfid.PCD_Init();                   // initialise the NFC reader
  delay(50);                         // short pause so the reader settles
  rfid.PCD_AntennaOn();              // turn the reader's antenna on (reliable reads)

  IrReceiver.begin(IR_PIN, DISABLE_LED_FEEDBACK); // start IR receiver, no blink LED

  lcd.init();                        // initialise the LCD
  lcd.backlight();                   // turn the LCD backlight on
  lcd.setCursor(0, 0);               // move to row 0, column 0
  lcd.print("Smart Access");         // show the project name
  lcd.setCursor(0, 1);               // move to row 1
  lcd.print("Starting...");          // show a startup message
  beep(1);                           // one beep to signal power-on
  delay(2000);                       // hold the startup screen for 2 seconds
  showReady();                       // switch to the idle "ready" screen

  Serial.println("=== SMART ACCESS - OPTION B (face decides) ==="); // log start
}

void loop() {                        // runs over and over, forever
  checkRemote();                     // 1) react to any remote button press
  checkTamper();                     // 2) check for someone forcing the door
  checkAutoRearm();                  // 3) re-arm the system if the freeze time passed
  updateReadyScreen();               // 4) refresh the idle LCD (clock / last visitor)

  if (millis() - lastCardCheck > 600) { // only check the card every 600 ms
    checkCard();                     // 5) read the NFC reader (in short bursts)
    lastCardCheck = millis();        // remember when we last checked
  }

  if (!systemArmed) return;          // if the system is frozen, skip the rest

  long d = readDistanceCM();         // measure distance to anything in front

  if (millis() - lastPrint > 400) {  // print status to Serial about 2-3 times a sec
    Serial.print("Distance: ");      // label
    if (d == 0) Serial.print("--- (timeout)");      // 0 means nothing detected
    else { Serial.print(d); Serial.print(" cm"); }  // otherwise show the distance
    Serial.print("   |   Light: ");  // label for the light reading
    Serial.print(analogRead(LDR_PIN)); // current light value
    Serial.print(isNight() ? " (night)" : " (day)"); // day or night
    Serial.print("   |   State: ");  // label for the state
    Serial.println(personWasHere ? "BUSY" : "clear"); // busy if someone is present
    lastPrint = millis();            // remember when we last printed
  }

  if (d > 0 && d < DETECT_CM) {       // someone is within the detect distance
    if (!personWasHere) {            // and they JUST arrived (not already counted)
      personWasHere = true;          // mark that a person is now present
      beamStartTime = millis();      // start the gate-timeout timer
      Serial.println(">>> BEAM BROKEN - asking laptop for face check"); // log
      askLaptopForFace();            // Option B: face decides
    } else {                         // they are still standing here
      if (millis() - beamStartTime > 10000) {  // blocked for over 10 seconds
        Serial.println(">> GATE TIMEOUT - beam blocked too long!"); // log warning
        lcd.clear();                 // clear the screen
        lcd.setCursor(0, 0);         // row 0
        lcd.print("Gate timeout!");  // warn on the LCD
        lcd.setCursor(0, 1);         // row 1
        lcd.print("Clear the door"); // ask them to move
        beep(3);                     // three warning beeps
        beamStartTime = millis();    // reset the timeout timer
      }
    }
  } else if (d == 0 || d > CLEAR_CM) {   // nothing there, or person has stepped away
    if (personWasHere) {             // if we thought someone was here
      personWasHere = false;         // mark the zone as clear again
      beamStartTime = 0;             // reset the timeout timer
      Serial.println(">>> Zone clear"); // log
    }
  }
}

// ===== OPTION B: ask the laptop to recognise the face =====
void askLaptopForFace() {            // sends "D" and waits for the laptop's answer
  while (Serial.available()) Serial.read();   // clear old junk

  Serial.println("D");   // tell Python: person here, check the face

  lcd.clear();                       // clear the screen
  lcd.setCursor(0, 0);               // row 0
  lcd.print("Checking face..");      // tell the user we're checking
  lcd.setCursor(0, 1);               // row 1
  lcd.print("Please look up");       // ask them to face the camera

  unsigned long startWait = millis();  // record when the wait started
  char answer = 0;                   // will hold 'F', 'U', or stay 0 if no reply
  while (millis() - startWait < FACE_WAIT_MS) {  // wait up to 6 seconds
    checkRemote();                   // still react to the remote while waiting
    if (millis() - lastCardCheck > 600) { checkCard(); lastCardCheck = millis(); } // still allow card

    if (Serial.available() > 0) {    // did the laptop send something?
      char c = Serial.read();        // read one character
      if (c == 'F' || c == 'U') { answer = c; break; } // keep only F or U, then stop
    }
  }

  if (answer == 'F') {               // laptop recognised a known face
    Serial.println("   Laptop: KNOWN face"); // log
    grantAccess();                   // open the door
  } else if (answer == 'U') {        // laptop saw an unknown face
    Serial.println("   Laptop: UNKNOWN face"); // log
    denyFace();                      // keep it locked
  } else {                           // no reply at all (Python not running?)
    Serial.println("   Laptop: no answer (is Python running?)"); // log
    lcd.clear();                     // clear the screen
    lcd.setCursor(0, 0);             // row 0
    lcd.print("No camera reply");    // tell the user
    beep(2);                         // two beeps to signal no answer
    delay(1500);                     // hold the message briefly
    personWasHere = false;           // reset so it can try again
    showReady();                     // back to idle
  }
}

void denyFace() {                    // response when the face is unknown
  digitalWrite(GREEN_LED, LOW);      // green off
  digitalWrite(RED_LED, HIGH);       // red on (denied)
  lcd.clear();                       // clear screen
  lcd.setCursor(0, 0);               // row 0
  lcd.print("Access Denied");        // message line 1
  lcd.setCursor(0, 1);               // row 1
  lcd.print("Unknown face");         // message line 2
  beep(3);                           // three beeps = denied
  delay(2000);                       // hold the message
  personWasHere = false;             // reset presence
  showReady();                       // back to idle
}

void grantAccess() {                 // response when entry is allowed
  visitorCount++;                    // add one to the visitor total
  lastVisitor = visitorCount;        // remember this visitor's number
  Serial.print(">> GRANTED #"); Serial.println(visitorCount); // log

  digitalWrite(GREEN_LED, HIGH);     // green on (granted)
  digitalWrite(RED_LED,   LOW);      // red off
  digitalWrite(RELAY,     HIGH);     // turn the lock/lamp on (unlocked)

  lcd.clear();                       // clear screen
  lcd.setCursor(0, 0);               // row 0
  lcd.print("Welcome!");             // greeting
  lcd.setCursor(0, 1);               // row 1
  if (isNight()) {                   // if it is dark
    lcd.print("Night entry");        // show night label
    Serial.println("   Night mode"); // log
  } else {                           // otherwise (daytime)
    lcd.print("Visitors: ");         // show the count label
    lcd.print(visitorCount);         // and the number
    Serial.println("   Day mode");   // log
  }

  beep(1);                           // one welcome beep
  openGate();                        // swing the door open
  delay(2000);                       // keep it open for 2 seconds
  closeGate();                       // close the door

  digitalWrite(RELAY,     LOW);      // turn the lock/lamp back off
  personWasHere = false;             // reset presence
  digitalWrite(GREEN_LED, LOW);      // green off
  digitalWrite(RED_LED,   HIGH);     // red on (locked again)
  showReady();                       // back to idle
  Serial.println(">>> GATE CLOSED - lock engaged"); // log
}

void checkCard() {                   // reads the NFC reader and decides
  IrReceiver.stop();                 // pause IR so it doesn't clash with NFC/SPI

  if (!rfid.PICC_IsNewCardPresent()) {  // no card near the reader?
    IrReceiver.start();              // resume IR
    return;                          // nothing to do
  }
  if (!rfid.PICC_ReadCardSerial()) {    // couldn't read the card's serial?
    IrReceiver.start();              // resume IR
    return;                          // give up this round
  }

  Serial.print(">> CARD UID: ");     // start printing the card's ID
  for (byte i = 0; i < rfid.uid.size; i++) {  // loop over each ID byte
    if (rfid.uid.uidByte[i] < 16) Serial.print("0"); // pad single digits with 0
    Serial.print(rfid.uid.uidByte[i], HEX);   // print the byte in hex
    Serial.print(" ");               // space between bytes
  }
  Serial.println();                  // end the line

  if (matchUID(knownCard) || matchUID(knownKeychain)) {  // is it a known tag?
    Serial.println("   KNOWN - access granted"); // log
    lcd.clear();                     // clear screen
    lcd.setCursor(0, 0);             // row 0
    lcd.print("Card accepted");      // message
    IrReceiver.start();              // resume IR before opening
    grantAccess();                   // open the door
  } else {                           // unknown tag
    Serial.println("   UNKNOWN - rejected"); // log
    lcd.clear();                     // clear screen
    lcd.setCursor(0, 0);             // row 0
    lcd.print("Access Denied");      // message line 1
    lcd.setCursor(0, 1);             // row 1
    lcd.print("Unknown card");       // message line 2
    digitalWrite(RED_LED, HIGH);     // red on
    beep(3);                         // three beeps = rejected
    delay(1500);                     // hold the message
    IrReceiver.start();              // resume IR
    showReady();                     // back to idle
  }

  rfid.PICC_HaltA();                 // tell the card to stop talking
  rfid.PCD_StopCrypto1();            // end the secure session with the card
}

void checkTamper() {                 // detects someone forcing/shaking the door
  if (millis() - lastTamper < 10000) return;  // ignore for 10s after last alarm

  int count = 0;                     // counts how many samples felt a shake
  for (int i = 0; i < 30; i++) {     // take 30 quick samples
    if (analogRead(VIBRATION) > 400) count++;  // count strong readings
    delay(5);                        // tiny gap between samples
  }

  if (count >= 15) {                 // if at least half the samples were strong
    lastTamper = millis();           // start the cooldown
    Serial.println(">> TAMPER DETECTED!"); // log
    lcd.clear();                     // clear screen
    lcd.setCursor(0, 0);             // row 0
    lcd.print("!! TAMPER !!");       // alarm message line 1
    lcd.setCursor(0, 1);             // row 1
    lcd.print("Alarm!");             // alarm message line 2
    digitalWrite(RED_LED, HIGH);     // red on
    beep(2);                         // alarm beeps
    digitalWrite(RED_LED, LOW);      // red off
    delay(1000);                     // brief pause
    showReady();                     // back to idle
  }
}

void checkRemote() {                 // reads and acts on remote button presses
  if (IrReceiver.decode()) {         // did a button signal arrive?
    int code = IrReceiver.decodedIRData.command;  // get the button's code
    Serial.print(">> REMOTE: 0x"); Serial.println(code, HEX); // log the code

    if (code == BTN_ARM) {           // Power button: toggle armed/off
      systemArmed = !systemArmed;    // flip the armed state
      lcd.clear();                   // clear screen
      lcd.setCursor(0, 0);           // row 0
      if (systemArmed) {             // now armed
        lcd.print("System ARMED");   // message
        digitalWrite(RED_LED,   HIGH); // red on (locked)
        digitalWrite(GREEN_LED, LOW);  // green off
        digitalWrite(RELAY,     LOW);  // lock off
        beep(1);                     // one beep
        Serial.println("   System ARMED"); // log
      } else {                       // now off/frozen
        lcd.print("System OFF");     // message line 1
        lcd.setCursor(0, 1);         // row 1
        lcd.print("30s auto rearm"); // message line 2
        digitalWrite(RED_LED,   LOW);  // red off
        digitalWrite(GREEN_LED, LOW);  // green off
        digitalWrite(RELAY,     LOW);  // lock off
        disarmTime = millis();       // start the auto-rearm countdown
        beep(2);                     // two beeps
        Serial.println("   System OFF - auto rearm in 30s"); // log
      }
      delay(1200);                   // hold the message
      if (systemArmed) showReady();  // if armed, return to idle screen
    }

    else if (code == BTN_EMERGENCY) {  // Num2: open the door manually
      Serial.println("   EMERGENCY OPEN"); // log
      lcd.clear();                   // clear screen
      lcd.setCursor(0, 0);           // row 0
      lcd.print("Emergency open");   // message
      digitalWrite(RELAY, HIGH);     // unlock
      beep(1);                       // one beep
      openGate();                    // open the door
      delay(4000);                   // keep open 4 seconds
      closeGate();                   // close the door
      digitalWrite(RELAY, LOW);      // lock again
      showReady();                   // back to idle
    }

    else if (code == BTN_RESET) {    // Num1: reset the visitor counter
      visitorCount = 0;              // zero the running total
      lastVisitor  = 0;              // zero the last-visitor number
      Serial.println("   Visitor counter reset"); // log
      lcd.clear();                   // clear screen
      lcd.setCursor(0, 0);           // row 0
      lcd.print("Counter reset");    // message line 1
      lcd.setCursor(0, 1);           // row 1
      lcd.print("Visitors: 0");      // message line 2
      delay(1500);                   // hold the message
      showReady();                   // back to idle
    }

    IrReceiver.resume();             // get the receiver ready for the next button
  }
}

void checkAutoRearm() {              // turns the system back on after a freeze
  if (!systemArmed && disarmTime > 0) {  // only if currently off and a timer is set
    if (millis() - disarmTime > 30000) {  // 30 seconds have passed
      systemArmed = true;            // re-arm the system
      disarmTime  = 0;               // clear the timer
      Serial.println(">> AUTO REARM"); // log
      lcd.clear();                   // clear screen
      lcd.setCursor(0, 0);           // row 0
      lcd.print("Auto rearmed!");    // message
      digitalWrite(RED_LED, HIGH);   // red on (locked)
      beep(1);                       // one beep
      delay(1200);                   // hold the message
      showReady();                   // back to idle
    }
  }
}

void updateReadyScreen() {           // refreshes the idle screen every few seconds
  if (!systemArmed) return;          // skip if the system is off
  if (personWasHere) return;         // skip if someone is being handled
  if (millis() - lcdAltTimer < 4000) return;  // only update every 4 seconds
  lcdAltTimer = millis();            // remember this update time
  lcdShowClock = !lcdShowClock;      // switch between the two views

  lcd.clear();                       // clear screen
  lcd.setCursor(0, 0);               // row 0
  lcd.print("System Ready");         // top line stays the same
  lcd.setCursor(0, 1);               // row 1 changes below

  if (lcdShowClock) {                // view 1: the uptime clock
    unsigned long totalSec = millis() / 1000;  // seconds since power-on
    unsigned long mins     = totalSec / 60;    // whole minutes
    unsigned long secs     = totalSec % 60;    // leftover seconds
    if (isNight()) {                 // at night, show a short label instead
      lcd.print("Night ");           // night text
    } else {                         // daytime: show the clock
      lcd.print("Up: ");             // label
      if (mins < 10) lcd.print("0"); // leading zero for minutes
      lcd.print(mins);               // minutes
      lcd.print(":");                // colon separator
      if (secs < 10) lcd.print("0"); // leading zero for seconds
      lcd.print(secs);               // seconds
    }
  } else {                           // view 2: the last visitor
    if (lastVisitor == 0) {          // no one has entered yet
      lcd.print("No entries yet");   // show that
    } else {                         // someone has entered
      lcd.print("Last: visitor #");  // label
      lcd.print(lastVisitor);        // their number
    }
  }
}

bool isNight() {                     // returns true when it is dark
  return (analogRead(LDR_PIN) < NIGHT_LEVEL);  // low light reading = night
}

bool matchUID(byte saved[4]) {       // compares a scanned card to a saved ID
  for (byte i = 0; i < 4; i++)       // check each of the 4 bytes
    if (rfid.uid.uidByte[i] != saved[i]) return false;  // any mismatch = not equal
  return true;                       // all 4 matched = same card
}

void showReady() {                   // shows the basic idle "ready" screen
  lcdAltTimer = 0;                   // reset the alternating-display timer
  lcdShowClock = true;               // start the next idle view on the clock
  lcd.clear();                       // clear screen
  lcd.setCursor(0, 0);               // row 0
  lcd.print("System Ready");         // top line
  lcd.setCursor(0, 1);               // row 1
  lcd.print(isNight() ? "Night mode" : "Scanning...");  // night or scanning text
}

void beep(int times) {               // makes the buzzer beep a given number of times
  for (int t = 0; t < times; t++) {  // repeat for each beep
    for (int i = 0; i < 150; i++) {  // build one tone by pulsing the pin fast
      digitalWrite(BUZZER, HIGH);    // pin high
      delayMicroseconds(500);        // wait 0.5 ms
      digitalWrite(BUZZER, LOW);     // pin low
      delayMicroseconds(500);        // wait 0.5 ms (this pair sets the pitch)
    }
    delay(120);                      // short gap between beeps
  }
}

void openGate() {                    // moves the servo from closed to open
  int spd = isNight() ? 20 : 12;     // slower step delay at night, faster by day
  for (int a = GATE_CLOSED; a <= GATE_OPEN; a++) {  // step the angle up
    gateServo.write(a);              // move to the new angle
    delay(spd);                      // wait so the motion is smooth
  }
}

void closeGate() {                   // moves the servo from open to closed
  int spd = isNight() ? 20 : 12;     // same speed rule as opening
  for (int a = GATE_OPEN; a >= GATE_CLOSED; a--) {  // step the angle down
    gateServo.write(a);              // move to the new angle
    delay(spd);                      // wait so the motion is smooth
  }
}

long readDistanceCM() {              // measures distance with the ultrasonic sensor
  digitalWrite(TRIG, LOW);           // make sure Trig starts low
  delayMicroseconds(2);              // settle for 2 microseconds
  digitalWrite(TRIG, HIGH);          // send the pulse
  delayMicroseconds(10);             // hold it high for 10 microseconds
  digitalWrite(TRIG, LOW);           // end the pulse
  long duration = pulseIn(ECHO, HIGH, 30000);  // time the echo (give up after 30 ms)
  if (duration == 0) return 0;       // 0 means nothing was detected
  return duration * 0.034 / 2;       // convert echo time to centimetres
}