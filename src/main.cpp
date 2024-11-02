#include <Arduino.h>
#include <Update.h>
#include <WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "secrets.h"
#include "var.h"
#include "PCF8575.h"
#include "FirebaseJson.h"
struct Names {
  String name1;
  String name2;
  String name3;
  String name4;
  String name5;
  String name6;
  String name7;
};

struct Prices {
  int price1;
  int price2;
  int price3;
  int price4;
  int price5;
  int price6;
  int price7;
};

struct Tanks {
  float tank1;
  float tank2;
  float tank3;
  float tank4;
  float tank5;
  float tank6;
  float tank7;
};
Names names;
Prices prices;
Tanks tanks;
DefaultNetwork network;
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl_client1, ssl_client2;
using AsyncClient = AsyncClientClass;
AsyncClient aClient1(ssl_client1, getNetwork(network)), aClient2(ssl_client2, getNetwork(network));
RealtimeDatabase Database;

int valuePot1 = 0;
int valuePot2 = 0;
int valuePot3 = 0;
int valuePot4 = 0;
int valuePot5 = 0;
int valuePot6 = 0;
int valuePot7 = 0;

bool taskComplete = false;
bool firstInit = false;
unsigned long ms = 0;
int currentSelect = 0;

LiquidCrystal_I2C lcd(PCF8574_ADDR_A21_A11_A01, 4, 5, 6, 16, 11, 12, 13, 14, POSITIVE);

String deviceID;

void asyncCB(AsyncResult &aResult);
void printResult(AsyncResult &aResult);
void beepBuzzer();
void handleButtonClick(uint8_t pin);
void displayScrollingText(const char* text, uint8_t row);
object_t createInitVaraibles();
object_t createPotJson();
void printAccount(String account);
void getAllNameAndPrice(String jsonString);
void buttonLoop(unsigned long currentMillis);
void potLoop(unsigned long currentMillis);
void printNameAndPrice(String name, int prices, float potvalue);
void POTinit();
void LEDinit();
PCF8575 pcf8575(PCF8575_ADDRESS);

const uint8_t buttonPins[] = {
  BUTTON1, BUTTON2, BUTTON3, BUTTON4, BUTTON5, BUTTON6, BUTTON7, START_BUTTON, STOP_BUTTON
};
const uint8_t numButtons = sizeof(buttonPins) / sizeof(buttonPins[0]);

bool buttonStates[numButtons];
bool buttonLastStates[numButtons];

unsigned long buzzerStartTime = 0;
const unsigned long buzzerDuration = 100;
bool buzzerOn = false;

unsigned long lastButtonCheckTime = 0;
const unsigned long buttonCheckInterval = 50;

unsigned long lastPotCheck = 0;
unsigned long lastCountCheck = 0;
unsigned long lastPotSendCheck = 0;
unsigned long count = 0;

int account = 0;

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

    LEDinit();
    POTinit();
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

    // lcd.setCursor(0, 0);
    // lcd.print("+7 702 313 20 61");
    lcd.setCursor(0, 0);
    lcd.print("Balance:");

    for (uint8_t i = 0; i < numButtons; i++) {
      pcf8575.pinMode(buttonPins[i], INPUT);
      buttonStates[i] = false;
      buttonLastStates[i] = false;
    }
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    

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
    Database.get(aClient1, "/"+deviceID+"/account", asyncCB, true /* SSE mode (HTTP Streaming) */, "streamTask");

}

void loop()
{
  unsigned long currentMillis = millis();
  // The async task handler should run inside the main loop
  // without blocking delay or bypassing with millis code blocks.
  app.loop();
  Database.loop();
  buttonLoop(currentMillis);
  potLoop(currentMillis);

  if (app.ready() && !taskComplete){
    Database.get(aClient2, "/"+deviceID, asyncCB, false, "getInitInfo");
    taskComplete = true;
  }

  if(app.ready() && firstInit){
    Database.set<object_t>(aClient2, "/", createInitVaraibles(), asyncCB, "setInitInfo");
    lcd.setCursor(0, 3);
    lcd.print("firstInit");
    firstInit = false;
  }

  if(app.ready() && currentMillis - lastCountCheck >= 2000){
    lastCountCheck = currentMillis;
    Database.set<int>(aClient2, "/"+deviceID+"/count", count++, asyncCB, "setCountTask");
    Database.set<object_t>(aClient2, "/"+deviceID+"/tanks", createPotJson(), asyncCB, "setPotJsonTask");
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
            if(payload == "/"){
              printAccount(RTDB.to<const char *>());
            }
        }
        else
        {
            Serial.println("----------------------------");
            Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
            String task = aResult.uid().c_str();
            task.trim();
            if(task == "getInitInfo"){
              getAllNameAndPrice(RTDB.data());
            }
        }

#if defined(ESP32) || defined(ESP8266)
        Firebase.printf("Free Heap: %d\n", ESP.getFreeHeap());
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
        Firebase.printf("Free Heap: %d\n", rp2040.getFreeHeap());
#endif
    }
}

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

object_t createPotJson(){
  JsonWriter writer;
  object_t tk1, tk2, tk3, tk4, tk5, tk6, tk7, tan;
  writer.create(tk1, "tank1", tanks.tank1);
  writer.create(tk2, "tank2", tanks.tank2);
  writer.create(tk3, "tank3", tanks.tank3);
  writer.create(tk4, "tank4", tanks.tank4);
  writer.create(tk5, "tank5", tanks.tank5);
  writer.create(tk6, "tank6", tanks.tank6);
  writer.create(tk7, "tank7", tanks.tank7);
  writer.join(tan, 7, tk1, tk2, tk3, tk4, tk5, tk6, tk7);
  // writer.create(tan, "tanks", tan);
  return tan;
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

void handleButtonClick(uint8_t pin)
{
  beepBuzzer();

  if (pin == BUTTON1) {
    Serial.println("Клик на BUTTON1");
    printNameAndPrice(names.name1, prices.price1, tanks.tank1);
    currentSelect = 1;
  }
  else if (pin == BUTTON2) {
    Serial.println("Клик на BUTTON2");
    printNameAndPrice(names.name2, prices.price2, tanks.tank2);
    currentSelect = 2;
  }
  else if (pin == BUTTON3) {
    Serial.println("Клик на BUTTON3");
    printNameAndPrice(names.name3, prices.price3, tanks.tank3);
    currentSelect = 3;
  }
  else if (pin == BUTTON4) {
    Serial.println("Клик на BUTTON4");
    printNameAndPrice(names.name4, prices.price4, tanks.tank4);
    currentSelect = 4;
  }
  else if (pin == BUTTON5) {
    Serial.println("Клик на BUTTON5");
    printNameAndPrice(names.name5, prices.price5, tanks.tank5);
    currentSelect = 5;
  }
  else if (pin == BUTTON6) {
    Serial.println("Клик на BUTTON6");
    printNameAndPrice(names.name6, prices.price6, tanks.tank6);
    currentSelect = 6;
  }
  else if (pin == BUTTON7) {
    Serial.println("Клик на BUTTON7");
    printNameAndPrice(names.name7, prices.price7, tanks.tank7);
    currentSelect = 7;
  }
  else if (pin == START_BUTTON) {
    Serial.println("Клик на START_BUTTON");
    // Ваш код здесь
  }
  else if (pin == STOP_BUTTON) {
    Serial.println("Клик на STOP_BUTTON");
    // Ваш код здесь
  }
  else {
    Serial.print("Клик на неизвестной кнопке P");
    Serial.println(pin);
  }
  Serial.printf("%f %f %f %f %f %f %f\n", tanks.tank1, tanks.tank2, tanks.tank3, tanks.tank4, tanks.tank5, tanks.tank6, tanks.tank7);
}

void beepBuzzer()
{
  // digitalWrite(BUZZER_PIN, HIGH);
  buzzerStartTime = millis();
  buzzerOn = true;
}

void printAccount(String account1){
  beepBuzzer();
  lcd.setCursor(8, 0);
  lcd.print("          ");
  lcd.setCursor(8, 0);
  lcd.print(account1);
  account = account1.toInt();
}

void getAllNameAndPrice(String jsonString){
  FirebaseJson json;
  FirebaseJsonData jsonData;

  // Парсим JSON строку
  json.setJsonData(jsonString);

  json.get(jsonData, "names/name1"); if (jsonData.success) names.name1 = jsonData.stringValue;
  json.get(jsonData, "names/name2"); if (jsonData.success) names.name2 = jsonData.stringValue;
  json.get(jsonData, "names/name3"); if (jsonData.success) names.name3 = jsonData.stringValue;
  json.get(jsonData, "names/name4"); if (jsonData.success) names.name4 = jsonData.stringValue;
  json.get(jsonData, "names/name5"); if (jsonData.success) names.name5 = jsonData.stringValue;
  json.get(jsonData, "names/name6"); if (jsonData.success) names.name6 = jsonData.stringValue;
  json.get(jsonData, "names/name7"); if (jsonData.success) names.name7 = jsonData.stringValue;

  // Извлечение prices
  json.get(jsonData, "prices/price1"); if (jsonData.success) prices.price1 = jsonData.intValue;
  json.get(jsonData, "prices/price2"); if (jsonData.success) prices.price2 = jsonData.intValue;
  json.get(jsonData, "prices/price3"); if (jsonData.success) prices.price3 = jsonData.intValue;
  json.get(jsonData, "prices/price4"); if (jsonData.success) prices.price4 = jsonData.intValue;
  json.get(jsonData, "prices/price5"); if (jsonData.success) prices.price5 = jsonData.intValue;
  json.get(jsonData, "prices/price6"); if (jsonData.success) prices.price6 = jsonData.intValue;
  json.get(jsonData, "prices/price7"); if (jsonData.success) prices.price7 = jsonData.intValue;

  json.get(jsonData, "account"); if(jsonData.success) printAccount(String(jsonData.intValue));

  Serial.println("Names:");
  Serial.println("  Name1: " + names.name1);
  Serial.println("  Name2: " + names.name2);
  Serial.println("  Name3: " + names.name3);
  Serial.println("  Name4: " + names.name4);
  Serial.println("  Name5: " + names.name5);
  Serial.println("  Name6: " + names.name6);
  Serial.println("  Name7: " + names.name7);

  Serial.println("Prices:");
  Serial.println("  Price1: " + String(prices.price1));
  Serial.println("  Price2: " + String(prices.price2));
  Serial.println("  Price3: " + String(prices.price3));
  Serial.println("  Price4: " + String(prices.price4));
  Serial.println("  Price5: " + String(prices.price5));
  Serial.println("  Price6: " + String(prices.price6));
  Serial.println("  Price7: " + String(prices.price7));

}

void buttonLoop(unsigned long currentMillis){
    if (currentMillis - lastButtonCheckTime >= buttonCheckInterval) {
    lastButtonCheckTime = currentMillis;

    for (uint8_t i = 0; i < numButtons; i++) {
      uint8_t pinValue = pcf8575.digitalRead(buttonPins[i]);
      bool currentState = (pinValue == LOW);

      if (currentState != buttonLastStates[i]) {
        if (currentState == false) {
          handleButtonClick(buttonPins[i]);
        }
        buttonLastStates[i] = currentState;  // Обновляем состояние кнопки
      }
    }
  }
  if (buzzerOn && (millis() - buzzerStartTime >= buzzerDuration)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
  }
}

void LEDinit(){
  lcd.setCursor(0, 0);
  lcd.print("Init LEDs");
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(LED5, OUTPUT);
  pinMode(LED6, OUTPUT);
  pinMode(LED7, OUTPUT);
  delay(1000);
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
  digitalWrite(LED3, HIGH);
  digitalWrite(LED4, HIGH);
  digitalWrite(LED5, HIGH);
  digitalWrite(LED6, HIGH);
  digitalWrite(LED7, HIGH);
  delay(1000);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
  digitalWrite(LED5, LOW);
  digitalWrite(LED6, LOW);
  digitalWrite(LED7, LOW);
  delay(1000);
}

void POTinit(){
  lcd.clear();
  pinMode(POT1, INPUT);
  pinMode(POT2, INPUT);
  pinMode(POT3, INPUT);
  pinMode(POT4, INPUT);
  pinMode(POT5, INPUT);
  pinMode(POT6, INPUT);
  pinMode(POT7, INPUT);
  valuePot1 = analogRead(POT1);
  valuePot2 = analogRead(POT2);
  valuePot3 = analogRead(POT3);
  valuePot4 = analogRead(POT4);
  valuePot5 = analogRead(POT5);
  valuePot6 = analogRead(POT6);
  valuePot7 = analogRead(POT7);
  lcd.setCursor(0,0);
  lcd.print("1." + String(valuePot1));
  lcd.setCursor(0,1);
  lcd.print("2." + String(valuePot2));
  lcd.setCursor(0,2);
  lcd.print("3." + String(valuePot3));
  lcd.setCursor(0,3);
  lcd.print("4." + String(valuePot4));
  lcd.setCursor(10,0);
  lcd.print("5." + String(valuePot5));
  lcd.setCursor(10,1);
  lcd.print("6." + String(valuePot6));
  lcd.setCursor(10,2);
  lcd.print("7." + String(valuePot7));
  delay(5000);
  Serial.printf("%d %d %d %d %d %d %d\n", valuePot1, valuePot2, valuePot3, valuePot4, valuePot5, valuePot6, valuePot7);
  lcd.clear();
}

void printNameAndPrice(String name, int prices, float potvalue){
  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(0, 2);
  lcd.print("                    ");
  lcd.setCursor(0, 3);
  lcd.print("                    ");
  lcd.setCursor(0, 1);
  lcd.print(name);
  lcd.setCursor(0, 2);
  lcd.print("Price:"+String(prices)+" t/l");
  lcd.setCursor(0, 3);
  lcd.print("Tank:"+String(potvalue, 2)+" l");
  Serial.println(String(potvalue, 2));
}

void potLoop(unsigned long currentMillis){
  if(currentMillis - lastPotCheck >= 200){
    lastPotCheck = currentMillis;
    valuePot1 = analogRead(POT1);
    valuePot2 = analogRead(POT2);
    valuePot3 = analogRead(POT3);
    valuePot4 = analogRead(POT4);
    valuePot5 = analogRead(POT5);
    valuePot6 = analogRead(POT6);
    // valuePot7 = analogRead(POT7);
    tanks.tank1 = (valuePot1 * 10.0) / 4096.0;
    tanks.tank2 = (valuePot2 * 10.0) / 4096.0;
    tanks.tank3 = (valuePot3 * 10.0) / 4096.0;
    tanks.tank4 = (valuePot4 * 10.0) / 4096.0;
    tanks.tank5 = (valuePot5 * 10.0) / 4096.0;
    tanks.tank6 = (valuePot6 * 10.0) / 4096.0;
    tanks.tank7 = (valuePot7 * 10.0) / 4096.0;
    
  }
  // Serial.printf("%d %d %d %d %d %d %d\n", valuePot1, valuePot2, valuePot3, valuePot4, valuePot5, valuePot6, valuePot7);
}