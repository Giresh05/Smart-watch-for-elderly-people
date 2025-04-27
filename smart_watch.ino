// --- Blynk Credentials ---
#define BLYNK_TEMPLATE_ID "TMPL3MzNEfTAu"
#define BLYNK_TEMPLATE_NAME "Smartwatch"
#define BLYNK_AUTH_TOKEN "Gf15C4jIWq_zC04540ICObanw2LMdauQ" // Replace with your actual token if different
#define BLYNK_PRINT Serial // Enables Blynk logs to Serial Monitor

#include <Wire.h>
#include "MAX30100_PulseOximeter.h" // Using MAX30100_milan library
#include <MPU9250_asukiaaa.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

#include <WiFi.h>          // For ESP32 WiFi
#include <BlynkSimpleEsp32.h>
// --- End Blynk Configuration ---

// --- Sensor/System Configuration ---
#define SENSOR_READ_PERIOD_MS   1000 // How often to read sensors (1 second)
#define BLYNK_REPORT_PERIOD_MS  2500 // How often to send data to Blynk (2.5 seconds)
#define EMERGENCY_CHECK_PERIOD_MS 5000 // How often to check emergency status (5 seconds)
// #define FALL_THRESHOLD          1.5  // REMOVED OLD THRESHOLD
#define UPPER_FALL_THRESHOLD    1.2  // Threshold for high impact detection (adjust G value as needed)
#define LOWER_FALL_THRESHOLD    0.8  // Threshold for near-freefall detection (adjust G value as needed)
#define HEART_RATE_LOW_THRESHOLD 50  // Heart rate threshold for emergency (adjust BPM as needed) - Used in original emergency condition

// --- Virtual Pin Definitions ---
#define BLYNK_VPIN_HEART_RATE   V0
#define BLYNK_VPIN_SPO2         V1
#define BLYNK_VPIN_FALL_STATUS  V2 // Shows "Status: Normal" or "⚠️ Fall Detected!" - Resets quickly
#define BLYNK_VPIN_GPS          V3 // Use the Blynk GPS Streaming or Map widget for this pin
#define BLYNK_VPIN_EMERGENCY_MSG V4 // Shows "Status: OK" or "🚨 EMERGENCY..." - Changes only on state transition
// --- End Virtual Pin Definitions ---


// --- Sensor Objects ---
PulseOximeter pox;
MPU9250_asukiaaa mpu;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);  // UART1 for NEO-6M GPS (RX=16, TX=17 on many ESP32 boards)

// --- WiFi Credentials ---
char ssid[] = "Sri's S24 Ultra";     // Your WiFi network name
char pass[] = "slduke250"; // Your WiFi password
// --- End WiFi Credentials ---

BlynkTimer timer; // Blynk timer object

// --- Global Variables - To store the latest sensor readings ---
float currentHeartRate = 0;
int   currentSpO2 = 0;
float currentAccelX = 0, currentAccelY = 0, currentAccelZ = 0;
float currentAccelMag = 0;
float currentLat = 0, currentLng = 0;
bool  isLocationValid = false;
bool  fallDetected = false; // Flag reflecting state *during the last readSensors cycle*
String fallStatusMessage = "Status: Normal"; // Message for V2, resets each sensor cycle
String emergencyMessage = "Status: OK";      // Message for V4, changes on state transition
bool  emergencyConditionActive = false;      // Tracks if the V4 emergency condition is currently active

// --- Function Prototypes ---
void readSensors();
void sendDataToBlynk();
void checkEmergencyConditionAndReport();


void onBeatDetected() {
  // Callback for MAX30100 library (optional)
  // Serial.println("💓 Beat detected!");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Smartwatch System - Upper/Lower Fall Thresholds Added");

  // --- WiFi & Blynk Connection ---
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 30) { // 15 second timeout
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }
   if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\n❌ WiFi Connection Failed! Check credentials or network. Halting.");
      while(1) delay(1000); // Stop execution
  }
  Serial.println("\n✅ WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Blynk.config(BLYNK_AUTH_TOKEN);
  Serial.println("Connecting to Blynk...");
   if (Blynk.connect(5000)) { // 5 second timeout
      Serial.println("✅ Connected to Blynk!");
  } else {
      Serial.println("❌ Failed to connect to Blynk Server. Check Token/Server/Network.");
      // Consider halting if Blynk connection is critical
      // while(1) delay(1000);
  }
  // --- End WiFi & Blynk Connection ---


  // --- I2C Initialization ---
  Wire.begin(21, 22); // SDA, SCL pins for ESP32 (adjust if needed)
  // --- End I2C Initialization ---


  // --- MAX30100 Initialization ---
  Serial.println("Initializing MAX30100...");
  if (!pox.begin()) {
    Serial.println("❌ MAX30100 init failed. Check wiring or I2C address.");
    // Consider if operation should continue without this sensor
  } else {
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA); // Adjust sensitivity as needed
    pox.setOnBeatDetectedCallback(onBeatDetected);
    Serial.println("✅ MAX30100 ready.");
  }
  // --- End MAX30100 Initialization ---


  // --- MPU9250 Initialization ---
  Serial.println("Initializing MPU9250...");
  mpu.setWire(&Wire);

  // Call beginAccel() - It doesn't return a value to check.
  mpu.beginAccel();
  Serial.println("-> MPU9250 Accel initialization attempted.");

  // Call beginGyro() - It also doesn't return a value to check.
  mpu.beginGyro();
  Serial.println("-> MPU9250 Gyro initialization attempted.");

  Serial.println("✅ MPU9250 initialization sequence complete.");
  // --- End MPU9250 Initialization ---


  // --- GPS Initialization ---
  Serial.println("Initializing GPS...");
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17 (Standard ESP32 UART1)
  if (!gpsSerial) { // Check if serial port opened
      Serial.println("❌ Failed to begin GPS Serial communication on pins 16, 17!");
  } else {
      Serial.println("✅ GPS Serial ready (listening on pins 16, 17). Waiting for fix...");
  }
  // --- End GPS Initialization ---

  // --- Setup Timers ---
  // Timer 1: Read sensor data periodically
  timer.setInterval(SENSOR_READ_PERIOD_MS, readSensors);
  // Timer 2: Send collected data to Blynk periodically
  timer.setInterval(BLYNK_REPORT_PERIOD_MS, sendDataToBlynk);
  // Timer 3: Check for the emergency condition periodically
  timer.setInterval(EMERGENCY_CHECK_PERIOD_MS, checkEmergencyConditionAndReport);
  // --- End Setup Timers ---

  Serial.println("\n✅ Setup Complete. Starting main loop...\n------------------------------------------------");
}

void loop() {
  Blynk.run(); // IMPORTANT: Handles Blynk communication (sending/receiving data)
  timer.run(); // IMPORTANT: Executes scheduled timer functions (readSensors, sendDataToBlynk, etc.)

  // Keep updating the pulse oximeter state frequently for better readings
  pox.update();

  // Keep feeding GPS data to the parser continuously
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
}

// Function called by Timer 1: Reads all sensors and updates global variables
void readSensors() {
  // Serial.println("--- Reading Sensor Data (Cycle Start) ---"); // Indicate start of read cycle

  // --- HR and SpO2 ---
  currentHeartRate = pox.getHeartRate();
  currentSpO2 = pox.getSpO2();
  Serial.print("Read HR: "); Serial.print(currentHeartRate);
  Serial.print(" | Read SpO2: "); Serial.print(currentSpO2);

  // --- MPU Accelerometer & Fall Detection Logic ---
  // Reset fallDetected flag and V2 status message at the START of each sensor read cycle
  fallDetected = false;
  fallStatusMessage = "Status: Normal";

  if (mpu.accelUpdate() == 0) { // Check if update was successful (0 means success for this library)
    currentAccelX = mpu.accelX();
    currentAccelY = mpu.accelY();
    currentAccelZ = mpu.accelZ();
    currentAccelMag = sqrt(currentAccelX * currentAccelX + currentAccelY * currentAccelY + currentAccelZ * currentAccelZ);

    Serial.print(" | Accel Mag: "); Serial.print(currentAccelMag, 2); // Print magnitude

    // *** UPDATED FALL DETECTION CONDITION using Upper and Lower thresholds ***
    if (currentAccelMag > UPPER_FALL_THRESHOLD || currentAccelMag < LOWER_FALL_THRESHOLD) {
      // Fall detected THIS cycle due to breaching either upper or lower threshold
      fallDetected = true;
      fallStatusMessage = "⚠️ Fall Detected!";
      // Update debug message to reflect the new logic
      Serial.print(" | Fall Status Update: Threshold Breach! (Upper > ");
      Serial.print(UPPER_FALL_THRESHOLD);
      Serial.print(" or Lower < ");
      Serial.print(LOWER_FALL_THRESHOLD);
      Serial.print(")");
    } else {
      // No fall detected this cycle. Flags remain as reset at start.
    }
  } else {
    // MPU Accel update failed.
    fallStatusMessage = "Error: MPU Read Fail"; // Update V2 message on error
    currentAccelMag = 0; // Indicate error or invalid reading
    fallDetected = false; // Ensure flag is false on read failure
    Serial.print(" | MPU Accel update failed! Fall Status -> Error");
  }

  // --- GPS Location ---
  // gps.encode() is called in loop() for higher frequency parsing
  if (gps.location.isValid() && gps.location.isUpdated()) {
      currentLat = gps.location.lat();
      currentLng = gps.location.lng();
      isLocationValid = true;
      Serial.print(" | GPS Valid: Yes"); // Indicate GPS status
  } else {
      if (isLocationValid && !gps.location.isValid()){ // Only set false if truly invalid now
          isLocationValid = false;
      }
      Serial.print(" | GPS Valid: No"); // Indicate GPS status
       // isLocationValid state persists if it was valid and just hasn't updated
  }
  Serial.println(); // Newline after all sensor reads for this cycle
}


// Function called by Timer 2: Sends the latest data (from global vars) to Blynk
void sendDataToBlynk() {
  // Only send data if Blynk is actually connected
  if (!Blynk.connected()) {
      // Serial.println("--- Skipping Blynk send: Not connected ---"); // Reduce noise
      return;
  }

  Serial.println("--- Sending Data to Blynk ---");

  // --- Heart Rate (V0) ---
  // Send HR value regardless of whether it's > 0 or not.
  Blynk.virtualWrite(BLYNK_VPIN_HEART_RATE, currentHeartRate);
  Serial.print("  Sent HR (V0): "); Serial.println(currentHeartRate);

  // --- SpO2 (V1) ---
  // Send SpO2 value regardless of whether it's > 0 or not.
  Blynk.virtualWrite(BLYNK_VPIN_SPO2, currentSpO2);
  Serial.print("  Sent SpO2 (V1): "); Serial.println(currentSpO2);

  // --- Fall Status (V2) ---
  // Send Fall Status Message - reflects the last readSensors() result
  Serial.print("  Attempting to Send V2 Fall Status: '"); Serial.print(fallStatusMessage); Serial.println("'");
  Blynk.virtualWrite(BLYNK_VPIN_FALL_STATUS, fallStatusMessage);

  // --- GPS Location (V3) ---
  // Send GPS coordinates if they are valid
  if (isLocationValid) {
    Blynk.virtualWrite(BLYNK_VPIN_GPS, currentLat, currentLng);
    Serial.print("  Sent V3 Location: "); Serial.print(currentLat, 6);
    Serial.print(", "); Serial.println(currentLng, 6);
  } else {
     // Serial.println("  GPS location not valid/ready to send to V3.");
  }

   // Note: The emergency message (V4) is sent independently by checkEmergencyConditionAndReport()
}


// Function called by Timer 3: Checks emergency condition based on last sensor read and reports ONLY if state changes or is active
void checkEmergencyConditionAndReport() {
    // Only perform check if Blynk is connected
    if (!Blynk.connected()) {
      // Serial.println("--- Skipping Emergency Check: Blynk not connected ---"); // Reduce noise
      return;
    }

    // *** Choose the condition to check ***
    // !!! IMPORTANT: Remember to switch back to the original condition after testing !!!
    // bool currentEmergencyState = (fallDetected && currentHeartRate > 0 && currentHeartRate < HEART_RATE_LOW_THRESHOLD); // Original condition
    bool currentEmergencyState = fallDetected; // Simplified condition for testing V4/V2 interaction

   // --- Debug Block ---
   Serial.println("--- Checking Emergency Status (Timer 3) ---");
   Serial.print("  Value of 'fallDetected' flag (from last readSensors): "); Serial.println(fallDetected ? "TRUE" : "FALSE");
   // If using original condition, uncomment below for debugging:
   // Serial.print("  Current Heart Rate: "); Serial.println(currentHeartRate);
   // Serial.print("  HR > 0 ?: "); Serial.println(currentHeartRate > 0 ? "TRUE" : "FALSE");
   // Serial.print("  HR < Low Threshold ("); Serial.print(HEART_RATE_LOW_THRESHOLD); Serial.print(") ?: "); Serial.println(currentHeartRate < HEART_RATE_LOW_THRESHOLD ? "TRUE" : "FALSE");
   Serial.print("  >>> Overall Emergency Condition Met?: "); Serial.println(currentEmergencyState ? "YES" : "NO");
   Serial.print("  Was emergency already active (for V4)?: "); Serial.println(emergencyConditionActive ? "YES" : "NO");
   // --- End Debug Block ---


   if (currentEmergencyState) { // Checks the fallDetected flag from the *last* readSensors run
        // Set the V4 message based on the condition being checked
        // emergencyMessage = "🚨 EMERGENCY: Fall & Low HR!"; // Original message
        emergencyMessage = "🚨 EMERGENCY: Fall Detected!";    // Simplified message for testing

        // Send update to V4 only when the emergency state *newly* becomes active
        if (!emergencyConditionActive) {
            Serial.println("!!! EMERGENCY STATE TRIGGERED (Sending V4 Update) !!!");
            Serial.print("  Message: "); Serial.println(emergencyMessage);
            Serial.print("--> Attempting Blynk.virtualWrite to V"); Serial.println(BLYNK_VPIN_EMERGENCY_MSG);
            Blynk.virtualWrite(BLYNK_VPIN_EMERGENCY_MSG, emergencyMessage); // Send to V4
            Serial.println("--> Blynk.virtualWrite attempted.");
            // Optional: Trigger notification only once per event
            // Blynk.notify("🚨 EMERGENCY: Fall detected! Check location."); // Adjust notify message if needed
            emergencyConditionActive = true; // Mark V4 state as active so we don't spam Blynk
        } else {
             // Condition remains true (fall detected in last readSensors), but V4 was already showing emergency.
             // No need to send the same message again to V4.
             // Serial.println("  Emergency condition remains active (No new V4 update sent)."); // Reduce noise
        }

   } else { // currentEmergencyState is FALSE (no fall detected in the last readSensors run)
        // The condition is not met *now*. Check if it *was* active before, to send the clear message for V4.
        if (emergencyConditionActive) { // Send update to V4 only when the state *newly* becomes false
            emergencyMessage = "Status: OK"; // Reset V4 message
            Serial.println("--- Emergency Condition CLEARED (Sending V4 Update) ---");
            Serial.print("  Message: "); Serial.println(emergencyMessage);
            Serial.print("--> Attempting Blynk.virtualWrite to V"); Serial.println(BLYNK_VPIN_EMERGENCY_MSG);
            Blynk.virtualWrite(BLYNK_VPIN_EMERGENCY_MSG, emergencyMessage); // Send reset message to V4
            Serial.println("--> Blynk.virtualWrite attempted.");
            emergencyConditionActive = false; // Mark V4 state as inactive
            // NOTE: We do NOT reset fallDetected or fallStatusMessage here.
            // readSensors() handles resetting those variables at the beginning of its own cycle.
        } else {
             // Condition remains false, and was already false. No V4 update needed.
             // Serial.println("  No emergency condition detected (No change)."); // Reduce noise
        }
   }
}