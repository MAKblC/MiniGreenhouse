#include <Wire.h>

#define pump 13  // пин насоса // pump pin
#define led 4    // пин вентилятора // cooler pin

#include <BH1750.h>  // добавляем библиотеку датчика освещенности // adding Light intensity sensor library
BH1750 lightMeter;   // BH1750

#include <Adafruit_Sensor.h>  // добавляем библиотеку датчика температуры, влажности и давления // adding Temp Hum Bar sensor library
#include <Adafruit_BME280.h>  // BME280
Adafruit_BME280 bme280;       //

#define SOIL_MOISTURE 32     // A6
#define SOIL_TEMPERATURE 33  // A7
const float air_value = 1587.0;
const float water_value = 800.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;

//////////////////////////////////////////НАСТРОЙКИ/CONFIGURATION/////////////////////////////////////////////////////////////////
void setup() {
  Wire.begin();
  ledcAttachPin(pump, 1);
  ledcAttachPin(led, 2);
  // канал, частота, разрядность
  ledcSetup(1, 5000, 10);
  ledcSetup(2, 5000, 10);

  Serial.begin(115200);
  delay(512);

  lightMeter.begin();  // запуск датчика освещенности // turn the light intensity sensor on

  setBusChannel(0x07);
  bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }
}

void loop() {
  setBusChannel(0x07);
  float t = bme280.readTemperature();
  float h = bme280.readHumidity();
  float p = bme280.readPressure() / 133.3F;
  Serial.println("------------------------------------");
  Serial.println("Температур воздуха: " + String(t, 1) + " °C");
  Serial.println("Влажность воздуха:  " + String(h, 1) + " %");
  Serial.println("Атмосферное давление " + String(p, 1) + " мм рт.ст.");
  Serial.println("------------------------------------");
  float adc0 = analogRead(SOIL_MOISTURE);
  float adc1 = analogRead(SOIL_TEMPERATURE);
  float t1 = ((adc1 / 4095.0 * 6.27) - 0.5) * 100.0;  // АЦП разрядность (12) = 4095 и коэф. для напряжения ~4,45В
  float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
  Serial.println("Влажность почвы: " + String(t1, 1) + " °C");
  Serial.println("Температура почвы: " + String(h1, 1) + " %");
  Serial.println("------------------------------------");
  float l = lightMeter.readLightLevel();
  Serial.println("ОСвещенность: " + String(l) + " Лк");
  Serial.println("------------------------------------");
  delay(2000);
  Serial.println("Управление освещением");
  for (int value = 0; value <= 100; value++) {
    ledcWrite(2, value * 10.23);
    Serial.println("Яркость света: " + String(value) + " %");
    delay(50);
  }
  for (int value = 100; value >= 0; value--) {
    ledcWrite(2, value * 10.23);
    Serial.println("Яркость света: " + String(value) + " %");
    delay(50);
  }
  Serial.println("------------------------------------");
  Serial.println("Управление интенсивностью полива");
  for (int value = 0; value <= 100; value++) {
    ledcWrite(1, value * 10.23);
    Serial.println("Интенсивность полива: " + String(value) + " %");
    delay(50);
  }
  for (int value = 100; value >= 0; value--) {
    ledcWrite(1, value * 10.23);
    Serial.println("Интенсивность полива: " + String(value) + " %");
    delay(50);
  }
}

bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= 0x08) {
    return false;
  } else {
    Wire.beginTransmission(0x70);
    Wire.write(i2c_channel | 0x08);  // для микросхемы PCA9547
                                     // Wire.write(0x01 << i2c_channel);  // Для микросхемы PW548A
    Wire.endTransmission();
    return true;
  }
}