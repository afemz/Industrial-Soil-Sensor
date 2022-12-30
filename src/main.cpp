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
#include "DHT.h"
#include <Adafruit_Sensor.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "dika"
#define WIFI_PASSWORD "12345678"

// Insert Firebase project API Key
#define API_KEY "AIzaSyCw3TXmvWJU1v5jxn4wZJC8xR3BmdvDU0A"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "soil-sensorsk-default-rtdb.asia-southeast1.firebasedatabase.app"

// TB Credential
#define THINGSBOARD_ACCESS_TOKEN "9eLeFg0SMhGfx0mElJpU"
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
const int thresholddown = 20; // Batas kelembaban tanah bawah
const int thresholdup = 50;   // Batas kelembaban tanah bawah

// Constants
const int hygrometer = 33; // Hygrometer sensor analog pin output at pin A0 of Arduino
// Constants
const int dhtpin = 32; // dht sensor analog pin output at pin A0 of Arduino

#define DHTTYPE DHT22     // DHT 22  (AM2302)
DHT dht(dhtpin, DHTTYPE); //// Initialize DHT sensor for normal 16mhz Arduino

float hum;  // Stores humidity value
float temp; // Stores temperature value

int soilHumidity;

int val = 0;

void thingsBoardConnect();
void trigPump();
void getData();
void deepSleep();
void humidityChp();
void WiFiConnect();
void fbSendData();
// void WifiReconnect();

bool pumpState;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

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
    Serial.println("ok");
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

  oled.clearDisplay();
}

void loop()
{
  // WifiReconnect();
  thingsBoardConnect();
  getData();

  Serial.print("PumpState : ");
  Serial.println(pumpState);
  if (!pumpState && getVwc(Anemometer_buf[5], Anemometer_buf[6]) <= thresholddown)
  {
    Serial.println("Sampe Sini Pump State Nyala");
    pumpState = true;
  }

  if (pumpState && getVwc(Anemometer_buf[5], Anemometer_buf[6]) >= thresholdup)
  {
    Serial.println("Sampe Sini Pump State Mati");
    pumpState = false;
  }

  // trigPump();

  float TempData = getTemperature(Anemometer_buf[3], Anemometer_buf[4]);
  // tb.sendTelemetryFloat("Temp", TempData);

  float VwcData = getVwc(Anemometer_buf[5], Anemometer_buf[6]);
  tb.sendTelemetryFloat("Vwc", VwcData);

  float EcData = getElect(Anemometer_buf[7], Anemometer_buf[8]);
  tb.sendTelemetryFloat("Ec", EcData);

  Firebase.RTDB.setFloat(&fbdo, "SensorReading/VWCindustry", VwcData);
  Firebase.RTDB.setFloat(&fbdo, "SensorReading/ECindustry", EcData);

  memset(Anemometer_buf, 0x00, sizeof(Anemometer_buf));

  hum = dht.readHumidity();
  tb.sendTelemetryFloat("RoomHum", hum);
  temp = dht.readTemperature();
  tb.sendTelemetryFloat("RoomTemp", temp);

  fbSendData();

  Serial.print("Humidity: ");
  Serial.print(hum);
  Serial.print(" %, Temp: ");
  Serial.print(temp);

  humidityChp();

  // val = digitalRead(pompa);
  // Serial.print("status SSR:");
  // Serial.println(val);

  oled.setTextSize(1);
  oled.setTextColor(WHITE);

  // Temp Oled
  oled.setCursor(10, 5);
  oled.println("Temp : ");
  oled.setCursor(50, 5);
  oled.println(TempData);

  // VWC Oled
  oled.setCursor(10, 15);
  oled.println("VWC : ");
  oled.setCursor(45, 15);
  oled.println(VwcData);

  // EC Oled
  oled.setCursor(10, 25);
  oled.println("EC : ");
  oled.setCursor(40, 25);
  oled.println(EcData);

  oled.display();
  delay(100);
  oled.clearDisplay();

  delay(500);
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

float getElect(uint8_t ecR1, uint8_t ecR2)
{
  uint16_t elect = ((uint16_t)ecR1 << 8) | ecR2;
  return elect;
}

float getVwc(uint8_t wcR1, uint8_t wcR2)
{
  uint16_t Vwc = ((uint16_t)wcR1 << 8) | wcR2;
  Vwc = Vwc / 100; // ini buat sekam

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
      Serial.print(Anemometer_buf[i], HEX);
      Serial.print(" ");
    }
    Serial.println(" ");
  }
  Serial.print("Temperature : ");
  Serial.println(getTemperature(Anemometer_buf[3], Anemometer_buf[4]));

  Serial.print("Water Content : ");
  Serial.println(getVwc(Anemometer_buf[5], Anemometer_buf[6]));

  Serial.print("Electroconductivity : ");
  Serial.println(getElect(Anemometer_buf[7], Anemometer_buf[8]));
}

void trigPump()
{
  unsigned long currentMillis = millis();
  unsigned long previousMillis = 0;
  unsigned long period = 30000;
  bool pumpStatus;

  if (pumpState == true)
  {
    pumpStatus = true;                               // THINGSBOARD
    tb.sendAttributeBool("Pump Status", pumpStatus); // THINGSBOARD

    digitalWrite(pompa, HIGH);
    Serial.println("Pompa Nyala");
    if (currentMillis - previousMillis >= period)
    {
      digitalWrite(pompa, LOW);
      previousMillis = currentMillis;
    }
    // delay(30000); // Delay agar arus air sampai pada valve sebelum terbuka

    // delay(5000);

    Serial.println("Masuk delay pompa");
    digitalWrite(pompa, LOW);
    delay(5000); // Delay agar air meresap media
    if (pumpState == false)
    {
      // pumpStatus = false;                              // THINGSBOARD
      // tb.sendAttributeBool("Pump Status", pumpStatus); // THINGSBOARD
      Serial.println("Pompa Meninggal");
      digitalWrite(pompa, LOW);
    }
  }
  else if (pumpState == false)
  {
    pumpStatus = false;                              // THINGSBOARD
    tb.sendAttributeBool("Pump Status", pumpStatus); // THINGSBOARD
    Serial.println("Pompa Meninggal 2");
    digitalWrite(pompa, LOW);
  }
}

void humidityChp()
{
  soilHumidity = analogRead(hygrometer); // Read analog value
  // Serial.println(" ");
  // Serial.println("value analog :");
  // Serial.println(value);5
  soilHumidity = constrain(soilHumidity, 2500, 4095); // Keep the ranges!

  soilHumidity = map(soilHumidity, 4095, 2500, 0, 100); // Map value : 400 will be 100 and 1023 will be 0

  tb.sendTelemetryFloat("VWCchp", soilHumidity);

  Serial.print("Soil humidity: ");
  Serial.print(soilHumidity);
  Serial.println("%");
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

void WifiConnect()
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

// unsigned long previousMillis = 0;
// unsigned long interval = 5000;
// void WifiReconnect()
// {
//     unsigned long currentMillis = millis();
//     // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
//     if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval))
//     {
//         Serial.print(millis());
//         Serial.println("Reconnecting to WiFi...");
//         WiFi.disconnect();
//         WiFi.reconnect();
//         previousMillis = currentMillis;
//     }
// }

void fbSendData()
{

  // tb.sendTelemetryFloat("Ec", EcData);
  if (Firebase.ready() && signupOK)
  {
    // Write an Int number on the database path test/int
    Firebase.RTDB.setInt(&fbdo, "SensorReading/VWCcheap", soilHumidity);

    Firebase.RTDB.setInt(&fbdo, "SensorReading/RoomHum", hum);
    Firebase.RTDB.setInt(&fbdo, "SensorReading/RoomTmp", temp);
  }
}

// void deepSleep()
// {
//     esp_sleep_enable_timer_wakeup(10000000);
//     esp_deep_sleep_start();
// }
