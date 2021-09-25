#include <Arduino_JSON.h>
#include <time.h>
extern "C" {
#include "user_interface.h"
}
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"

typedef struct {
  String time;
  float value;
} SensorLog;

#define CHART_NAME "Rubber Tree"
#define CHART_COLOR "#FF6384"
#define LOG_LENGTH 28

SensorLog sensorLogs[LOG_LENGTH];

String lastRunnedAtDate = "";

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < LOG_LENGTH; i++) {
    sensorLogs[i] = {"", 0.0};
  }
}

void loop() {
  if (!startWiFi()) {
    delay(60000);
    return;
  }
  configTzTime("JST-9", "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  delay(10000);
  push(getCurrentTimeLabel(), readSensor());
  postImage(generateChartImageUrl());
  int sleepTime = 86400 - getStartOfSecond();
  stopWiFi();
  for (int i = 0; i < sleepTime; i++) {
    delay(1000);
  }
}

bool startWiFi() {
  WiFi.begin(wifiSsid, wifiPassword);
  int retry = 10;
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    if (retry-- < 0) {
      return false;
    }
  }
  return true;
}

void push(String time, float value) {
  for (int i = 1; i < LOG_LENGTH; i++) {
    sensorLogs[i - 1].time = sensorLogs[i].time;
    sensorLogs[i - 1].value = sensorLogs[i].value;
  }
  sensorLogs[LOG_LENGTH - 1].time = time;
  sensorLogs[LOG_LENGTH - 1].value = value;
}

void stopWiFi() { WiFi.disconnect(); }

void postImage(String imageUrl) {
  String body =
      "token=" + String(slackToken) + "&channel=" + String(slackChannel) +
      "&as_user=true&blocks=\%5B\%7B\%22type\%22\%3A\%22image\%22\%2C\%22image_"
      "url\%22\%3A\%22" +
      encodeURIComponent(imageUrl) +
      "\%22\%2C\%22alt_text\%22\%3A\%22chart\%22\%7D\%5D";
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  client.connect("slack.com", 443);
  client.println("POST /api/chat.postMessage HTTP/1.1");
  client.println("Host: slack.com");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Content-Length: " + String(body.length()));
  client.println();
  client.println(body);
  client.println();
  delay(10000);
  client.stop();
}

float readSensor() {
  int repeat = 1000;
  float value = 0;
  for (int i = 0; i < repeat; i++) {
    value = value + system_adc_read();
    delay(1);
  }
  value = value / (float)repeat;
  return value > 1000 ? 1000 : value;
}

String getCurrentTimeLabel() {
  time_t t;
  struct tm *tm;
  static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

  t = time(NULL);
  tm = localtime(&t);
  char buf[32];
  sprintf(buf, "%02d/%02d(%s) %02d:%02d", tm->tm_mon + 1, tm->tm_mday,
          wd[tm->tm_wday], tm->tm_hour, tm->tm_min);
  return String(buf);
}

int getStartOfSecond() {
  time_t t;
  struct tm *tm;
  t = time(NULL);
  tm = localtime(&t);
  return tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

String generateChartImageUrl() {
  JSONVar body;
  body["type"] = "line";
  for (int i = 0; i < LOG_LENGTH; i++) {
    body["data"]["labels"][i] = sensorLogs[i].time;
  }
  body["data"]["datasets"][0]["label"] = CHART_NAME;
  body["data"]["datasets"][0]["backgroundColor"] = CHART_COLOR;
  body["data"]["datasets"][0]["borderColor"] = CHART_COLOR;
  body["data"]["datasets"][0]["fill"] = false;
  body["options"]["scales"]["yAxes"][0]["ticks"]["min"] = 0;
  body["options"]["scales"]["yAxes"][0]["ticks"]["max"] = 1000;
  body["options"]["scales"]["yAxes"][0]["ticks"]["stepSize"] = 100;
  for (int i = 0; i < LOG_LENGTH; i++) {
    body["data"]["datasets"][0]["data"][i] = sensorLogs[i].value;
  }
  String c = JSON.stringify(body);
  return "https://quickchart.io/chart?w=960&h=360&bkg=%23ffffff&c=" +
         encodeURIComponent(c);
}

String encodeURIComponent(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '!' ||
               c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
    yield();
  }
  return encodedString;
}
