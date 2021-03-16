//Abgeleitet von Ihrem mitgeschickten Sketch.

#define ENABLE_GxEPD2_GFX 0
#define BUTTON_PIN 39
#define uS_to_S_Factor 1000000
#define time_to_sleep 5
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

//display ist ein Objekt der Template Klasse GxEPD2_BW mit dem Type GxEPD2_213_B73
GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH0213B73

#include <Fonts/FreeMono9pt7b.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLECharacteristic.h>
#include "google.h"

#define SERV_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

//RTC_DATA_ATTR fuer Speicher, der nach DeepSleep noch vorhanden ist.
RTC_DATA_ATTR BLECharacteristic *pCharacteristic;         //Characteristic in welcher sich die Daten befinden.
RTC_DATA_ATTR int bootCount = 0;

RTC_DATA_ATTR bool roomBooked[12];          //if room is booked half-houred; marks the edges not the corners
RTC_DATA_ATTR bool isConnected = false;
RTC_DATA_ATTR bool newConnection = false;
RTC_DATA_ATTR bool firstStart = true;

void initDisplay() {
  display.init(115200);
  display.setRotation(1);   //Ausrichtung des Displays
  display.fillScreen(GxEPD_WHITE);    //Grundfarbe
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMono9pt7b);
  display.setFullWindow();

  display.firstPage();    //Colors Display white and sets 2 variables.
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(5, 90, 241, 12, GxEPD_BLACK);
    for (int i = 1; i < 12; i++) {
      display.drawLine(5 + 20 * i, 90, 5 + 20 * i, 101, GxEPD_BLACK);    //Zeitleistenbegrenzer
    }
    //Zeitleiste beschriften
    display.setCursor(0, 115);
    display.print("8");
    display.setCursor(35, 115);
    display.print("10");
    display.setCursor(75, 115);
    display.print("12");
    display.setCursor(115, 115);
    display.print("14");
    display.setCursor(155, 115);
    display.print("16");
    display.setCursor(195, 115);
    display.print("18");
    display.setCursor(230, 115);
    display.print("20");
    
    display.setCursor(10, 30);
    display.print("Raum 187");
    display.setCursor(10, 50);
    display.print("10 Personen");
    display.drawBitmap(190, 10, NaN, 50, 50, GxEPD_BLACK);
    //Felder fuer Reservierungen fuellen
    if (roomBooked) {
      display.setCursor(0, 75);
      for (int i = 0; i < sizeof(roomBooked)/sizeof(roomBooked[0]); i++) {
        if (roomBooked[i]) {
          display.fillRect((i * 20) + 5, 90, 20, 12, GxEPD_BLACK);
        }
      }
    }
  } while (display.nextPage());     //Checks if pages are left
}

class bleCharacteristicCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string msg = pCharacteristic->getValue();
    Serial.printf("msg: %s|\n", msg.c_str());    
    //Serial.flush();       //Um Buffer zu leeren.
    delay(5000);
    //App Input Auswertung
    Serial.printf("Laenge: %d\n", msg.length());
    if (msg.at(0) == 'f' && msg.at(1) == 'r' && msg.at(2) == 'e' && msg.at(3) == 'e'/*msg.compare("free") == 0*/) {   //String so pruefen, weil die App ein mir unbekanntes Whitespace anhaengt.
      Serial.println("FREE");
      freeRooms();
    } 
    //Z.B. "8 9" ; Laenge von AI2 um 1 groesser.
    else if (msg.length() == 4) {
      Serial.printf("%d | %d\n", int(msg.at(0)), int(msg.at(2)));
      bookRoom(int(msg.at(0)) - 48, int(msg.at(2)) - 48);     //-48 because of char to int
    }
    //z.B. "9 16"
    else if (msg.length() == 5) {
      char buf[3] = {msg.at(2), msg.at(3)};
      Serial.printf("%d | %d\n", int(msg.at(0)) - 48, atoi(buf));
      bookRoom(int(msg.at(0)) - 48, atoi(buf));
    } 
    //z.B. "19 20"
    else if (msg.length() == 6) {
      char buf[3] = {msg.at(0), msg.at(1)};
      char buf1[3] = {msg.at(3), msg.at(4)};
      Serial.printf("%d | %d\n", atoi(buf), atoi(buf1));
      bookRoom(atoi(buf), atoi(buf1));
    }
  }
};

class MyServerCallbacks: public BLEServerCallbacks {
  // aufruf bei connect - flag setzen
  void onConnect(BLEServer *pServer) {
    Serial.printf("! mit server verbunden\n");
    isConnected = true;
    newConnection = true;
  };
  // aufruf bei disconnect - flag ruecksetzen
  void onDisconnect(BLEServer* pServer) {
    Serial.printf("! verbindung zu server getrennt\n");
    isConnected = false;
    
    startSleep();
  }
};

void startSleep() {
    ++bootCount;
    Serial.println("Boot number: " + String(bootCount));
    esp_sleep_enable_timer_wakeup(time_to_sleep * uS_to_S_Factor);
    Serial.println("Going to sleep now");
    delay(1000);
    Serial.flush();
    esp_deep_sleep_start();
}

//Von 8 Uhr morgens bis 20 Uhr abends
void bookRoom(uint8_t startHour, uint8_t endHour) {
  // Matches restrictions
  Serial.printf("Zu buchender Zeitraum: %d | %d\n", startHour, endHour);
  if ((startHour >= 8 && startHour <= 20) && (endHour >= 8 && endHour <= 20)) {
        uint8_t  period[endHour - startHour];
        uint8_t periodSize = sizeof(period) / sizeof(period[0]);
        //writes booking periods in form of roomBooked(0 - 12); damit es leichter zu vergleichen ist
        for (int i = 0; i < periodSize; i++) {
          period[i] = (startHour + i) - 8;
        }
        //Is room still free?
        for (int i = 0; i < periodSize; i++) {
          //If true then not free
          if (roomBooked[period[i]]) {
            if (isConnected) {
              pCharacteristic->setValue("1");
              pCharacteristic->notify();
            }
            Serial.println("Belegt!");
            return;
          }
        }
        //App Feedback fuer erfolgreiche Buchung
        pCharacteristic->setValue("0");
        pCharacteristic->notify();
        Serial.println("set+notify");
        delay(1000);
        //Book the room
        for (int i = 0; i < periodSize; i++) {
          roomBooked[period[i]] = true;
        }
        initDisplay();
  }
  else {
    Serial.printf("Reservierung nur von 8-20 Uhr moeglich");
  }
}

void freeRooms() {
  if (roomBooked) {
    for (int i = 0; i < sizeof(roomBooked) / sizeof(roomBooked[0]); i++) {
      roomBooked[i] = false;
    }
    initDisplay();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("# starte ble server");
  // ble initialisieren
  BLEDevice::init("ESP-SRV");
  // ble server erzeugen
  BLEServer *pServer = BLEDevice::createServer();
  // server callbacks installieren
  pServer->setCallbacks(new MyServerCallbacks());
  // service erzeugen
  BLEService *pService = pServer->createService(SERV_UUID);
  // characteristic erzeugen und einstellen welche Properties es haben soll.
  pCharacteristic = pService->createCharacteristic(
        CHAR_UUID, 
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE);
  // descriptor erzeugen - fuer notify/indicate notwendig. Fuer die "Pipe" zustaendig schaetze ich.
  pCharacteristic->addDescriptor(new BLE2902());
  // service starten
  pService->start();
  pCharacteristic->setCallbacks(new bleCharacteristicCallback());

  // advertising starten
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERV_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); 
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("# ble server gestartet. warte auf anrufe ...");
  delay(5000);    //Ansonsten wird die naechste Methode uebersprungen.
  if (firstStart || newConnection) {
    initDisplay();
    firstStart = false;
    newConnection = false;
  }
  Serial.println("Enter Command: ");

  if (!isConnected){
    startSleep();
  }
}

void loop() {
  
}
