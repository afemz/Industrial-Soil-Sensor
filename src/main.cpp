// #define BLYNK_TEMPLATE_ID "TMPLtTRnCSxO"
// #define BLYNK_DEVICE_NAME "Monitoring"
// #define BLYNK_AUTH_TOKEN "I9SeQV2Tmo--qoBjy8q18BZp5VEIl5sU"

// #include <WiFiManager.h>
#include <Arduino.h>
#include <Update.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ThingsBoard.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Firebase_ESP_Client.h>
// #include <FirebaseArduino.h>
#include <DHT.h>
// #include <BlynkSimpleEsp32.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "SmartIoT"
#define WIFI_PASSWORD "smartIoT!@#"

// Insert Firebase project API Key
#define API_KEY "AIzaSyCw3TXmvWJU1v5jxn4wZJC8xR3BmdvDU0A"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "soil-sensorsk-default-rtdb.asia-southeast1.firebasedatabase.app"

// TB Credential
#define THINGSBOARD_ACCESS_TOKEN "8VQ4SvlYjUkqTGDhaCKJ"
#define THINGSBOARD_SERVER "demo.thingsboard.io"

// // Initialize ThingsBoard client
WiFiClient espClient;

// // Initialize ThingsBoard instance
ThingsBoard tb(espClient);

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

// Oled Init
const int lebar = 128;
const int tinggi = 64;
const int reset = 4;
Adafruit_SSD1306 oled(lebar, tinggi, &Wire, reset);

uint8_t Anemometer_request[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x03, 0xB0, 0x0B}; // request_frame
byte Anemometer_buf[11];

float getTemperature(uint8_t tempR1, uint8_t tempR2); // declare variable for storing temps values
float getElect(uint8_t ecR1, uint8_t ecR2);           // declare variable for storing ec values
float getVwc(uint8_t wcR1, uint8_t wcR2);             // declare variable for storing vwc values

const int pompa = 27;         // nama alias pin 4 dengan nama "pompa"
const int thresholddown = 30; // Batas kelembaban tanah bawah
const int thresholdup = 40;   // Batas kelembaban tanah bawah

int pumpInit = 0;

// const int dry = 3580;
// const int wet = 1420;

const int dry = 2590;
const int wet = 890;

// Constants
const int hygrometer = 33; // Hygrometer sensor analog pin output at pin 33 of Arduino
// Constants
const int dhtpin = 26; // dht sensor analog pin output at pin 32 of Arduino

DHT dht(dhtpin, DHT22); //// Initialize DHT sensor for normal 16mhz Arduino

float hum;  // Stores humidity value
float temp; // Stores temperature value

int humPersen;
int val = 0;
int humPercentage;

unsigned long previousMillis = 0;
unsigned long millisTbSebelum = 0;
unsigned long millisFbSebelum = 0;
unsigned long millisRsSebelum = 0;

unsigned long period = 10000;
unsigned long periodRestart = 1800000;

void thingsBoardConnect();
void trigPump();
void getData();
void deepSleep();
void humidityChp();
void WiFiConnect();
void sendTb();
void sendFb();
// void fbSendData();

// void WifiReconnect();

bool pumpState;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

// float GET_VWC_TEST(uint16_t wcMSB, uint16_t wcLSB)
// {
//   int rawdata = (wcMSB * 256 + wcLSB);
//   float cleandata = (rawdata / 100.0);
//   return cleandata;
// }

void setup()
{

  Serial.begin(9600);
  WiFiConnect();
  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", ""))
  {
    Serial.println("sign up success");
    signupOK = true;
  }
  else
  {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  pinMode(pompa, OUTPUT);

  dht.begin();

  pumpState = false;
  digitalWrite(pompa, LOW);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Serial.println("GET VWC SETUP: ");
  // Serial.print(GET_VWC_TEST(0x0E, 0x93));

  oled.clearDisplay();
}

void loop()
{
  // WifiReconnect();

  thingsBoardConnect();
  getData();
  humidityChp();
  Firebase.reconnectWiFi(true);

  // Serial.print("PumpState : ");
  // Serial.println(pumpState);
  if (!pumpState && getVwc(Anemometer_buf[5], Anemometer_buf[6]) <= thresholddown)
  {
    // Serial.println("Sampe Sini Pump State Nyala");
    Serial.println("pumpInit");
    Serial.println(pumpInit);
    pumpState = true;
    pumpInit++;
  }

  if (pumpState && getVwc(Anemometer_buf[5], Anemometer_buf[6]) >= thresholdup)
  {
    // Serial.println("Sampe Sini Pump State Mati");
    Serial.println("pumpInit");
    Serial.println(pumpInit);
    pumpState = false;
    pumpInit = 0;
  }

  trigPump();

  float VwcData = getVwc(Anemometer_buf[5], Anemometer_buf[6]);
  // tb.sendTelemetryFloat("Vwc", VwcData);

  float EcData = getElect(Anemometer_buf[7], Anemometer_buf[8]);
  // tb.sendTelemetryFloat("Ec", EcData);

  memset(Anemometer_buf, 0x00, sizeof(Anemometer_buf));

  hum = dht.readHumidity();
  temp = dht.readTemperature();

  // Firebase.reconnectWiFi();

  unsigned long millisTb = millis();
  if ((unsigned long)(millisTb - millisTbSebelum) >= 10000)
  {
    sendTb();
    Serial.println("Tb data Sent");
    millisTbSebelum = millisTb;
  }

  unsigned long millisFb = millis();
  if ((unsigned long)(millisTb - millisTbSebelum) >= 5000)
  {
    sendFb();
    Serial.println("Fb data Sent");
    millisFbSebelum = millisFb;
  }

  Serial.print("Humidity: ");
  Serial.print(hum);
  Serial.print(" %, Temp: ");
  Serial.print(temp);

  oled.clearDisplay();

  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  // VWC Oled
  oled.setCursor(10, 15);
  oled.println("VWC : ");
  oled.setCursor(45, 15);
  oled.println(VwcData);

  // // RoomHum Oled
  // oled.setCursor(10, 35);
  // oled.println("RoomHum : ");
  // oled.setCursor(70, 35);
  // oled.println(humPercentage);

  // EC Oled
  oled.setCursor(10, 25);
  oled.println("EC : ");
  oled.setCursor(40, 25);
  oled.println(EcData);

  // RoomHum Oled
  oled.setCursor(10, 35);
  oled.println("RoomHum : ");
  oled.setCursor(70, 35);
  oled.println(hum);

  // RoomTemp Oled
  oled.setCursor(10, 45);
  oled.println("RoomTemp : ");
  oled.setCursor(70, 45);
  oled.println(temp);

  oled.display();
  delay(100);

  Serial.println("  ");

  unsigned long millisRs = millis();
  if (millisRs - millisRsSebelum >= periodRestart && (pumpState == false))
  {
    millisRsSebelum = millisRs;
    ESP.restart();
  }
  Serial.println(millis());
  delay(100);
}

void thingsBoardConnect()
{
  if (!tb.connected())
  {
    if (tb.connect(THINGSBOARD_SERVER, THINGSBOARD_ACCESS_TOKEN))
      Serial.println("Connected to thingsboard");
    else
    {
      Serial.println("Error connecting to thingsboard");
      delay(3000);
    }
  }
  tb.loop();
}

float getTemperature(uint8_t tempR1, uint8_t tempR2)
{
  uint16_t temperature = ((uint16_t)tempR1 << 8) | tempR2;
  temperature = temperature / 100;
  return temperature;
}

// int rawdata = (wcMSB * 256 + wcLSB);
// float cleandata = (rawdata / 100.0);
// return cleandata;

float getElect(uint8_t ecR1, uint8_t ecR2)
{
  uint16_t elect = ((uint16_t)ecR1 << 8) | ecR2;
  return elect;
}

float getVwc(uint8_t wcR1, uint8_t wcR2)
{
  uint16_t Vwc = ((uint16_t)wcR1 << 8) | wcR2;
  Vwc = Vwc / 100;

  // int rawVwc = (wcR1 * 256 + wcR2);
  // float cleanVwc = (rawVwc / 100);

  return Vwc;
}

// Industrial Soil Sensor data Gathering
void getData()
{
  Serial2.write(Anemometer_request, sizeof(Anemometer_request) / sizeof(Anemometer_request[0])); // request
  Serial2.flush();                                                                               // wait for the request to be sent
  delay(1000);
  while (Serial2.available())
  {
    Serial2.readBytes(Anemometer_buf, sizeof(Anemometer_buf)); // read
    Serial.println("Data: ");                                  // print out data
    for (byte i = 0; i < 11; i++)
    {
      // Serial.print(Anemometer_buf[i], HEX);
      // Serial.print(" ");
    }
    // Serial.println(" ");
  }
  // Serial.print("Temperature : ");
  // Serial.println(getTemperature(Anemometer_buf[3], Anemometer_buf[4]));

  Serial.print("Soil Moisture Industrial Sensor : ");
  Serial.println(getVwc(Anemometer_buf[5], Anemometer_buf[6]));

  Serial.print("Electroconductivity Industrial Sensor : ");
  Serial.println(getElect(Anemometer_buf[7], Anemometer_buf[8]));
}

bool toggle = true;

void trigPump()
{

  unsigned long currentMillis = millis();
  bool pumpStatus;

  if (pumpState == true)
  {

    // toggle = pumpState;
    toggle ? digitalWrite(pompa, HIGH) : digitalWrite(pompa, LOW);
    toggle ? Serial.println("Pompa Nyala") : Serial.println("Pompa Mati");

    if ((unsigned long)(currentMillis - previousMillis) >= period)
    {
      // Serial.println("Masuk pompa Off");
      digitalWrite(pompa, LOW);
      toggle = false;
    }
    if ((unsigned long)(currentMillis - previousMillis) >= period + 10000)
    {
      // Serial.println("Masuk pompa On lagi!");
      toggle = true;
      previousMillis = currentMillis;
    }
  }
  else
  {
    Serial.println("tanah basah");
    digitalWrite(pompa, LOW);
  }
}

void humidityChp()
{

  int sensorVal = analogRead(33);
  int humPercentage = map(sensorVal, wet, dry, 100, 0);
  // int VWCcheap = humPercentage;
  Serial2.readBytes(Anemometer_buf, sizeof(Anemometer_buf));
  float vwcCalibrate = getVwc(Anemometer_buf[5], Anemometer_buf[6]) * 1.075925;
  int humPercentage1 = humPercentage - vwcCalibrate;

  // Serial.println(humPercentage1);

  if (humPercentage1 <= 2)
  {
    humPercentage = 0;
    Serial.print("Low cost Soil Moisture Sensor Value : ");
    Serial.print(humPercentage1);
    Serial.println("%");
  }
  else if (humPercentage1 >= 100)
  {
    humPercentage1 = 100;
    Serial.print("Low cost Soil Moisture Sensor Value : ");
    Serial.print(humPercentage1);
    Serial.println("%");
  }
  else
  {
    Serial.print("Low cost Soil Moisture Sensor Value : ");
    Serial.print(humPercentage1);
    Serial.println("%");
  }
  humPersen = humPercentage1;

  // Serial.print(sensorVal);
}

void sendTb()
{

  getData();
  float VwcData = getVwc(Anemometer_buf[5], Anemometer_buf[6]);
  tb.sendTelemetryFloat("Vwc", VwcData);

  float EcData = getElect(Anemometer_buf[7], Anemometer_buf[8]);
  tb.sendTelemetryFloat("Ec", EcData);

  humidityChp();
  tb.sendTelemetryFloat("VWCchp", humPersen);
  tb.sendTelemetryFloat("RoomHum", hum);
  tb.sendTelemetryFloat("RoomTmp", temp);
}

void sendFb()
{

  getData();
  float VwcData = getVwc(Anemometer_buf[5], Anemometer_buf[6]);

  float EcData = getElect(Anemometer_buf[7], Anemometer_buf[8]);

  humidityChp();

  Firebase.RTDB.setInt(&fbdo, "SensorReading/VWCcheap", humPersen);
  Firebase.RTDB.setFloat(&fbdo, "SensorReading/VWCindustry", VwcData);
  Firebase.RTDB.setFloat(&fbdo, "SensorReading/ECindustry", EcData);
  Firebase.RTDB.setFloat(&fbdo, "SensorReading/RoomHum", hum);
  Firebase.RTDB.setFloat(&fbdo, "SensorReading/RoomTmp", temp);
  Firebase.RTDB.setBool(&fbdo, "SensorReading/PumpState", pumpState);
}

void WiFiConnect()
{

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.print("System connected with IP address: ");
  Serial.println(WiFi.localIP());
  Serial.printf("RSSI: %d\n", WiFi.RSSI());
}

// void deepSleep()
// {
//     esp_sleep_enable_timer_wakeup(10000000);
//     esp_deep_sleep_start();
// }

// （0EH * 256 + 93H） / 100 = 3731 / 100 = 37.31 %