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