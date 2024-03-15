#include <iocontrol.h>
#include <WiFi.h>
// Wi-Fi
const char* ssid = "XXXXXXXX";
const char* password = "XXXXXXXXXXXXXXXXX";

// Название панели на сайте iocontrol.ru
const char* myPanelName = "XXXXXX";
int status;
// Название переменных как на сайте iocontrol.ru
const char* VarName_sensorT = "Temperature";
const char* VarName_sensorH = "Humidity";
const char* VarName_sensorP = "Pressure";
const char* VarName_sensorTS = "Soil_temperature";
const char* VarName_sensorHS = "Soil_humidity";
const char* VarName_sensorL = "Light_intensity";
const char* VarName_button = "Pump";
const char* VarName_slider = "LED";
// Создаём объект клиента
WiFiClient client;
// Создаём объект iocontrol, передавая название панели и клиента
iocontrol mypanel(myPanelName, client);

#include <Wire.h>

#define pump 13  // пин насоса
#define led 4    //  пин светодиодной ленты

#include <BH1750.h>  // библиотека датчика освещенности
BH1750 lightMeter;

#include <Adafruit_Sensor.h>  // библиотека датчика температуры, влажности и давления
#include <Adafruit_BME280.h>
Adafruit_BME280 bme280;

#define SOIL_MOISTURE 32  // пины датчика температуры и влажности почвы
#define SOIL_TEMPERATURE 33
// откалиброванные значения
const float air_value = 1587.0;
const float water_value = 800.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;

void setup() {
  Wire.begin();

  pinMode(pump, OUTPUT);
  ledcAttachPin(led, 2);
  ledcSetup(2, 5000, 10);

  Serial.begin(115200);
  delay(512);

  lightMeter.begin();  // запуск датчика освещенности

  // смена канала I2C
  setBusChannel(0x07);
  //запуск датчика MGS-THP80
  bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }

  WiFi.begin(ssid, password);
  // Ждём подключения
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  // Вызываем функцию первого запроса к сервису
  mypanel.begin();
}

void loop()  // вызываем функцию обработки сообщений через определенный период
{
  // ************************ ЧТЕНИЕ ************************
  // Чтение значений переменных из сервиса
  status = mypanel.readUpdate();
  // Если статус равен константе OK...
  if (status == OK) {
    // Выводим текст в последовательный порт
    Serial.println("------- Read OK -------");
    // Записываем считанный из сервиса значения в переменные
    digitalWrite(pump, mypanel.readInt(VarName_button));
    int mySlider = mypanel.readInt(VarName_slider);
    ledcWrite(2, mySlider * 10.23);
  }
  // ************************ ЗАПИСЬ ************************
  // считывание датчиков
  int l = lightMeter.readLightLevel();
  setBusChannel(0x07);
  float t = bme280.readTemperature();
  int h = bme280.readHumidity();
  int p = bme280.readPressure() / 133.3F;
  int adc0 = analogRead(SOIL_MOISTURE);
  float adc1 = analogRead(SOIL_TEMPERATURE);
  float t1 = ((adc1 / 4095.0 * 6.27) - 0.5) * 100.0;  // АЦП разрядность (12) = 4095 и коэф. для напряжения ~4,45В
  int h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
  // Записываем  значение в переменную для отправки в сервис
  mypanel.write(VarName_sensorT, t);   
  mypanel.write(VarName_sensorH, h);    
  mypanel.write(VarName_sensorP, p);    
  mypanel.write(VarName_sensorTS, t1);  
  mypanel.write(VarName_sensorHS, h1);  
  mypanel.write(VarName_sensorL, l);    
  // Отправляем переменные из контроллера в сервис
  status = mypanel.writeUpdate();
  // Если статус равен константе OK...
  if (status == OK) {
    // Выводим текст в последовательный порт
    Serial.println("------- Write OK -------");
  }
  delay(1000);
}

bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= 0x08) {
    return false;
  } else {
    Wire.beginTransmission(0x70);
    // для микросхемы PCA9547
    Wire.write(i2c_channel | 0x08);
    // Для микросхемы PW548A
    // Wire.write(0x01 << i2c_channel);
    Wire.endTransmission();
    return true;
  }
}
