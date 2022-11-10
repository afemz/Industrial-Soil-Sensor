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

// Oled Init
const int lebar = 128;
const int tinggi = 64;
const int reset = 4;

Adafruit_SSD1306 oled(lebar, tinggi, &Wire, reset);

// Deklarasi Fungsi
void WifiConnect();

// Initialize ThingsBoard client
WiFiClient espClient;
// Initialize ThingsBoard instance
ThingsBoard tb(espClient);
// PubSubClient mqtt(esp);

String temp = "Temperature : ";
String ec = "Electroconductivity : ";
String wc = "Water Content : ";

uint8_t Anemometer_request[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x03, 0xB0, 0x0B}; // request_frame
byte Anemometer_buf[11];

float getTemperature(uint8_t tempR1, uint8_t tempR2); // declare variable for storing temps values
float getElect(uint8_t ecR1, uint8_t ecR2);           // declare variable for storing ec values
float getVwc(uint8_t wcR1, uint8_t wcR2);             // declare variable for storing vwc values

void setup()
{
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  Serial.begin(9600);
  WifiConnect();
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
}

void loop()
{
  // TB Connect
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

  // Industrial Soil Sensor data Gathering
  Serial2.write(Anemometer_request, sizeof(Anemometer_request)); // request
  Serial2.flush();                                               // wait for the request to be sent
  delay(2000);
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
  // Serial.print(" C");

  Serial.print(wc);
  Serial.println(getVwc(Anemometer_buf[5], Anemometer_buf[6]));
  // Serial.print(" %");

  Serial.print(ec);
  Serial.println(getElect(Anemometer_buf[7], Anemometer_buf[8]));
  // Serial.print(" μs/cm");

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

  // float EC = randomEC().toDouble();
  oled.clearDisplay();

  delay(2000);
}

void WifiConnect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
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

  Vwc = Vwc / 100;

  return Vwc;
}