#include <Arduino.h>
#include <Update.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ThingsBoard.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Declare Wifi SSID and Pass
const char *ssid = "PocoX3";
const char *password = "test1234";

// TB Credential
#define THINGSBOARD_ACCESS_TOKEN "9eLeFg0SMhGfx0mElJpU"
#define THINGSBOARD_SERVER "demo.thingsboard.io"

// Deklarasi Fungsi
void WifiConnect();

// Initialize ThingsBoard client
WiFiClient espClient;

// Initialize ThingsBoard instance
ThingsBoard tb(espClient);

String temp = "Temperature : ";
String ec = "Electroconductivity : ";
String wc = "Water Content : ";

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

const int SSR = 4;            // nama alias pin 4 dengan nama "SSR"
const int thresholddown = 20; // Batas kelembaban tanah bawah
const int thresholdup = 50;   // Batas kelembaban tanah bawah

void thingsBoardConnect();
void trigPump();
void getData();
void deepSleep();

bool pumpState;

void setup()
{
  pumpState = false;
  digitalWrite(SSR, LOW);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(9600);
  // WifiConnect();
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  digitalWrite(SSR, LOW);
  oled.clearDisplay();
}

void loop()
{
  // WifiConnect();
  // thingsBoardConnect();
  getData();

  Serial.println(pumpState);
  if (!pumpState && getVwc(Anemometer_buf[5], Anemometer_buf[6]) <= thresholddown)
  {
    Serial.print("Sampe Sini Pump State Nyala");
    pumpState = true;
  }

  if (pumpState && getVwc(Anemometer_buf[5], Anemometer_buf[6]) >= thresholdup)
  {
    Serial.print("Sampe Sini Pump State Mati");
    pumpState = false;
  }

  trigPump();

  float TempData = getTemperature(Anemometer_buf[3], Anemometer_buf[4]);
  tb.sendTelemetryFloat("Temp", TempData);

  float VwcData = getVwc(Anemometer_buf[5], Anemometer_buf[6]);
  tb.sendTelemetryFloat("Vwc", VwcData);

  float EcData = getElect(Anemometer_buf[7], Anemometer_buf[8]);
  tb.sendTelemetryFloat("Ec", EcData);

  memset(Anemometer_buf, 0x00, sizeof(Anemometer_buf));

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

  oled.clearDisplay();

  delay(500);
}

void WifiConnect()
{
  int count = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(1000);
    ESP.restart();

    if (WL_CONNECTED)
    {
      Serial.print("System connected with IP address: ");
      Serial.println(WiFi.localIP());
      Serial.printf("RSSI: %d\n", WiFi.RSSI());
    }
  }
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
  Serial.print(temp);
  Serial.println(getTemperature(Anemometer_buf[3], Anemometer_buf[4]));

  Serial.print(wc);
  Serial.println(getVwc(Anemometer_buf[5], Anemometer_buf[6]));

  Serial.print(ec);
  Serial.println(getElect(Anemometer_buf[7], Anemometer_buf[8]));
}

void pumpStat()
{

  Serial.println(pumpState);
  if (!pumpState && getVwc(Anemometer_buf[5], Anemometer_buf[6]) <= thresholddown)
  {
    Serial.print("Sampe Sini Pump State Nyala");
    pumpState = true;
  }

  if (pumpState && getVwc(Anemometer_buf[5], Anemometer_buf[6]) >= thresholdup)
  {
    Serial.print("Sampe Sini Pump State Mati");
    pumpState = false;
  }
}

void trigPump()
{
  // unsigned long interval = 4000;
  // unsigned long previousMillis = 0;
  // unsigned long currentMillis = millis();

  if (pumpState == true)
  {
    // if (currentMillis - previousMillis >= interval)
    // {
    digitalWrite(SSR, HIGH);
    Serial.println("Pompa Nyala");
    // previousMillis = millis();
    // pumpStatus = true; // THINGSBOARD
    // tb.sendAttributeBool("Pump Status", pumpStatus); // THINGSBOARD
    // }
  }
  else if (pumpState == false)
  {
    // pumpStatus = true; // THINGSBOARD
    // tb.sendAttributeBool("Pump Status", pumpStatus); // THINGSBOARD
    Serial.println("Pompa Meninggal");
    digitalWrite(SSR, LOW);
  }
}

void deepSleep()
{
  esp_sleep_enable_timer_wakeup(10000000);
  esp_deep_sleep_start();
}
