#include <GyverPortal.h>
GyverPortal ui;
// Задаем статический IP-адрес:
IPAddress local_IP(192, 168, 4, 32);
// Задаем IP-адрес сетевого шлюза:
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);

#include <EEPROM.h>
#define EEPROM_SIZE 100

#include <Wire.h>
bool TCA9548A;

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

//Переменные для GP
int ledStat = 0;
bool pumpStat = 0;
int t = 0;
int h = 0;
int l = 0;
int p = 0;
int h1 = 0;
int t1 = 0;

char* AP_SSID;
int number = 0;

void build() {
  GP.BUILD_BEGIN(900);  // ширина колонки

  GP.THEME(GP_DARK);
  //GP.THEME(GP_LIGHT);

  GP.UPDATE("hS,tS,l,t,h,p");
  GP_MAKE_BLOCK_TAB(
    "Умная мини-теплица",
    GP_MAKE_BOX(GP_CENTER, GP_MAKE_BLOCK_THIN_TAB(
                             "Яркость освещения",
                             GP_MAKE_BOX(GP_CENTER, GP.SLIDER("bright", ledStat, 0, 100);););

                GP_MAKE_BLOCK_THIN_TAB(
                  "Насос",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL("Выкл"); GP.SWITCH("pump", pumpStat); GP.LABEL("Вкл"););););

    GP_MAKE_BOX(GP_CENTER, GP_MAKE_BLOCK_THIN_TAB(
                             "Освещенность",
                             GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "l"); GP.LABEL("lx");););
                GP_MAKE_BLOCK_THIN_TAB(
                  "Атмосферное давление",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "p"); GP.LABEL("мм рт.ст."););););

    GP_MAKE_BOX(GP_CENTER, GP_MAKE_BLOCK_THIN_TAB(
                             "Температура воздуха",
                             GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "t"); GP.LABEL("°C");););
                GP_MAKE_BLOCK_THIN_TAB(
                  "Влажность воздуха",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "h"); GP.LABEL("%"););););
    GP_MAKE_BOX(GP_CENTER, GP_MAKE_BLOCK_THIN_TAB(
                             "Температура почвы",
                             GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "tS"); GP.LABEL("°C");););
                GP_MAKE_BLOCK_THIN_TAB(
                  "Влажность почвы",
                  GP_MAKE_BOX(GP_CENTER, GP.LABEL_BLOCK("0", "hS"); GP.LABEL("%");););););
  GP.BUILD_END();
}

void action() {
  if (ui.click()) {
    if (ui.clickInt("bright", ledStat)) {
      ledcWrite(2, ledStat * 10.23);
    }

    if (ui.clickBool("pump", pumpStat)) {
      ledcWrite(1, pumpStat * 1023);
    }
  }

  if (ui.update()) {

    if (ui.update("l")) {
      ui.answer(l);
    }
    if (ui.update("t")) {
      ui.answer(t);
    }
    if (ui.update("h")) {
      ui.answer(h);
    }
    if (ui.update("p")) {
      ui.answer(p);
    }
    if (ui.update("tS")) {
      ui.answer(t1);
    }
    if (ui.update("hS")) {
      ui.answer(h1);
    }
  }
}

void setup() {
  Wire.begin();
  setI2Cmodule();

  ledcAttachPin(pump, 1);
  ledcAttachPin(led, 2);
  // канал, частота, разрядность
  ledcSetup(1, 5000, 10);
  ledcSetup(2, 5000, 10);

  Serial.begin(115200);
  delay(512);

  EEPROM.begin(EEPROM_SIZE);
  checkNamber();

  // Инициируем WiFi
  WiFi.mode(WIFI_AP);
  Serial.print("Настоить статический IP: ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Удалось" : "Не удалось!");
  WiFi.softAP(AP_SSID);
  Serial.print("IP для подключения: ");
  Serial.println(WiFi.softAPIP());

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

  // подключаем конструктор и запускаем
  ui.attachBuild(build);
  ui.attach(action);
  ui.start();
  Serial.println("Веб страница запущена");
}

void checkNamber() {
  EEPROM.get(95, number);
  Serial.println(number);
  if (number == -1) {
    number = random(10000);
    EEPROM.put(95, number);
    EEPROM.commit();
  }

  String n = "MiniGreenHouse_№-";
  n = n + String(number);

  AP_SSID = strcpy(new char[n.length() + 1], n.c_str());
}

static uint32_t tmr;
void loop() {
  ui.tick();

  if (millis() - tmr >= 500) {
    Serial.println("Wifi: " + String(AP_SSID) + " / Адрес: " + WiFi.softAPIP().toString());
    tmr = millis();
    l = lightMeter.readLightLevel();
    setBusChannel(0x07);
    t = bme280.readTemperature();
    h = bme280.readHumidity();
    p = bme280.readPressure() / 133.3F;
    float adc0 = analogRead(SOIL_MOISTURE);
    float adc1 = analogRead(SOIL_TEMPERATURE);
    t1 = ((adc1 / 4095.0 * 6.27) - 0.5) * 100.0;  // АЦП разрядность (12) = 4095 и коэф. для напряжения ~4,45В
    h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
  }
}

int setI2Cmodule(void) {
  int error;
  /*Контроллер подключается к адресу платы расширения*/
  Wire.beginTransmission(0x70);
  delay(100);
  error = Wire.endTransmission();  //если плата расширения не найдена или не определена - контроллер выходит из функции определения с выведением ошибки.
  if (error != 0) {
    Serial.print("\nNo I2C Module / Broken Module\n");
    Serial.print("Error #");
    Serial.print(error);
    Serial.print("\n");
    return error;
  }
  /*Контроллер переключает вывод платы расширения на GP14/GP15 по старому образцу*/
  Wire.beginTransmission(0x70);
  delay(100);
  Wire.write(0x08 | 0x07);
  Wire.endTransmission();
  Wire.beginTransmission(0x76);  //опрос подключенного к выводу датчика MGS-THP80
  error = Wire.endTransmission();
  if (error == 0) {
    Serial.print("\nOld Type Module");
    TCA9548A = false;
    return error;
  }
  Wire.beginTransmission(0x77);  //опрос подключенного к выводу датчика MGS-THP80
  error = Wire.endTransmission();
  if (error == 0) {
    Serial.print("\nOld Type Module");
    TCA9548A = false;
    return error;
  }
  /*Контроллер переключает вывод платы расширения на GP14/GP15 по новому образцу*/
  Wire.beginTransmission(0x70);
  delay(100);
  Wire.write(0x01 << 0x07);
  Wire.endTransmission();
  Wire.beginTransmission(0x76);  ///опрос подключенного к выводу датчика MGS-THP80
  error = Wire.endTransmission();
  if (error == 0) {
    Serial.print("\nNew Type Module");
    TCA9548A = true;
    return error;
  }
  Wire.beginTransmission(0x77);  ///опрос подключенного к выводу датчика MGS-THP80
  error = Wire.endTransmission();
  if (error == 0) {
    Serial.print("\nNew Type Module");
    TCA9548A = true;
    return error;
  }
  /*если плата расширения обнаружена, но не определился образец модели, выводится ошибка*/
  Serial.print("\nError #");
  Serial.print(error);
  return error;

  /*ERRORS:
    0: success.

    1: data too long to fit in transmit buffer.
    2: received NACK on transmit of address.
    3: received NACK on transmit of data.
    4: other error.
    5: timeout
  */
}

bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= 0x08) {
    return false;
  } else {
    Wire.beginTransmission(0x70);
    /*определена плата расширения модели TI TCA9548A (нового образца)*/
    if (TCA9548A == true) {
      Serial.println("New TI TCA9548A module");
      Wire.write(0x01 << i2c_channel);
    }
    /*в противном случае считается, что модель платы расширения - NXP PSA9547PW (старого образца)*/
    else {
      Serial.println("Old NXP PCA9547PW module");
      Wire.write(i2c_channel | 0x08);
    }

    Wire.endTransmission();
    return true;
  }
}
