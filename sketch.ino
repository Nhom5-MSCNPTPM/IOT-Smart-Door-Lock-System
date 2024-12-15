#include <Keypad.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Pin definitions for the components
#define SERVO_PIN 5
#define BUZZER_PIN 15
#define RED_LED_PIN 27
#define GREEN_LED_PIN 26
#define BLUE_LED_PIN 25

// LCD setup
LiquidCrystal_I2C lcd(0x27, 16, 2); // Address 0x27, 16 columns and 2 rows

// Servo setup
Servo myServo;

// Keypad setup (4x4 matrix)
const byte ROWS = 4; // Four rows
const byte COLUMNS = 4; // Four columns
char keys[ROWS][COLUMNS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte pin_rows[ROWS] = {2, 0, 4, 16};
byte pin_column[COLUMNS] = {17, 18, 19, 23};
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROWS, COLUMNS);

// Wi-Fi credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* thingSpeakUrl = "https://api.thingspeak.com/update?api_key=XL3RF6SCTRQK7RSM";

// Door code
String doorCode = "1234";  // Mã khóa cửa (bạn có thể thay đổi theo nhu cầu)
String enteredCode = ""; // Ma khoa nhap vao

int attemptCount = 0; // Dem so lan nhap sai code
int usageCount = 0; // So lan su dung he thong (hay so lan mo cua thanh cong)
unsigned long doorOpenTime = 0; // Thoi gian mo cua

// Door control state
bool doorStatus = false;  // false = closed, true = open

// Setup function
void setup() {
  // Init Serial
  Serial.begin(115200);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.setCursor(0, 0);
    lcd.print("Connecting...");
    Serial.println("Connecting to WiFi...");
  }
  lcd.clear();
  lcd.print("WiFi Connected");
  delay(2000);
  lcd.clear();
  Serial.println("Connected to WiFi");

  lcd.setCursor(0, 0);
  lcd.print("Enter Passcode:");

  // Setup servo
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  Serial.println("Servo in lock status");

  // Setup LEDs
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);

  setRGBColor(255, 0, 0);
  delay(200);
  setRGBColor(255, 255, 255);

  // Setup buzzer
  tone(BUZZER_PIN, 1000);
  delay(200);
  noTone(BUZZER_PIN);
  delay(200);
}

// Main loop
void loop() {
  char key = keypad.getKey();

  if (key) {
    if (key == '#') {
      checkPassword();
    } else if (key == '*') {
      enteredCode = ""; // Reset password
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter Passcode:");
    } else {
      enteredCode += key;
      lcd.setCursor(0, 1);
      lcd.print(enteredCode);
    }
  }
}

void checkPassword() {
  if (enteredCode == doorCode) {
    // Dung mat khau
    attemptCount = 0;
    usageCount++;
    doorStatus = true;
    doorOpenTime = millis(); // Ghi lai thoi gian mo cua

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door Opened");
    Serial.println("Door Opened");
    setRGBColor(0, 255, 0); // Green (Opened)
    myServo.write(90); // Unlock position
    Serial.println("Servo in unlock status");
    buzz(1);
    sendDataToThingSpeak(1, WiFi.status() == WL_CONNECTED ? 1 : 0, usageCount, attemptCount, enteredCode);

    delay(5000);
    myServo.write(0);
    Serial.println("Servo back to lock status");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door Closed");
    delay(200);
    setRGBColor(255, 255, 255);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Passcode:");
  } else {
    // Mat khau khong dung
    attemptCount++;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Incorrect Code");
    Serial.println("Incorrect Code");
    setRGBColor(255, 255, 0);
    buzz(2);
    sendDataToThingSpeak(0, WiFi.status() == WL_CONNECTED ? 1 : 0, usageCount, attemptCount, enteredCode);
    delay(2000);
    if (attemptCount >= 3) {
      // He thong khoa sau khi nhap sai mat khau 3 lan
      doorStatus = false;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Door Closed");
      Serial.println("Door Closed");
      setRGBColor(255, 0, 0); // Red
      buzz(3);
      myServo.write(0); // Lock position
      delay(2000);
      setRGBColor(255, 255, 255);
      attemptCount = 0; // Reset counter
      sendDataToThingSpeak(0, WiFi.status() == WL_CONNECTED ? 1 : 0, usageCount, attemptCount, enteredCode);
    } else {
      delay(2000);
      setRGBColor(255, 255, 255);
    }
    // Hiển thị lại "Enter Passcode:" sau khi sai
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Passcode:");
  }
  enteredCode = ""; // Reset input
}

void setRGBColor(int r, int g, int b) {
  // digitalWrite(RED_LED_PIN, r > 0 ? HIGH : LOW);
  // digitalWrite(GREEN_LED_PIN, g > 0 ? HIGH : LOW);
  // digitalWrite(BLUE_LED_PIN, b > 0 ? HIGH : LOW);
  analogWrite(RED_LED_PIN, r);
  analogWrite(GREEN_LED_PIN, g);
  analogWrite(BLUE_LED_PIN, b);
}

void buzz(int times) {
  for (int i = 0; i < times; i++) {
    tone(BUZZER_PIN, 1000);
    delay(200);
    noTone(BUZZER_PIN);
    delay(200);
  }
}

void sendDataToThingSpeak(int doorStatus, int wifiStatus, int usageCount, int attemptCount, const String& passcode) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(thingSpeakUrl)
                 + "&field1=" + String(doorStatus)
                 + "&field2=" + String(millis() - doorOpenTime)
                 + "&field3=" + String(wifiStatus)
                 + "&field4=" + String(attemptCount)
                 + "&field5=" + String(usageCount)
                 + "&field6=" + passcode;
    http.begin(url);
    int httpCode = http.GET();  // Make HTTP GET request
    if (httpCode > 0) {
      Serial.println("Data sent successfully!");
    } else {
      Serial.println("Error sending data");
    }
    http.end();
  }
}
