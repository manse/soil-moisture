extern "C"
{
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"

typedef struct
{
  String key;
  String value;
} Entry;

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  if (!startWifi())
  {
    delay(60 * 1000);
    return;
  }

  int readCount = 1000;
  float sensorValue = 0;
  for (int i = 0; i < readCount; i++)
  {
    sensorValue = sensorValue + system_adc_read();
    delay(1);
  }
  sensorValue = sensorValue / (float)readCount;
  delay(1000);
  postMessage(String(sensorValue));
  Serial.println(sensorValue);
  delay(3 * 3600 * 1000);
}

bool startWifi()
{
  WiFi.begin(wifiSsid, wifiPassword);
  int retry = 10;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(2000);
    if (retry-- < 0)
    {
      return false;
    }
  }
  Serial.println(WiFi.localIP());
  return true;
}

void postMessage(String message)
{
  WiFiClientSecure client;
  client.setFingerprint(fingerPrint);
  if (!client.connect("slack.com", 443))
  {
    return;
  }

  String boundary = "MY_FRIEND_DUFFY";
  Entry params[] = {
      {"token", String(slackToken)},
      {"channel", String(slackChannel)},
      {"as_user", "true"},
      {"text", message}};
  String payload = buildMultipartPayload(params, boundary);
  String request = "POST /api/chat.postMessage HTTP/1.1\n";
  request += "Host: slack.com\n";
  request += "Content-Type: multipart/form-data; boundary=" + boundary + "\n";
  request += "Content-Length: " + String(payload.length()) + "\n\n";
  request += payload;
  client.println(request);
  Serial.println(request);
  delay(10 * 1000);
  // while (client.available())
  // {
  //   String line = client.readStringUntil('\r');
  //   Serial.print(line);
  // }
  client.stop();
}

String buildMultipartPayload(Entry params[], String boundary)
{
  String result = "";
  int length = sizeof(params);
  for (int i = 0; i < length; i++)
  {
    String key = params[i].key;
    String value = params[i].value;
    result += "--" + boundary + "\n";
    result += "Content-Disposition: form-data; name=\"" + key + "\"\n\n";
    result += value + "\n";
  }
  result += "--" + boundary + "--";
  return result;
}
