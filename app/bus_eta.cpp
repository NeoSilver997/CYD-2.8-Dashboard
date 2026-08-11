#include "bus_eta.h"
#include "config.h"
#include "app_data.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

static const uint32_t INTERVAL_MS = 60UL * 1000;   // 1 min

#define BUS_DUMMY 1

static const char* BUS_API_URL = "https://bus-eta-sage.vercel.app/api/stops/4A84482F3A54E4CC/eta";

static uint32_t nextAttemptMs = 0;
static uint32_t backoffMin    = 0;

static void seedDummy() {
  g_data.busCount = 8;
  strncpy(g_data.busRoutes[0].route, "290",  sizeof(g_data.busRoutes[0].route) - 1);
  strncpy(g_data.busRoutes[0].dest,  "荃灣西站", sizeof(g_data.busRoutes[0].dest) - 1);
  strncpy(g_data.busRoutes[0].eta1, "02:18", sizeof(g_data.busRoutes[0].eta1) - 1);
  strncpy(g_data.busRoutes[0].eta2, "02:35", sizeof(g_data.busRoutes[0].eta2) - 1);

  strncpy(g_data.busRoutes[1].route, "290X", sizeof(g_data.busRoutes[1].route) - 1);
  strncpy(g_data.busRoutes[1].dest,  "荃灣西站", sizeof(g_data.busRoutes[1].dest) - 1);
  strncpy(g_data.busRoutes[1].eta1, "02:22", sizeof(g_data.busRoutes[1].eta1) - 1);

  strncpy(g_data.busRoutes[2].route, "93M", sizeof(g_data.busRoutes[2].route) - 1);
  strncpy(g_data.busRoutes[2].dest,  "寶林", sizeof(g_data.busRoutes[2].dest) - 1);
  strncpy(g_data.busRoutes[2].eta1, "02:25", sizeof(g_data.busRoutes[2].eta1) - 1);

  strncpy(g_data.busRoutes[3].route, "95", sizeof(g_data.busRoutes[3].route) - 1);
  strncpy(g_data.busRoutes[3].dest,  "九龍站", sizeof(g_data.busRoutes[3].dest) - 1);
  strncpy(g_data.busRoutes[3].eta1, "02:30", sizeof(g_data.busRoutes[3].eta1) - 1);

  strncpy(g_data.busRoutes[4].route, "N293", sizeof(g_data.busRoutes[4].route) - 1);
  strncpy(g_data.busRoutes[4].dest,  "旺角(柏景灣)", sizeof(g_data.busRoutes[4].dest) - 1);
  strncpy(g_data.busRoutes[4].eta1, "02:45", sizeof(g_data.busRoutes[4].eta1) - 1);

  strncpy(g_data.busRoutes[5].route, "N691", sizeof(g_data.busRoutes[5].route) - 1);
  strncpy(g_data.busRoutes[5].dest,  "中環(港澳碼頭)", sizeof(g_data.busRoutes[5].dest) - 1);
  strncpy(g_data.busRoutes[5].eta1, "03:02", sizeof(g_data.busRoutes[5].eta1) - 1);

  strncpy(g_data.busRoutes[6].route, "98C", sizeof(g_data.busRoutes[6].route) - 1);
  strncpy(g_data.busRoutes[6].dest,  "美孚", sizeof(g_data.busRoutes[6].dest) - 1);
  g_data.busRoutes[6].eta1[0] = '\0';

  strncpy(g_data.busRoutes[7].route, "290A", sizeof(g_data.busRoutes[7].route) - 1);
  strncpy(g_data.busRoutes[7].dest,  "荃灣西站", sizeof(g_data.busRoutes[7].dest) - 1);
  g_data.busRoutes[7].eta1[0] = '\0';

  g_data.busUpdatedAt = (uint32_t)time(nullptr);
  g_data.busValid = true;
  Serial.println("bus: dummy data seeded");
}

static bool doFetch() {
  Serial.println("bus: fetch starting");

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  IPAddress ip;
  Serial.printf("bus: resolving bus-eta-sage.vercel.app ... ");
  if (!WiFi.hostByName("bus-eta-sage.vercel.app", ip) || ip == INADDR_NONE || ip == IPAddress(0, 0, 0, 0)) {
    Serial.println("FAILED, trying explicit DNS (8.8.8.8)");
    IPAddress dns1(8, 8, 8, 8);
    IPAddress dns2(1, 1, 1, 1);
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
    delay(200);
    if (!WiFi.hostByName("bus-eta-sage.vercel.app", ip) || ip == INADDR_NONE || ip == IPAddress(0, 0, 0, 0)) {
      Serial.println("FAILED");
      return false;
    }
  }
  Serial.printf("ok -> %s\n", ip.toString().c_str());

  HTTPClient https;
  if (!https.begin(client, BUS_API_URL)) {
    Serial.println("bus: https.begin failed");
    return false;
  }
  https.setTimeout(15000);

  int code = https.GET();
  if (code != 200) {
    Serial.printf("bus: HTTP %d\n", code);
    https.end(); client.stop();
    return false;
  }
  Serial.printf("bus: HTTP %d\n", code);

  String payload = https.getString();
  https.end(); client.stop();
  Serial.printf("bus: payload %d bytes\n", (int)payload.length());

  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("bus: JSON parse failed: %s\n", err.c_str());
    return false;
  }

  JsonArray arr = doc["data"];
  if (arr.isNull()) {
    Serial.println("bus: no data array");
    return false;
  }
  Serial.printf("bus: data array size %d\n", (int)arr.size());

  g_data.busCount = 0;
  for (JsonObject item : arr) {
    if (g_data.busCount >= AppData::BUS_MAX) {
      Serial.println("bus: hit BUS_MAX");
      break;
    }

    AppData::BusRoute& r = g_data.busRoutes[g_data.busCount];
    const char* route = item["route"] | "";
    const char* dest = item["dest_tc"] | item["dest_en"] | item["dest_sc"] | "";
    strncpy(r.route, route, sizeof(r.route) - 1);
    strncpy(r.dest, dest, sizeof(r.dest) - 1);
    Serial.printf("bus: route=%s dest=%s\n", r.route, r.dest);

    r.eta1[0] = '\0';
    r.eta2[0] = '\0';
    r.remark1[0] = '\0';
    r.remark2[0] = '\0';

    JsonArray eta = item["eta"];
    if (eta.isNull()) {
      Serial.printf("bus: route %s eta array null\n", r.route);
      g_data.busCount++;
      continue;
    }
    Serial.printf("bus: route %s eta count %d\n", r.route, (int)eta.size());

    if (eta.size() > 0) {
      const char* e = eta[0]["eta"];
      Serial.printf("bus: route %s eta[0]=%s\n", r.route, e ? e : "null");
      if (e && strlen(e) >= 16) {
        snprintf(r.eta1, sizeof(r.eta1), "%c%c:%c%c", e[11], e[12], e[14], e[15]);
      }
      const char* rm = eta[0]["remark_en"];
      if (rm && rm[0]) strncpy(r.remark1, rm, sizeof(r.remark1) - 1);
    }
    if (eta.size() > 1) {
      const char* e = eta[1]["eta"];
      Serial.printf("bus: route %s eta[1]=%s\n", r.route, e ? e : "null");
      if (e && strlen(e) >= 16) {
        snprintf(r.eta2, sizeof(r.eta2), "%c%c:%c%c", e[11], e[12], e[14], e[15]);
      }
      const char* rm = eta[1]["remark_en"];
      if (rm && rm[0]) strncpy(r.remark2, rm, sizeof(r.remark2) - 1);
    }

    g_data.busCount++;
  }

  g_data.busUpdatedAt = (uint32_t)time(nullptr);
  g_data.busValid = true;

  Serial.printf("bus: %d routes fetched  heap %u\n", g_data.busCount, ESP.getFreeHeap());
  return true;
}

void busEta_begin() {
  nextAttemptMs = 0;
  backoffMin = 0;
#if BUS_DUMMY
  seedDummy();
#endif
  Serial.printf("bus: TLS test begin... ");
  {
    WiFiClientSecure tlsTest;
    tlsTest.setInsecure();
    tlsTest.setTimeout(5000);
    HTTPClient h;
    if (h.begin(tlsTest, "https://api.open-meteo.com") && h.GET() > 0) {
      Serial.printf("HTTP %d\n", h.GET());
      h.end();
    } else {
      Serial.println("FAILED");
    }
    h.end();
  }
  Serial.println("bus: TLS test done");
}

void busEta_tick() {
  if ((int32_t)(millis() - nextAttemptMs) < 0) return;

  if (WiFi.status() != WL_CONNECTED) {
    nextAttemptMs = millis() + 30000;
    return;
  }

  if (doFetch()) {
    backoffMin = 0;
    nextAttemptMs = millis() + INTERVAL_MS;
  } else {
    backoffMin = (backoffMin == 0) ? 1 : backoffMin * 2;
    if (backoffMin > 15) backoffMin = 15;
    nextAttemptMs = millis() + backoffMin * 60UL * 1000;
    Serial.printf("bus: fetch failed, retry in %u min\n", backoffMin);
#if BUS_DUMMY
    if (!g_data.busValid) seedDummy();
#endif
  }
}
