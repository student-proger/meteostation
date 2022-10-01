/*******************************************************************************
* Author:         Gladyshev Dmitriy (2022) 
* 
* Create Date:    17.07.2022  
* Name:           Метеостанция
* Version:        4.0
* Target Devices: ESP32
* Tool versions:  Arduino 1.8.13, ESP32 1.0.6
*
* URL:            
* 
*******************************************************************************/

#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_BMP085.h>
#include <HTTPClient.h>

//** НАСТРОЙКИ *******************************************************************
#define ssid               "*************"      // Имя WiFi сети
#define password           "*************"      // Пароль WiFi
#define PIN_DS18B20        4                    // Пин подключения датчика DS18B20
#define BMP085_EXIST       1                    // наличие датчика атмосферного давления
#define USE_NARODMON       1
#define USE_OWNSERVER      0
#define OWNER              "username"           // Имя владельца (логин на narodmon)
// URL для отправки данных на свой сервер:
#define OWNURL             "http://example.com/upd.php?param1=1&"
//********************************************************************************

OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);
#if BMP085_EXIST == 1
  Adafruit_BMP085 bmp;
#endif

int timeout = 20000;
int timeout2 = 20000;
float t;
float th;
float pmm;

void WiFiEvent(WiFiEvent_t event);

bool sendToNarodmon()
{
  WiFiClient client;

  String buf = "#" + WiFi.macAddress() + "\n";
  buf = buf + "#OWNER#" + OWNER + "\n";
  buf = buf + "#T1#" + String(t) + "\n";
  #if BMP085_EXIST == 1
    buf = buf + "#P1#" + String(pmm) + "\n";
  #endif
  buf = buf + "##";

  Serial.print(F("Connecting to narodmon... "));
  if (client.connect("narodmon.ru", 8283))
  {
    client.print(buf);
    delay(250);

    while (client.available())
    {
      String s = client.readString();
      Serial.println(s);
    }

    client.stop();
    Serial.println(F("Success"));
    return true;
  }
  else
  {
    Serial.println(F("FAIL"));
    return false;
  }
}

bool sendToLocalServer()
{
  String url = String(OWNURL) + "r=";
  url = url + String(millis() % 1000);
  url = url + "&t=" + String(t);
  #if BMP085_EXIST == 1
    url = url + "&p=" + String(pmm);
    url = url + "&th=" + String(th);
  #endif
  
  HTTPClient http;
  Serial.println(url);
  Serial.print(F("Connecting to local server... "));
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0) 
  {
      Serial.printf("code: %d\n", httpCode);

      if (httpCode == HTTP_CODE_OK)
      {
        http.end();
        Serial.println(F(" Success"));
        return true;
      }
  }
  else
  {
      Serial.printf("FAIL, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  return false;
}

void readSensors()
{
  Serial.print(F("Requesting temperature... "));
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println(F("DONE"));

  t = sensors.getTempCByIndex(0);
  #if BMP085_EXIST == 1
    Serial.print(F("Requesting pressure... "));
    th = bmp.readTemperature();
    int p = bmp.readPressure();
    Serial.println(F("DONE"));
    pmm = (float) p / 133.321995;
  #endif
  
  Serial.print(F("Temperature DS18B20 = "));
  Serial.println(t);

  #if BMP085_EXIST == 1
    Serial.print(F("Temperature BMP085 = "));
    Serial.print(th);
    Serial.println(F(" *C"));
      
    Serial.print(F("Pressure = "));
    Serial.print(p);
    Serial.println(F(" Pa"));

    Serial.print(F("Pressure = "));
    Serial.print(pmm);
    Serial.println(F(" mm Hg"));
    Serial.println();
  #endif
}

/*******************************************************************************
* Function Name  : setup_wifi
* Description    : Настройка WiFi соединения
*******************************************************************************/

void setup_wifi() 
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.onEvent(WiFiEvent);

  randomSeed(micros());
}

void setup(void)
{
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println();

  for(uint8_t t = 4; t > 0; t--)
  {
      Serial.printf("[SETUP] WAIT %d...\n", t);
      Serial.flush();
      delay(1000);
  }

  setup_wifi();

  sensors.begin();
  #if BMP085_EXIST == 1
    bmp.begin();
  #endif
}

void loop(void)
{ 
  if (USE_NARODMON)
  {
    static unsigned long updateTime = millis() - 20000;
    if (millis() - updateTime >= timeout)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        readSensors();
        if (sendToNarodmon())
        {
          timeout = 60000 * 5;
        }
        else
        {
          timeout = 20000;
        }
      }
      else
      {
        timeout = 20000;
        Serial.println(F("ERROR: Wi-Fi not connected."));
      }
      updateTime = millis();
    }
  }

  if (USE_OWNSERVER)
  {
    static unsigned long updateTime2 = millis() - 30000;
    if (millis() - updateTime2 >= timeout2)
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        readSensors();
        if (sendToLocalServer())
        {
          timeout2 = 60000 * 5;
        }
        else
        {
          timeout2 = 20000;
        }
      }
      else
      {
        timeout2 = 20000;
        Serial.println(F("ERROR: Wi-Fi not connected."));
      }
      updateTime2 = millis();
    }
  }
}

/*******************************************************************************
* Function Name  : WiFiEvent
* Description    : Обработка изменений состояния Wi-Fi соединения.
*                  Вызывается автоматически.
*******************************************************************************/
 
void WiFiEvent(WiFiEvent_t event) 
{
  Serial.printf("[WiFi-event] event: %d\n", event);
 
  switch(event)
  {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println(F("[WiFi-event] WiFi connected"));
      Serial.print(F("IP address: "));
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println(F("[WiFi-event] WiFi lost connection"));
      break;
  }
}
