#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_SCD30.h>

#include "config.h"

#define AQ_QUALITY_TOPIC BASE_TOPIC_AQ "quality"
#define AQ_CO2_TOPIC BASE_TOPIC_AQ "co2"
#define TEMPERATURE_CURRENT_TOPIC BASE_TOPIC_TEMPERATURE "current"
#define HUMIDITY_CURRENT_TOPIC BASE_TOPIC_HUMIDITY "current"

#ifndef CLIENT_ID
#define CLIENT_ID "air_quality_meter"
#endif

WiFiClient espClient;
PubSubClient client(espClient);
char msg[64];
Adafruit_SCD30 scd30;

const char *normal = "NORMAL";
const char *abnormal = "ABNORMAL";
const char *online = "ONLINE";

const char *unknown = "UNKNOWN";
const char *excellent = "EXCELLENT";
const char *good = "GOOD";
const char *fair = "FAIR";
const char *inferior = "INFERIOR";
const char *poor = "POOR";

const char *co2_ppm_to_quality(float ppm)
{
  if (scd30.CO2 >= 400.0 && scd30.CO2 < 600)
  {
    return excellent;
  }
  else if (scd30.CO2 < 800)
  {
    return good;
  }
  else if (scd30.CO2 < 1000)
  {
    return fair;
  }
  else if (scd30.CO2 < 1200)
  {
    return inferior;
  }
  else if (scd30.CO2 <= 10000)
  {
    return poor;
  }
  return unknown;
}

void setup_scd30()
{
  Serial.print("Setting up SCD30: ");
  while (!scd30.begin())
  {
    Serial.print(".");
    delay(250);
  }
  Serial.println("done");

  if (!scd30.setMeasurementInterval(2))
  {
    Serial.println("Failed to set measurement interval");
    while (1)
    {
      delay(10);
    }
  }

  Serial.printf("Measurement Interval: %d seconds\n", scd30.getMeasurementInterval());
}

void setup_wifi()
{
  Serial.printf("Connecting to WiFi: %s\n", SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PSK);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.println("Connecting to MQTT broker...");
    if (!client.connect(CLIENT_ID))
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
  client.subscribe("foo");
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  setup_wifi();
  setup_scd30();
  client.setServer(MQTT_HOST, MQTT_PORT);
}

void loop()
{
  sensors_event_t temp;
  sensors_event_t humidity;

  while (!scd30.dataReady())
  {
    Serial.print(".");
    delay(10);
  }

  if (!scd30.getEvent(&humidity, &temp))
  {
    Serial.println("Failed to read sensor data");
    delay(1000);
    return;
  }

  Serial.printf("CO2: %f ppm\t Temp: %f C\tHumidity: %f\n", scd30.CO2, temp.temperature, humidity.relative_humidity);

  if (WiFi.status() != WL_CONNECTED)
  {
    setup_wifi();
  }

  if (!client.connected())
  {
    reconnect();
  }

  memset(msg, 0, sizeof(msg));
  snprintf(msg, sizeof(msg), "%f", scd30.CO2);
  client.publish(AQ_CO2_TOPIC, msg);
  client.publish(AQ_QUALITY_TOPIC, co2_ppm_to_quality(scd30.CO2));

  memset(msg, 0, sizeof(msg));
  snprintf(msg, sizeof(msg), "%f", temp.temperature);
  client.publish(TEMPERATURE_CURRENT_TOPIC, msg);

  memset(msg, 0, sizeof(msg));
  snprintf(msg, sizeof(msg), "%f", humidity.relative_humidity);
  client.publish(HUMIDITY_CURRENT_TOPIC, msg);

  client.loop();

  if (SLEEP)
  {
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(SLEEP_TIME_MS);
  }
  else
  {
    delay(1000);
  }
}
