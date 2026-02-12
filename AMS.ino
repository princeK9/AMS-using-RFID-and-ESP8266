/*
  ===================================================================
  == MPMC ATTENDANCE SYSTEM - MASTER CODE
  ===================================================================
*/
#include <SPI.h>
#include "FS.h" 
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <MFRC522.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h> // <-- ADDED FOR UPLOADING
#include <WiFiClientSecure.h> 
#include <Wire.h>
#include <LiquidCrystal_I2C.h> 
#include <RTClib.h> // RTC 

#define SDA_PIN 0 // GPIO 0
#define SCL_PIN 2 // GPIO 2

const int SWITCH_PIN = A0;

const int V0 = 12;   
const int V1 = 300; 
const int V2 = 567;  
const int V3 = 800; 

int s0 = 0;
int s1 = 0;
int Mode = 0;
int a0Value=0;

LiquidCrystal_I2C lcd(0x27, 16, 2); //0x27 is the address of the display
RTC_DS3231 rtc;

String SCRIPT_URL = "https://script.google.com/macros/s/AKfycbw0fWt8ArwMV9eePDYXHn6o3b5Un7cRWFC4jo9woQdiTmYcsjVN22H9VRwBQ3JBFFsn/exec";

const char* WIFI_SSID = "GalaxyA14";
const char* WIFI_PASS = "pk123456";
WiFiClientSecure client;
#define SS_PIN  4 
#define RST_PIN 5
MFRC522 mfrc522(SS_PIN, RST_PIN);
const char* ap_ssid = "Attendance_Setup";
const char* ap_password = "12345678";
ESP8266WebServer server(80);
DNSServer dnsServer;
const char* dnsName = "configAttendance";
enum State { STATE_WAITING_FOR_CARD, STATE_WAITING_FOR_INPUT, STATE_DUPLICATE_ERROR };
State currentState = STATE_WAITING_FOR_CARD;
String globalScannedUID = "";
unsigned long errorTimer = 0; // Timer for the error message
bool hotspotcon = false;
bool wifi = false;
// bool connected = false;
bool connectToWifiState = false;
bool visitWebsiteState =false;
bool tapPrinted = false;
bool visitedWebsite = false;
bool new_disp = false;

// ===================================================================
// == GLOBAL VARIABLES - MODE 3 (Upload DB)
// ===================================================================
// flag to ensure we only run the upload ONCE per boot
bool upload_in_progress = true; // Flag for mode 3 
bool log_upload_in_progress = true; // Flag for Mode 2 upload

//attendance logger variables
unsigned long lastScanTime_M1 = 0;
const unsigned long scanCooldown_M1 = 3000; // 3-second cooldown

bool inCooldown_M1 = false;


// ===================================================================
//    HELPER FUNCTIONS - MODE 0 (Unchanged)
// ===================================================================
void findMode(){
  const long THRESHOLD_1 = (long(V0 + V1)) / 2;
  const long THRESHOLD_2 = (long(V1) + V2) / 2;
  const long THRESHOLD_3 = (long(V2) + V3) / 2;
  long sum = 0;
  int readings = 10;
  for (int i = 0; i < readings; i++) {
    sum += analogRead(SWITCH_PIN);
    delay(10);
  }
  a0Value = sum / readings;
  if (a0Value < THRESHOLD_1){
    s0=0;
    s1=0;
    Mode=0;
  }
  else if (a0Value < THRESHOLD_2) {
    Mode=1;
    s0=0;
    s1=1;
  }
  else if (a0Value < THRESHOLD_3) {
    Mode=2;
    s0=1;
    s1=0;
  }
  else {
    Mode = 3;
    s0=1;
    s1=1;
  }
}
String getUIDString(byte *buffer, byte bufferSize) {
  String uid = "";
  for (byte i = 0; i < bufferSize; i++) {
    if (buffer[i] < 0x10) uid += "0";
    uid += String(buffer[i], HEX);
    if (i < bufferSize - 1) uid += ":";
  }
  uid.toUpperCase();
  return uid;
}
bool isUidRegistered(String uid) {
  Serial.print("Checking database for UID: ");
  Serial.println(uid);
  File lookupFile = SPIFFS.open("/lookup.csv", "r");
  if (!lookupFile) { Serial.println("lookup.csv not found."); return false; }
  while (lookupFile.available()) {
    String line = lookupFile.readStringUntil('\n');
    line.trim();
    int comma1 = line.indexOf(',');
    if (comma1 != -1) {
      if (line.substring(0, comma1).equals(uid)) {
        Serial.println("Duplicate found!");
        lookupFile.close();
        return true;
      }
    }
  }
  Serial.println("No duplicate found.");
  lookupFile.close();
  return false;
}

void handleRoot() {

  if (!visitedWebsite) {
    visitedWebsite = true;
  }

  String html = "<html><head><title>Registration</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  if (currentState == STATE_WAITING_FOR_CARD) {
    html += "<meta http-equiv='refresh' content='3'>";
  } else if (currentState == STATE_DUPLICATE_ERROR) {
    html += "<meta http-equiv='refresh' content='5;url=/'>";
  }
  html += "<style>";
  html += "body{font-family: Arial, sans-serif; background-color: #f4f4f4; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0;}";
  html += ".container{background-color: #fff; max-width: 400px; width: 90%; padding: 25px; box-shadow: 0 6px 15px rgba(0,0,0,0.1); border-radius: 10px; text-align: center;}";
  html += "h1{color: #333; margin-top: 0; font-size: 24px;}";
  html += "h2{color: #555; font-size: 18px; font-weight: normal;}";
  html += ".dots-container { display: flex; justify-content: center; align-items: center; padding: 20px 0; }";
  html += ".dot { width: 15px; height: 15px; margin: 5px; background-color: #3498db; border-radius: 50%; animation: pulse 1.4s infinite ease-in-out both; }";
  html += ".dot:nth-child(1) { animation-delay: -0.32s; }";
  html += ".dot:nth-child(2) { animation-delay: -0.16s; }";
  html += "@keyframes pulse { 0%, 80%, 100% { transform: scale(0.3); opacity: 0.5; } 40% { transform: scale(1.0); opacity: 1; } }";
  html += "form{margin-top: 20px;}";
  html += "input[type='text']{width: calc(100% - 20px); padding: 12px 10px; margin-bottom: 15px; border: 1px solid #ccc; border-radius: 5px; font-size: 16px;}";
  html += "input[type='submit']{width: 100%; padding: 12px; background-color: #007bff; color: white; border: 0; border-radius: 5px; font-size: 16px; cursor: pointer; transition: background-color 0.3s;}";
  html += "input[type='submit']:hover{background-color: #0056b3;}";
  html += ".uid{font-weight: bold; color: #0056b3; font-size: 1.1em; word-wrap: break-word;}";
  html += ".error h1 {color: #dc3545;}";
  html += "</style>";
  html += "</head><body>";
  if (currentState == STATE_DUPLICATE_ERROR) {
    html += "<div class='container error'>";
  } else {
    html += "<div class='container'>";
  }
  html += "<h1>MPMC Attendance Configuration Setup</h1>";
  if (currentState == STATE_WAITING_FOR_CARD) {
    html += "<h2>Please scan a new student card...</h2>";
    html += "<div class='dots-container'><div class='dot'></div><div class='dot'></div><div class='dot'></div></div>";
  } else if (currentState == STATE_WAITING_FOR_INPUT) {
    html += "<h2>Card Scanned:</h2>";
    html += "<p class='uid'>" + globalScannedUID + "</p>";
    html += "<p>Enter details for this card:</p>";
    html += "<form action='/save' method='POST'>";
    html += "<input type='hidden' name='uid' value='" + globalScannedUID + "'>";
    html += "<input type='text' name='roll' placeholder='Enter Roll Number' required><br>";
    html += "<input type='text' name='name' placeholder='Enter Student Name' required><br>";
    html += "<input type='submit' value='Save Student'>";
    html += "</form>";
  } else if (currentState == STATE_DUPLICATE_ERROR) {
    html += "<h1>Error: Card Already Registered</h1>";
    html += "<p>The card with UID:</p>";
    html += "<p class='uid'>" + globalScannedUID + "</p>";
    html += "<p>...is already in the database.</p>";
    html += "<p>Returning to scanner in a moment...</p>";
  }

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}
void handleSave() {
  if (server.hasArg("uid") && server.hasArg("roll") && server.hasArg("name")) {
    String uid = server.arg("uid");
    String roll = server.arg("roll");
    String name = server.arg("name");
    File lookupFile = SPIFFS.open("/lookup.csv", "a");
    if (lookupFile) {
      String dataLine = uid + "," + roll + "," + name + "\n";
      lookupFile.print(dataLine);
      lookupFile.close();
      Serial.println("Student saved: " + dataLine);
    } else {
      Serial.println("Error opening /lookup.csv for writing");
    }
    String html = "<html><head><title>Saving...</title>";
    html += "<meta http-equiv='refresh' content='2;url=/'></head>";
    html += "<style>";
    html += "body{font-family: Arial, sans-serif; background-color: #f4f4f4; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0;}";
    html += ".container{background-color: #fff; max-width: 400px; width: 90%; padding: 25px; box-shadow: 0 6px 15px rgba(0,0,0,0.1); border-radius: 10px; text-align: center;}";
    html += "h1{color: #28a745;}";
    html += "</style>";
    html += "</head><body><div class='container'>";
    html += "<h1>Success!</h1>";
    html += "<h2>Saved " + name + "</h2>";
    html += "<p>Returning to registration screen...</p>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
    currentState = STATE_WAITING_FOR_CARD;

    displayOnLCD("Tap Your ID Card","");
    globalScannedUID = "";
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

// ===================================================================
//    Helper functions for mode 1.
// ===================================================================
int checkTapStatus(String uid, DateTime now) {
  File attendanceFile = SPIFFS.open("/attendance.csv", "r");
  if (!attendanceFile) {
    Serial.println("attendance.csv not found.");
    return 0; // File doesn't exist, so count is 0
  }

  int count = 0;
  long mostRecentTapEpoch = 0; // To store the epoch time of the last tap
  long nowEpoch = now.unixtime(); // Current time as a number

  while (attendanceFile.available()) {
    String line = attendanceFile.readStringUntil('\n');
    line.trim();

    int comma1 = line.indexOf(',');
    if (comma1 == -1) continue; 
    String lineUid = line.substring(0, comma1);

    // --- Check if UID matches ---
    if (lineUid.equals(uid)) {
      int comma3 = line.indexOf(',', line.indexOf(',', comma1 + 1) + 1);
      int comma4 = line.indexOf(',', comma3 + 1);
      if (comma3 == -1 || comma4 == -1) continue; 

      // Extract "MM-DD HH:MM:SS"
      String timestampStr = line.substring(comma3 + 1, comma4);
      
      // --- Parse the timestamp string into a DateTime object ---
      int year = now.year(); 
      int month = timestampStr.substring(0, 2).toInt();
      int day = timestampStr.substring(3, 5).toInt();
      int hour = timestampStr.substring(6, 8).toInt();
      int minute = timestampStr.substring(9, 11).toInt();
      int second = timestampStr.substring(12, 14).toInt();

      DateTime tapTime = DateTime(year, month, day, hour, minute, second);

      // --- Fix for New Year Bug ---
      if (tapTime.unixtime() > nowEpoch) {
        tapTime = DateTime(year - 1, month, day, hour, minute, second);
      }
      
      // --- Check if this tap was on the SAME DAY as 'now' ---
      if (tapTime.day() == now.day() && 
          tapTime.month() == now.month() && 
          tapTime.year() == now.year()) {
            
        // This is a tap from today
        count++; // Increment the tap count
        mostRecentTapEpoch = tapTime.unixtime(); // Store this tap's time
      }
    }
  }
  attendanceFile.close();

  // --- After checking the file, apply the 40-minute rule ---
  if (mostRecentTapEpoch > 0) { // If we found a tap from today
    long secondsSinceLastTap = nowEpoch - mostRecentTapEpoch;
    
    // 40 minutes = 2400 seconds
    if (secondsSinceLastTap < 2400 && count%2 != 0) {
      // Cooldown is active!
      Serial.println("Cooldown active. Last tap was " + String(secondsSinceLastTap) + " seconds ago.");
      return -1; // Return special code for "cooldown"
    }
  }

  // If not in cooldown, return the regular count
  return count;
}


String getStudentDetails(String uid) {
  Serial.print("Checking lookup DB for: "); Serial.println(uid);
  File lookupFile = SPIFFS.open("/lookup.csv", "r");
  if (!lookupFile) {
    Serial.println("lookup.csv not found.");
    return "";
  }
  while (lookupFile.available()) {
    String line = lookupFile.readStringUntil('\n');
    line.trim();
    int comma1 = line.indexOf(',');
    if (comma1 != -1) {
      if (line.substring(0, comma1).equals(uid)) {
        lookupFile.close();
        Serial.println("Found student.");
        return line; // Return the full "UID,Roll,Name" string
      }
    }
  }
  lookupFile.close();
  Serial.println("Student not found in DB.");
  displayOnLCD("Student Not","Found");
  return ""; // Not found
}

void logAttendance(String studentDetails, String tapType, String timestamp){
  File attendanceFile = SPIFFS.open("/attendance.csv", "a"); // "a" for append
  if (!attendanceFile) {
    Serial.println("Failed to open /attendance.csv for writing");
    return;
  }
  
  // Format: UID,Roll,Name,Timestamp,Type
  String logEntry = studentDetails + "," + timestamp + "," + tapType;

  int coma1 = studentDetails.indexOf(',');
  int coma2 = studentDetails.indexOf(',',coma1+1);
  String rollno = studentDetails.substring(coma1+1,coma2);
  String row1 = rollno + " " + tapType; 
  displayOnLCD(row1, timestamp);
  delay(1000);
  attendanceFile.println(logEntry);
  attendanceFile.close();
  
  Serial.println("ATTENDANCE LOGGED: " + logEntry);
  // You could add a GREEN LED blink here
}


void displayOnLCD(String row1,String row2) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(row1);
  lcd.setCursor(0,1);
  lcd.print(row2);

}

void clearAttendanceLog() {
    if (SPIFFS.exists("/attendance.csv")) {
      if (SPIFFS.remove("/attendance.csv")) {
        Serial.println("Daily attendance log cleared.");
      } else {
          Serial.println("Failed to clear the log.");
      }
    }
    else {
      Serial.println("File doesn't exist");
    }

    
}


// bool isDeviceConnectedToAP() {
//   // Returns the number of stations (devices) connected to the Soft-AP.
//   if (WiFi.softAPgetStationNum() > 0) {
//     return true;
//   }
//   return false;
// }


// ===================================================================
//  MAIN SETUP FUNCTION
// ===================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\nStarting MPMC Attendance System...");


  findMode();


  Wire.begin(SDA_PIN, SCL_PIN); 
    if (! rtc.begin()) {
      Serial.println("Couldn't find RTC!");
      displayOnLCD("RTC Module", "NOT FOUND!");
      while (1) delay(10); // Stop here if RTC fails
    }
    else Serial.println("RTC Module OK.");
  
  lcd.init();      // Initialize the lcd
  lcd.backlight();   // Turn on the backlight
  
  // lcd.setContrast(255);
  Serial.println("LCD Initialized. Printing message...");
  // pinMode(S0_PIN, INPUT);
  // pinMode(S1_PIN, INPUT); 
  if (!SPIFFS.begin()) { 
    Serial.println("SPIFFS mount failed! Halting.");
    lcd.print("SPIFFS FAILED");
    while(1);
  }
  Serial.println("SPIFFS Mounted.");
  SPI.begin();

  // clearAttendanceLog();  

  // Serial.println("Reading mode switches...");
  // s0 = digitalRead(S0_PIN);
  // s1 = digitalRead(S1_PIN);
  // Serial.print("Mode s0: "); Serial.println(s0);
  // Serial.print("Mode s1: "); Serial.println(s1);

  if (s0 == 0 && s1 == 0) {
    // ---------------------------------
    // --- MODE 0: CONFIGURATION MDOE
    // ---------------------------------
    Serial.println("Entering Mode 0: CONFIGURATION");
    displayOnLCD("Mode - 0:","Configuration");
    delay(2000);
    
    // 1. Start RFID and Hotspot
    mfrc522.PCD_Init();
    Serial.println("RFID Reader ready.");
    WiFi.softAP(ap_ssid, ap_password);
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("Hotspot created. IP: "); Serial.println(apIP);

    // 2. STAGE 1: Tell user to connect
    displayOnLCD("Connect to:","Attendance_Setup");
    Serial.println("Waiting for a client to connect to the hotspot...");

    // 3. Wait here until someone connects to the WiFi
    while (WiFi.softAPgetStationNum() == 0) {
      delay(500);
      Serial.print(".");
    }


    // 4. STAGE 2: Client connected!
    Serial.println("\nClient connected!");
    displayOnLCD("Visit:","configAttendance");
    visitWebsiteState = true;
    delay(4000);

    // 5. NOW start the web server and DNS
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);

    server.begin();
    Serial.println("Web server started.");
    dnsServer.start(53, dnsName, apIP);
    Serial.print("DNS Server started. Connect to: http://"); Serial.println(dnsName);
  }
  else if (s0 == 0 && s1 == 1) {
    // ---------------------------------
    //      MODE 1: ATTENDANCE LOGGING
    // ---------------------------------
    Serial.println("Entering Mode 1: ATTENDANCE LOGGING");
    displayOnLCD("Mode - 1:","Attendance Mode");
    delay(2000);
    displayOnLCD("Tap your ID card","");
  
    // 2. Start RFID
    mfrc522.PCD_Init();
    Serial.println("RFID Reader ready. Please scan card for attendance.");
  }
  else if (s0 == 1 && s1 == 0) {
      // ---------------------------------
      //     MODE 2: UPLOAD ATTENDANCE LOG
      // ---------------------------------
      Serial.println("Entering Mode 2: UPLOAD DAILY LOG");
      displayOnLCD("Mode 2: Upload", "Daily Log");
      delay(2000);
      displayOnLCD("Connecting to", WIFI_SSID);

      client.setInsecure();

      // Try to connect to WiFi
      Serial.print("Connecting to WiFi: "); Serial.print(WIFI_SSID);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      int retries = 0;
      while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
      }

      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFailed to connect to WiFi. Halting.");
        displayOnLCD("WiFi Failed!", "Check SSID/PASS");
      } else {
        Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
        displayOnLCD("WiFi Connected!", "Starting Sync...");
        delay(1000);
      }
  }
  else if (s0 == 1 && s1 == 1) {
    // ---------------------------------
    //     MODE 3: UPLOAD STUDENT DATABASE
    // ---------------------------------
    Serial.println("Entering Mode 3: UPLOAD STUDENT DATABASE");
    displayOnLCD("Mode - 3:","");
    delay(1000);
    displayOnLCD("Initialising","Database");
    delay(1000);

    client.setInsecure();

    Serial.print("Connecting to WiFi: "); Serial.print(WIFI_SSID);
    displayOnLCD("Connecting to",String(WIFI_SSID));
    delay(500);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }

    if (WiFi.status() != WL_CONNECTED) {
      displayOnLCD("WiFi connection","not found");
      delay(1000);
      displayOnLCD("Try Again","Press reset");
      Serial.println("\nFailed to connect to WiFi. Halting.");
    } else {
      displayOnLCD("WiFi connected","Uploading data..");
      Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
    }
  }
}

// ===================================================================
//     MAIN LOOP FUNCTION
// ===================================================================
void loop() {
  // --- MODE SELECTION ---
  
  if (s0 == 0 && s1 == 0) {
    // ---------------------------------
    // --- MODE 0: CONFIGURATION MODE
    // ---------------------------------
    
    if (WiFi.softAPgetStationNum() == 0) {
      if (!connectToWifiState) {
        displayOnLCD("Connect to:","Attendance_Setup");
        connectToWifiState = true;
        visitWebsiteState = false;
        visitedWebsite = false;
        tapPrinted = false;
      }
    }
    else {
      if (!visitWebsiteState) {
        displayOnLCD("Visit website","configAttendance");
        visitWebsiteState = true;
        connectToWifiState = false;
        visitedWebsite = false; 
        tapPrinted = false;
      }
    }

    dnsServer.processNextRequest();
    server.handleClient();
    
    // Logic to reset LCD after error
    if (currentState == STATE_DUPLICATE_ERROR) {
      if (millis() - errorTimer > 3000) {
        currentState = STATE_WAITING_FOR_CARD;
        // ADDED: Reset LCD
        if (!tapPrinted && visitedWebsite) {
          displayOnLCD("Tap Your ID Card","");
          tapPrinted = true;
        }
        
      }
    }
    
    if (currentState == STATE_WAITING_FOR_CARD) {
      if (!tapPrinted && visitedWebsite) {
        displayOnLCD("Tap Your ID Card","");
        tapPrinted = true;
      }
      if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        String uid = getUIDString(mfrc522.uid.uidByte, mfrc522.uid.size);
        Serial.println("Card Scanned: " + uid);
        if (isUidRegistered(uid)) {
          globalScannedUID = uid;
          currentState = STATE_DUPLICATE_ERROR;
          displayOnLCD("Duplicate",""); // CHANGED: Fixed typo
          errorTimer = millis();
        } else {
          globalScannedUID = uid;
          currentState = STATE_WAITING_FOR_INPUT;
          displayOnLCD("Please Wait","");
        }
        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        tapPrinted = false;
      }
    }
  }
  
  else if (s0 == 0 && s1 == 1) {
    // ---------------------------------
    // --- MODE 1: ATTENDANCE LOGGING
    // ---------------------------------
    if (millis() - lastScanTime_M1 < scanCooldown_M1) {
      inCooldown_M1 = true; // We are in cooldown
      return; // Still in cooldown, do nothing
    }
    
    if (inCooldown_M1) {
      inCooldown_M1 = false; // Reset the flag
      displayOnLCD("Tap your ID card",""); // Reset the display
    }

    // 2. Look for new card
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      return; // No card or read failed, do nothing
    }
    
    // --- A card has been tapped ---
    lastScanTime_M1 = millis(); // Reset cooldown timer

    // 3. Get the UID
    String uid = getUIDString(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println("Card Scanned: " + uid);

    // 4. Halt the card *immediately*
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    String studentData = getStudentDetails(uid); 

    if (studentData.equals("")) {
      Serial.println("Error: Unknown Card. Not in student database.");
      displayOnLCD("UNKNOWN CARD", ""); // Give feedback
      return;
    }

    // Log the event
    DateTime now = rtc.now(); // Get the current time

    // This ONE call checks cooldown AND gets the tap count
    int tapStatus = checkTapStatus(uid, now);

    // --- Format the timestamp string (only if we are logging) ---
    String timestamp = "";
    if (now.month() < 10) timestamp += "0";
    timestamp += String(now.month());
    timestamp += "-";
    if (now.day() < 10) timestamp += "0";
    timestamp += String(now.day());
    timestamp += " "; // Space between date and time
    if (now.hour() < 10) timestamp += "0";
    timestamp += String(now.hour());
    timestamp += ":";
    if (now.minute() < 10) timestamp += "0";
    timestamp += String(now.minute());
    timestamp += ":";
    if (now.second() < 10) timestamp += "0";
    timestamp += String(now.second());
    
    String tapType;

    if (tapStatus == -1) {
      // Case 1: Cooldown Active
      Serial.println("STUDENT TAPPED TOO RECENTLY (40 min cooldown).");
      displayOnLCD("PLEASE WAIT", "TRY AGAIN LATER");
      return; // Exit
    
    } else if (tapStatus == 0) {
      // Case 2: No taps today
      tapType = "IN";
      Serial.println("Status: Logging IN.");

    } else if (tapStatus == 1) {
      // Case 3: One tap today
      tapType = "OUT";
      Serial.println("Status: Logging OUT.");
      
    } else { 
      // Case 4: (tapStatus >= 2) Already logged out
      Serial.println("STUDENT ALREADY LOGGED OUT FOR TODAY...");
      displayOnLCD("ALREADY","LOGGED OUT");
      return;
    }
    
    // If we get here, it was a valid tap (IN or OUT)
    Serial.println("Current Time: " + timestamp);
    logAttendance(studentData, tapType, timestamp);

  }

  else if (s0 == 1 && s1 == 0) {
    // ---------------------------------
    // --- MODE 2: UPLOAD DAILY LOG
    // ---------------------------------
    // Check if we are connected AND if we haven't uploaded yet
      if (WiFi.status() == WL_CONNECTED && log_upload_in_progress) {
        log_upload_in_progress = false; // Set flag so we only run this once!

        Serial.println("Starting upload of attendance.csv...");
        
        File attendanceFile = SPIFFS.open("/attendance.csv", "r");
        if (!attendanceFile) {
          Serial.println("attendance.csv not found. No log to upload.");
          displayOnLCD("Log Empty", "Nothing to Sync.");
          return; // Stop here
        }

        // Open a temporary file to write *failed* uploads to
        File tempFile = SPIFFS.open("/temp_log.csv", "w");
        if (!tempFile) {
          Serial.println("Failed to create temp file. Aborting.");
          displayOnLCD("Sync Failed!", "SPIFFS Error.");
          return; // Stop here
        }

        HTTPClient http;
        int successCount = 0;
        int failCount = 0;
        int lineCount = 0;
        
        displayOnLCD("Syncing...", "Please Wait");
        while (attendanceFile.available()) {
            String line = attendanceFile.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue; // Skip empty lines

            lineCount++;
            Serial.print("Syncing line " + String(lineCount) + ": "); Serial.println(line);
            displayOnLCD("Syncing line...", String(lineCount));
            
            // --- Parse the local line ---
            // Format: UID,Roll,Name,Timestamp,Type
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);
            int comma4 = line.indexOf(',', comma3 + 1);
            
            if (comma1 == -1 || comma2 == -1 || comma3 == -1 || comma4 == -1) {
              Serial.println("Skipping malformed line. Saving for next time.");
              tempFile.println(line); // Save bad line to temp file
              failCount++;
              continue;
            }

            String uid = line.substring(0, comma1);
            String roll = line.substring(comma1 + 1, comma2);
            String name = line.substring(comma2 + 1, comma3);
            String timestamp = line.substring(comma3 + 1, comma4);
            String type = line.substring(comma4 + 1);

            // Timestamp format is "MM-DD HH:MM:SS"
            int spaceIndex = timestamp.indexOf(' ');
            if (spaceIndex == -1) {
              Serial.println("Skipping malformed timestamp. Saving for next time.");
              tempFile.println(line); // Save bad line
              failCount++;
              continue;
            }
            String date = timestamp.substring(0, spaceIndex); // This is "MM-DD"
            String time = timestamp.substring(spaceIndex + 1); // This is "HH:MM:SS"

            // URL-Encode spaces (just in case)
            name.replace(" ", "%20");

            // Build the URL for the Apps Script
            String url = SCRIPT_URL;
            url += "?action=addAttendance";
            url += "&uid=" + uid;
            url += "&roll=" + roll;
            url += "&name=" + name;
            url += "&date=" + date;
            url += "&time=" + time;
            url += "&type=" + type;
            
            http.begin(client, url);
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            int httpCode = http.GET();
            Serial.println("HTTP Response Code: " + String(httpCode)); // debug line
            bool uploadSuccess = false; // Flag for this *one* line

            if (httpCode > 0) {
              if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
                String payload = http.getString();
                Serial.println("Server response: " + payload);
                if (payload.startsWith("Success")) {
                  successCount++;
                  uploadSuccess = true;
                }
              }
            } else {
              Serial.printf("GET failed, error: %s\n", http.errorToString(httpCode).c_str());
            }
            http.end();
            
            // --- This is the "Safe Sync" logic ---
            if (!uploadSuccess) {
              // Upload failed! Save this line to the temp file
              Serial.println("Upload FAILED. Saving line for next sync.");
              failCount++;
              tempFile.println(line);
            } else {
              Serial.println("Upload SUCCEEDED.");
              // Do not write to temp file, so it's "deleted"
            }
            
            delay(200); // Small delay between requests

        } // end of while loop
        // --- Final Cleanup ---
        attendanceFile.close();
        tempFile.close();
        // Delete the original file
        SPIFFS.remove("/attendance.csv");
        // Rename the temp file to be the new log
        SPIFFS.rename("/temp_log.csv", "/attendance.csv");

        Serial.println("--- SYNC COMPLETE ---");
        Serial.print("Success: "); Serial.println(successCount);
        Serial.print("Failed:  "); Serial.println(failCount);


        // --- Final Result on LCD ---
        if (failCount == 0 && successCount > 0) {
          Serial.println("All entries uploaded. Log is now clear.");
          // LCD Case: Full Success
          displayOnLCD("Sync Complete!", "Log Cleared.");
        } else if (failCount == 0 && successCount == 0) {
          Serial.println("Log file was empty.");
          // LCD Case: Log was empty (This is a safety check)
          displayOnLCD("Log Empty", "Nothing to Sync.");
        } else {
          Serial.println("Some entries failed. Log updated.");
          // LCD Case: Partial Fail
          displayOnLCD("Sync Failed!", "Log NOT Cleared.");
        }
        
        Serial.println("You can now change modes.");

      } else if (WiFi.status() != WL_CONNECTED) {
        // This will loop if WiFi was never connected
        Serial.println("Waiting for WiFi...");
        // LCD Case: WiFi Failed (looping)
        if (!wifi) {
          displayOnLCD("WiFi Failed!", "Please Reset.");
          wifi = true;
        }
        delay(1000);
      }
  }
  else if (s0 == 1 && s1 == 1) {
    // --- MODE 3: UPLOAD STUDENT DATABASE
    // Check if we are connected AND if we haven't uploaded yet
    if (WiFi.status() == WL_CONNECTED && upload_in_progress) {
      new_disp = false;
      upload_in_progress = false; // Set flag so we only run this once

      Serial.println("Starting upload of lookup.csv...");
      File lookupFile = SPIFFS.open("/lookup.csv", "r");
      if (!lookupFile) {
        Serial.println("Failed to open lookup.csv. Nothing to upload.");
        return;
      }

      HTTPClient http;
      int successCount = 0;
      int failCount = 0;

      while (lookupFile.available()) {
        String line = lookupFile.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        int comma1 = line.indexOf(',');
        int comma2 = line.indexOf(',', comma1 + 1);
        if (comma1 == -1 || comma2 == -1) {
          Serial.println("Skipping malformed line: " + line);
          continue;
        }

        String uid = line.substring(0, comma1);
        String roll = line.substring(comma1 + 1, comma2);
        String name = line.substring(comma2 + 1);

        name.replace(" ", "%20"); // 

        // Build the URL for the Apps Script
        String url = SCRIPT_URL;
        url += "?action=addStudent";
        url += "&uid=" + uid;
        url += "&roll=" + roll;
        url += "&name=";
        url += name;



        Serial.print("Uploading: "); Serial.println(line);
        

        // Google Scripts redirect
        http.begin(client, url);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        int httpCode = http.GET();

        if (httpCode > 0) {
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = http.getString();
            Serial.println("Server response: " + payload);
            if (payload.startsWith("Success")) {
              successCount++;
            } else {
              failCount++;
            }
          }
        } else {
          Serial.printf("GET failed, error: %s\n", http.errorToString(httpCode).c_str());
          failCount++;
        }
        http.end();
        delay(200);
      }

      lookupFile.close();
      Serial.println("--- UPLOAD COMPLETE ---");
      Serial.print("Successfully uploaded: "); Serial.println(successCount);
      String success = "uploaded: " + String(successCount);
      displayOnLCD("Successfully",success);

      delay(2000);
      Serial.print("Failed to upload: "); Serial.println(failCount);
      String fail = "upload: " + String(failCount);

      displayOnLCD("Failed to",fail);
      delay(1000);

      if (failCount > 0) displayOnLCD("Press reset","to try again");
      Serial.println("You can now change modes.");
      if (failCount == 0) displayOnLCD("You can now","change modes");

    } else if (WiFi.status() != WL_CONNECTED) {
      if(!new_disp) {
        displayOnLCD("WiFi failed","Press reset");
        new_disp = true;
      }
      Serial.println("Waiting for WiFi... (Please check WIFI_SSID and WIFI_PASS)");
      delay(1000);
    }
    // If WiFi is connected and upload is done (upload_in_progress = false),
    // we do nothing. We just sit here idly.
  }
}
