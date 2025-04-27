**#Smart Watch for Elderly People**
**Overview**
This project is designed to create a smartwatch for elderly people that provides various health monitoring features. The watch will monitor vital health parameters, such as heart rate, SPO2 levels, and movement, to assist in detecting potential health issues like falls or irregularities.
**Features**
- Heart Rate Monitoring: Using the MAX30100 pulse oximeter sensor to track heart rate and oxygen levels.
- Fall Detection: Using the MPU-9250 gyroscope to detect sudden movements or falls.
- Real-time Health Monitoring: Display real-time health data on a connected OLED screen.
- Wireless Communication: Data is sent to a connected device for further analysis via Bluetooth/WiFi.
**Components Used**
- ESP32: The main microcontroller that processes sensor data and manages communication.
- MAX30100 Pulse Oximeter: Used for measuring heart rate and SPO2 levels.
- MPU-9250 Gyroscope: Detects falls or unusual movement.
- OLED Display: Displays real-time health data.
- Blynk App: Used to visualize health data on a mobile device in real-time.
**Installation
Prerequisites**
Make sure you have the following installed before starting:
- Arduino IDE: The latest version of the Arduino IDE.
- ESP32 Board: Install the ESP32 board in the Arduino IDE.
**Steps**
1. Clone the repository to your local machine:
   ```bash
   git clone https://github.com/Giresh05/Smart-watch-for-elderly-people.git
   ```
2. Open the project in Arduino IDE.
3. Install the necessary libraries:
   - Wire: For I2C communication.
   - Adafruit_GFX: For display graphics.
   - Adafruit_SSD1306: For OLED display support.
   - MAX30105: For heart rate sensor support.
   - MPU9250_asukiaaa: For gyroscope sensor support.
4. Upload the code to your ESP32 board.
5. Connect the sensors and display as per the schematic in the repository.
6. Monitor your health data through the OLED display or send it to a mobile device using Blynk.
Future Enhancements
- More Sensors: Integrate more health sensors, like temperature or ECG sensors.
- Cloud Integration: Upload data to the cloud for better tracking and analysis.
- Customizable Alerts: Implement customizable health alerts based on predefined thresholds.
**License**
This project is licensed under the MIT License - see the LICENSE file for details.
**Acknowledgements**
- Thanks to the open-source community for the libraries used in this project.
- Special thanks to GitHub for hosting this repository.
