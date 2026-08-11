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

static const char* BUS_STOP_1 = "4A84482F3A54E4CC";
static const char* BUS_STOP_2 = "ABC15E2110BF7A49";

static uint32_t nextAttemptMs = 0;
static uint32_t backoffMin    = 0;

static void seedDummy() {
  g_data.busCount = 10;
  strncpy(g_data.busRoutes[0].route, "290",  sizeof(g_data.busRoutes[0].route) - 1);
  strncpy(g_data.busRoutes[0].dest,  "TSUEN WAN WEST STATION", sizeof(g_data.busRoutes[0].dest) - 1);
  strncpy(g_data.busRoutes[0].eta1, "02:18", sizeof(g_data.busRoutes[0].eta1) - 1);
  strncpy(g_data.busRoutes[0].eta2, "02:35", sizeof(g_data.busRoutes[0].eta2) - 1);

  strncpy(g_data.busRoutes[1].route, "290X", sizeof(g_data.busRoutes[1].route) - 1);
  strncpy(g_data.busRoutes[1].dest,  "TSUEN WAN WEST STATION", sizeof(g_data.busRoutes[1].dest) - 1);
  strncpy(g_data.busRoutes[1].eta1, "02:22", sizeof(g_data.busRoutes[1].eta1) - 1);

  strncpy(g_data.busRoutes[2].route, "93M", sizeof(g_data.busRoutes[2].route) - 1);
  strncpy(g_data.busRoutes[2].dest,  "PO LAM", sizeof(g_data.busRoutes[2].dest) - 1);
  strncpy(g_data.busRoutes[2].eta1, "02:25", sizeof(g_data.busRoutes[2].eta1) - 1);

  strncpy(g_data.busRoutes[3].route, "95", sizeof(g_data.busRoutes[3].route) - 1);
  strncpy(g_data.busRoutes[3].dest,  "KOWLOON STATION", sizeof(g_data.busRoutes[3].dest) - 1);
  strncpy(g_data.busRoutes[3].eta1, "02:30", sizeof(g_data.busRoutes[3].eta1) - 1);

  strncpy(g_data.busRoutes[4].route, "N293", sizeof(g_data.busRoutes[4].route) - 1);
  strncpy(g_data.busRoutes[4].dest,  "MONG KOK (PARK AVENUE)", sizeof(g_data.busRoutes[4].dest) - 1);
  strncpy(g_data.busRoutes[4].eta1, "02:45", sizeof(g_data.busRoutes[4].eta1) - 1);

  strncpy(g_data.busRoutes[5].route, "101", sizeof(g_data.busRoutes[5].route) - 1);
  strncpy(g_data.busRoutes[5].dest,  "TAI HANG", sizeof(g_data.busRoutes[5].dest) - 1);
  strncpy(g_data.busRoutes[5].eta1, "02:10", sizeof(g_data.busRoutes[5].eta1) - 1);

  strncpy(g_data.busRoutes[6].route, "103", sizeof(g_data.busRoutes[6].route) - 1);
  strncpy(g_data.busRoutes[6].dest,  "KWUN TONG", sizeof(g_data.busRoutes[6].dest) - 1);
  g_data.busRoutes[6].eta1[0] = '\0';

  strncpy(g_data.busRoutes[7].route, "290X", sizeof(g_data.busRoutes[7].route) - 1);
  strncpy(g_data.busRoutes[7].dest,  "TSUEN WAN WEST STATION", sizeof(g_data.busRoutes[7].dest) - 1);
  strncpy(g_data.busRoutes[7].eta1, "02:50", sizeof(g_data.busRoutes[7].eta1) - 1);

  strncpy(g_data.busRoutes[8].route, "N691", sizeof(g_data.busRoutes[8].route) - 1);
  strncpy(g_data.busRoutes[8].dest,  "CENTRAL (MACAO FERRY)", sizeof(g_data.busRoutes[8].dest) - 1);
  strncpy(g_data.busRoutes[8].eta1, "03:02", sizeof(g_data.busRoutes[8].eta1) - 1);

  strncpy(g_data.busRoutes[9].route, "988", sizeof(g_data.busRoutes[9].route) - 1);
  strncpy(g_data.busRoutes[9].dest,  "TUNG CHUNG", sizeof(g_data.busRoutes[9].dest) - 1);
  g_data.busRoutes[9].eta1[0] = '\0';

  g_data.busUpdatedAt = (uint32_t)time(nullptr);
  g_data.busValid = true;
  Serial.println("bus: dummy data seeded");
}

static void busDedup() {
  int w = 0;
  for (int i = 0; i < g_data.busCount; i++) {
    bool dup = false;
    for (int j = 0; j < w; j++) {
      if (strcmp(g_data.busRoutes[i].route, g_data.busRoutes[j].route) == 0 &&
          strcmp(g_data.busRoutes[i].dest, g_data.busRoutes[j].dest) == 0) {
        if (g_data.busRoutes[i].eta1[0] != '\0' && g_data.busRoutes[j].eta1[0] == '\0') {
          g_data.busRoutes[j] = g_data.busRoutes[i];
        }
        dup = true;
        break;
      }
    }
    if (!dup) {
      if (w != i) g_data.busRoutes[w] = g_data.busRoutes[i];
      w++;
    }
  }
  g_data.busCount = w;
}

static bool fetchOneStop(const char* stopId) {
  String url = String("https://bus-eta-sage.vercel.app/api/stops/") + stopId + "/eta";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.printf("bus: https.begin failed for stop %s\n", stopId);
    return false;
  }
  https.setTimeout(15000);

  int code = https.GET();
  if (code != 200) {
    Serial.printf("bus: stop %s HTTP %d\n", stopId, code);
    https.end(); client.stop();
    return false;
  }

  String payload = https.getString();
  https.end(); client.stop();

  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("bus: stop %s JSON parse failed: %s\n", stopId, err.c_str());
    return false;
  }

  JsonArray arr = doc["data"];
  if (arr.isNull()) {
    Serial.printf("bus: stop %s no data array\n", stopId);
    return false;
  }

  int startCount = g_data.busCount;
  for (JsonObject item : arr) {
    if (g_data.busCount >= AppData::BUS_MAX) {
      Serial.println("bus: hit BUS_MAX");
      break;
    }

    AppData::BusRoute& r = g_data.busRoutes[g_data.busCount];
    const char* route = item["route"] | "";
    const char* dest = item["dest_en"] | item["dest_sc"] | "";
    strncpy(r.route, route, sizeof(r.route) - 1);
    strncpy(r.dest, dest, sizeof(r.dest) - 1);

    r.eta1[0] = '\0';
    r.eta2[0] = '\0';
    r.remark1[0] = '\0';
    r.remark2[0] = '\0';

    JsonArray eta = item["eta"];
    if (!eta.isNull() && eta.size() > 0) {
      const char* e = eta[0]["eta"];
      if (e && strlen(e) >= 16) {
        snprintf(r.eta1, sizeof(r.eta1), "%c%c:%c%c", e[11], e[12], e[14], e[15]);
      }
      const char* rm = eta[0]["remark_en"];
      if (rm && rm[0]) strncpy(r.remark1, rm, sizeof(r.remark1) - 1);
    }
    if (!eta.isNull() && eta.size() > 1) {
      const char* e = eta[1]["eta"];
      if (e && strlen(e) >= 16) {
        snprintf(r.eta2, sizeof(r.eta2), "%c%c:%c%c", e[11], e[12], e[14], e[15]);
      }
      const char* rm = eta[1]["remark_en"];
      if (rm && rm[0]) strncpy(r.remark2, rm, sizeof(r.remark2) - 1);
    }

    g_data.busCount++;
  }

  Serial.printf("bus: stop %s added %d routes\n", stopId, g_data.busCount - startCount);
  return true;
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

  g_data.busCount = 0;

  bool ok1 = fetchOneStop(BUS_STOP_1);
  bool ok2 = fetchOneStop(BUS_STOP_2);

  if (!ok1 && !ok2) {
    Serial.println("bus: both stops failed");
    return false;
  }

  busDedup();

  g_data.busUpdatedAt = (uint32_t)time(nullptr);
  g_data.busValid = true;

  Serial.printf("bus: total %d unique routes after dedup  heap %u\n", g_data.busCount, ESP.getFreeHeap());
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
