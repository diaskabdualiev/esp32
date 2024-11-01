#include <Arduino.h>
#include <Update.h>
#include <WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "secrets.h"

#define COLUMS           20   //LCD columns
#define ROWS             4    //LCD rows
#define LCD_SPACE_SYMBOL 0x20 //space symbol from LCD ROM, see p.9 of GDM2004D datasheet

// #define WIFI_SSID "TP-Link_6A2A"
// #define WIFI_PASSWORD "12624782"

// #define API_KEY "AIzaSyCdZvy2cIOA-ULtQoiGMY25nBVod-CZ6rY"
// #define USER_EMAIL "refill@mail.com"
// #define USER_PASSWORD "refill"
// #define DATABASE_URL "https://diastest-d6240-default-rtdb.europe-west1.firebasedatabase.app/"

void asyncCB(AsyncResult &aResult);
void printResult(AsyncResult &aResult);

DefaultNetwork network;
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl_client1, ssl_client2;
using AsyncClient = AsyncClientClass;
AsyncClient aClient1(ssl_client1, getNetwork(network)), aClient2(ssl_client2, getNetwork(network));
RealtimeDatabase Database;
bool taskComplete = false;
bool firstInit = false;
unsigned long ms = 0;

LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);

String deviceID;

object_t createInitVaraibles(){
  JsonWriter writer;
  object_t acc, json;
  object_t pr1, pr2, pr3, pr4, pr5, pr6, pr7, prices;
  object_t nm1, nm2, nm3, nm4, nm5, nm6, nm7, names;
  object_t tk1, tk2, tk3, tk4, tk5, tk6, tk7, tanks;

  writer.create(acc, "account", 5000);

  writer.create(pr1, "price1", 55);
  writer.create(pr2, "price2", 55);
  writer.create(pr3, "price3", 55);
  writer.create(pr4, "price4", 55);
  writer.create(pr5, "price5", 55);
  writer.create(pr6, "price6", 55);
  writer.create(pr7, "price7", 55);
  writer.join(prices, 7, pr1, pr2, pr3, pr4, pr5, pr6, pr7);
  writer.create(prices, "prices", prices);

  writer.create(nm1, "name1", "Adk");
  writer.create(nm2, "name2", "Sem");
  writer.create(nm3, "name3", "Dis");
  writer.create(nm4, "name4", "Nur");
  writer.create(nm5, "name5", "Loa");
  writer.create(nm6, "name6", "Iha");
  writer.create(nm7, "name7", "Gbh");
  writer.join(names, 7, nm1, nm2, nm3, nm4, nm5, nm6, nm7);
  writer.create(names, "names", names);  

  writer.create(tk1, "tank1", 2);
  writer.create(tk2, "tank2", 3);
  writer.create(tk3, "tank3", 1);
  writer.create(tk4, "tank4", 4);
  writer.create(tk5, "tank5", 2);
  writer.create(tk6, "tank6", 1);
  writer.create(tk7, "tank7", 3);
  writer.join(tanks, 7, tk1, tk2, tk3, tk4, tk5, tk6, tk7);
  writer.create(tanks, "tanks", tanks);
  writer.join(json, 4, prices, names, tanks, acc);
  writer.create(json, deviceID, json);
  return json;
}

void displayScrollingText(const char* text, uint8_t row)
{
    size_t len = strlen(text);
    if (len <= 20)
    {
        lcd.setCursor(0, row);
        lcd.print(text);
    }
    else
    {
        for (size_t i = 0; i <= len - 20; i++)
        {
            lcd.setCursor(0, row);
            lcd.print(text + i);
            delay(300);
        }
    }
}

void setup()
{

    Serial.begin(115200);

    while (lcd.begin(COLUMS, ROWS, LCD_5x8DOTS) != 1) //colums, rows, characters size
    {
      Serial.println(F("PCF8574 is not connected or lcd pins declaration is wrong. Only pins numbers: 4,5,6,16,11,12,13,14 are legal."));
      delay(5000);   
    }
    lcd.print(F("PCF8574 is OK..."));
    delay(2000);
    lcd.clear();


    lcd.setCursor(0, 0);
    lcd.print("Connecting to WiFi");
    displayScrollingText(WIFI_SSID, 1);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int dotCount = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        lcd.setCursor(0, 2);
        lcd.print("Status: ");
        for (int i = 0; i <= dotCount; i++)
        {
            lcd.print(".");
        }
        dotCount = (dotCount + 1) % 10; // Обновление количества точек (0-9)
        delay(500);
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print("IP:");
    lcd.print(WiFi.localIP());
    delay(2000);
    lcd.clear();

    deviceID = WiFi.macAddress();
    deviceID.replace(":", "");
    lcd.setCursor(0, 0);
    lcd.print("Wifi MAC Address");
    lcd.setCursor(0, 1);
    lcd.print(deviceID);
    delay(2000);
    lcd.clear();
    Serial.println(deviceID);

    lcd.setCursor(0, 0);
    lcd.print("Firebase Client");
    lcd.setCursor(0, 1);
    lcd.print("Version:");
    lcd.print(FIREBASE_CLIENT_VERSION);
    delay(2000);
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("+7 702 313 20 61");
    lcd.setCursor(0, 1);
    lcd.print("Balance:");

    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    Serial.println("Initializing app...");
    ssl_client1.setInsecure();
    ssl_client2.setInsecure();
    initializeApp(aClient1, app, getAuth(user_auth), asyncCB, "authTask");

    // Binding the FirebaseApp for authentication handler.
    // To unbind, use Database.resetApp();
    app.getApp<RealtimeDatabase>(Database);
    initializeApp(aClient2, app, getAuth(user_auth), asyncCB, "authTask");
    Database.url(DATABASE_URL);
    Database.setSSEFilters("get,put,patch,keep-alive,cancel,auth_revoked");
    Database.get(aClient2, "/"+deviceID, asyncCB, true /* SSE mode (HTTP Streaming) */, "streamTask");

}

void loop()
{
  // The async task handler should run inside the main loop
  // without blocking delay or bypassing with millis code blocks.
  app.loop();
  Database.loop();

  if (app.ready() && !taskComplete){
    Database.get(aClient2, "/" + deviceID + "/account", asyncCB, false, "getInitInfo");
    taskComplete = true;
  }

  if(app.ready() && firstInit){
    Database.set<object_t>(aClient2, "/", createInitVaraibles(), asyncCB, "setInitInfo");
    lcd.setCursor(0, 3);
    lcd.print("firstInit");
    firstInit = false;
  }

  

}

void asyncCB(AsyncResult &aResult)
{
    // WARNING!
    // Do not put your codes inside the callback and printResult.

    printResult(aResult);
}

void printResult(AsyncResult &aResult)
{
    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available())
    {
        RealtimeDatabaseResult &RTDB = aResult.to<RealtimeDatabaseResult>();
        if (RTDB.isStream())
        {
            Serial.println("----------------------------");
            Firebase.printf("task: %s\n", aResult.uid().c_str());
            Firebase.printf("event: %s\n", RTDB.event().c_str());
            Firebase.printf("path: %s\n", RTDB.dataPath().c_str());
            Firebase.printf("data: %s\n", RTDB.to<const char *>());
            Firebase.printf("type: %d\n", RTDB.type());

            // The stream event from RealtimeDatabaseResult can be converted to the values as following.
            bool v1 = RTDB.to<bool>();
            int v2 = RTDB.to<int>();
            float v3 = RTDB.to<float>();
            double v4 = RTDB.to<double>();
            String v5 = RTDB.to<String>();
            String payload = RTDB.dataPath().c_str();
            payload.trim();
            if(payload == "/account"){
              lcd.setCursor(8, 1);
              lcd.print(RTDB.to<const char *>());
            }
        }
        else
        {
            Serial.println("----------------------------");
            Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
            String payload = aResult.c_str();
            payload.trim();   
            if(aResult.uid() == "getInitInfo" && payload == "null"){
              firstInit = true;
            }else if(aResult.uid() == "getInitInfo"){
              lcd.setCursor(8, 1);
              lcd.print(payload);
            }
        }

#if defined(ESP32) || defined(ESP8266)
        Firebase.printf("Free Heap: %d\n", ESP.getFreeHeap());
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
        Firebase.printf("Free Heap: %d\n", rp2040.getFreeHeap());
#endif
    }
}


