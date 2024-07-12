#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include "settings.h"  // файл с вашими данными

const unsigned long BOT_MTBS = 1000;       // время обновления Телеграм (1 секунда)
const unsigned long SENSOR_MTBS = 600000;  // время отправки данных с датчиков  в гугл таблицу (10 минут)
const unsigned long sensor_alert = 60000;  // периодичность оповещения критических значений (1 минута)
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long bot_lasttime, water_timer, sensor_monitor, sensor_lasttime;

#include "time.h"
const char* ntpServer = "pool.ntp.org";  // отсюда считается текущие дата/время
const long gmtOffset_sec = 10800;        // +3 часа для времени по МСК
const int daylightOffset_sec = 0;
int hour;
String time_buffer;

#include <Wire.h>

#define pump 13  // пин насоса
#define led 4    //  пин светодиодной ленты

#include <BH1750.h>  // библиотека датчика освещенности
BH1750 lightMeter;

#include <Adafruit_Sensor.h>  // библиотека датчика температуры, влажности и давления
#include <Adafruit_BME280.h>
Adafruit_BME280 bme280;

#define SOIL_MOISTURE 34  // пины датчика температуры и влажности почвы
#define SOIL_TEMPERATURE 35
// откалиброванные значения
const float air_value = 1587.0;
const float water_value = 800.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;


// для инфографики в Телеграме
String temp_max = "29";
String hum_min = "20";
String water_last = "0:00";
String lightedge = "600";
String lightbegin = "8";
String lightend = "20";
String wateredge = "10";
String watertiming = "0";
String pumpPower = "100";

// настройки режима выращивания
int water_uptime;
int h_min = 20;       // минимальная влажность
int t_max = 29;       // максимальная температура
int lightE = 600;     // порог включения света (в Люкс)
int lightBegin = 8;   // начало светового дня в 8 утра
int lightEnd = 20;    // конец светового дня в 8 вечера
int waterE = 600000;  // периодичность полива в мс (10 мин)
int waterT = 0;       // длительность полива в мс (0 сек)
int pumpP = 100;      // мощность полива в %

void setup() {
  Wire.begin();

  ledcAttachPin(pump, 1);
  ledcSetup(1, 5000, 10);

  pinMode(led, OUTPUT);

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

  Serial.println();
  Serial.print("Подключаюсь к Wifi: ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi подключен. IP адрес: ");
  Serial.println(WiFi.localIP());

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // получение времени

  bot_setup();
}

void loop() {
  // проверка, не пора ли отправить данные в таблицу?
  if (millis() - sensor_lasttime > SENSOR_MTBS) {
    sheet_write();
    sensor_lasttime = millis();
  }

  // проверка, нужно ли уже поливать?
  if (millis() - water_timer > waterE) {
    ledcWrite(1, pumpP * 10.23);
    Serial.println("Включен полив");
    delay(waterT * 1000);
    ledcWrite(1, 0);
    Serial.println("Выключен полив");
    getTime();
    water_last = time_buffer;
    water_timer = millis();
  }

  // проверка, есть ли новое сообщение в Телеграме?
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }

  // проверка, идет ли световой день?
  getTime();
  float l = lightMeter.readLightLevel();
  if (l < lightE and hour >= lightBegin and hour < lightEnd) {
    digitalWrite(led, HIGH);
  } else {
    digitalWrite(led, LOW);
  }

  // проверка, не превышены ли значения климата?
  setBusChannel(0x07);
  float t = bme280.readTemperature();
  float h = bme280.readHumidity();
  if (t > t_max or h < h_min) {                      // если значение превышено
    if (millis() - sensor_monitor > sensor_alert) {  // если уже прошла 1 минута с последней отправки
      String welcome = "*Условия выращивания не соблюдаются!*\n";
      welcome += "Показания датчиков:\n-------------------------------------------\n";
      welcome += "🌡 Температура воздуха: " + String(t, 1) + " °C\n";
      welcome += "💧 Влажность воздуха: " + String(h, 0) + " %\n";
      bot.sendMessage(your_chat_ID, welcome, "Markdown");  // отправить уведомление
      sensor_monitor = millis();                           // сброс таймера
    }
  }
}


// обработка новых сообщений
void handleNewMessages(int numNewMessages) {
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    text.toLowerCase();
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";
    if (text == "/waterperiod") {
      String sms = "Введите периодичность полива в минутах\n";
      sms += "Текущая периодичность: ";
      sms += wateredge + " минут";
      bot.sendMessage(chat_id, sms, "");
      wateredge = text;
      while (wateredge == text) {
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        wateredge = bot.messages[i].text;
      }
      waterE = wateredge.toInt();
      bot.sendMessage(chat_id, "Периодичность полива теперь " + String(waterE) + (" минут"), "");
      waterE = waterE * 60000;
    }
    if (text == "/watertime") {
      String sms = "Введите длительность полива в секундах\n";
      sms += "Текущая периодичность: ";
      sms += watertiming + " секунд";
      bot.sendMessage(chat_id, sms, "");
      watertiming = text;
      while (watertiming == text) {
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        watertiming = bot.messages[i].text;
      }
      waterT = watertiming.toInt();
      bot.sendMessage(chat_id, "Длительность полива теперь " + String(waterT) + (" секунд"), "");
    }
    if (text == "/pumppower") {
      String sms = "Введите мощность полива в процентах\n";
      sms += "Текущая мощность: ";
      sms += pumpPower + " %";
      bot.sendMessage(chat_id, sms, "");
      pumpPower = text;
      while (pumpPower == text) {
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        pumpPower = bot.messages[i].text;
      }
      pumpP = pumpPower.toInt();
      bot.sendMessage(chat_id, "Мощность полива теперь " + String(pumpP) + (" %"), "");
    }
    if (text == "/lightedge") {
      String sms = "Введите порог включения искусственного освещения\n";
      sms += "Текущий порог: ";
      sms += lightedge + " Люкс";
      bot.sendMessage(chat_id, sms, "");
      lightedge = text;
      while (lightedge == text) {
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        lightedge = bot.messages[i].text;
      }
      lightE = lightedge.toInt();
      bot.sendMessage(chat_id, "Порог освещенности теперь " + String(lightE) + " Люкс", "");
    }
    if (text == "/lightbegin") {
      String sms = "Введите час начала светого дня\n";
      sms += "Текущий старт в ";
      sms += lightbegin + " часов";
      bot.sendMessage(chat_id, sms, "");
      lightbegin = text;
      while (lightbegin == text) {
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        lightbegin = bot.messages[i].text;
      }
      lightBegin = lightbegin.toInt();
      bot.sendMessage(chat_id, "Старт светового дня теперь в " + String(lightBegin) + " часов", "");
    }
    if (text == "/lightend") {
      String sms = "Введите час конца светого дня\n";
      sms += "Текущий конец в ";
      sms += lightend + " часов";
      bot.sendMessage(chat_id, sms, "");
      lightend = text;
      while (lightend == text) {
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        lightend = bot.messages[i].text;
      }
      lightEnd = lightend.toInt();
      bot.sendMessage(chat_id, "Конец светового дня теперь в " + String(lightEnd) + " часов", "");
    }
    if (text == "/t_max") {
      String sms = "Введите границу максимальной температуры\n";
      sms += "Текущая граница: ";
      sms += temp_max + " градусов";
      bot.sendMessage(chat_id, sms, "");
      temp_max = text;
      while (temp_max == text) {
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        temp_max = bot.messages[i].text;
      }
      t_max = temp_max.toInt();
      bot.sendMessage(chat_id, "Граница максимальной температуры теперь " + String(t_max) + " градусов", "");
    }
    if (text == "/h_min") {
      String sms = "Введите границу минимальной влажности\n";
      sms += "Текущая граница: ";
      sms += hum_min + "%";
      bot.sendMessage(chat_id, sms, "");
      hum_min = text;
      while (hum_min == text) {
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        hum_min = bot.messages[i].text;
      }
      h_min = hum_min.toInt();
      bot.sendMessage(chat_id, "Граница минимальной влажности теперь " + String(h_min) + "%", "");
    }
    if (text == "/info") {
      String welcome = "Настройки:\n-------------------------------------------\n";
      welcome += "💦 Последний полив: " + water_last + "\n";
      welcome += "⏳ Длительность полива: " + String(waterT) + " секунд\n";
      welcome += "📆 Периодичность полива: " + String(waterE / 60000) + " минут\n";
      welcome += "🚰 Мощность полива: " + String(pumpP) + " %\n";
      water_uptime = (waterE - (millis() - water_timer)) / 60000;
      welcome += "⏰ Следующий полив через: " + String(water_uptime) + " минут/ ~" + String(water_uptime / 60) + " часов\n";
      welcome += "💡 Порог включения света: " + String(lightE) + " Люкс\n";
      welcome += "☀️ Начало светового дня в: " + String(lightBegin) + " часов\n";
      welcome += "🌙 Конец светового дня в: " + String(lightEnd) + " часов\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
    if (text == "/sensors") {
      float l = lightMeter.readLightLevel();
      setBusChannel(0x07);
      float t = bme280.readTemperature();
      float h = bme280.readHumidity();
      float p = bme280.readPressure() / 133.3F;
      float adc0 = analogRead(SOIL_MOISTURE);
      float adc1 = analogRead(SOIL_TEMPERATURE);
      float t1 = ((adc1 / 4095.0 * 6.27) - 0.5) * 100.0;  // АЦП разрядность (12) = 4095 и коэф. для напряжения ~4,45В
      float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);
      String welcome = "Показания датчиков:\n-------------------------------------------\n";
      welcome += "🌡 Температура воздуха: " + String(t, 1) + " °C\n";
      welcome += "💧 Влажность воздуха: " + String(h, 0) + " %\n";
      welcome += "☁ Атмосферное давление: " + String(p, 0) + " мм рт.ст.\n";
      welcome += "☀ Освещенность: " + String(l) + " Лк\n\n";
      welcome += "🌱 Температура почвы: " + String(t1, 1) + " °C\n";
      welcome += "🌱 Влажность почвы: " + String(h1, 0) + " %\n\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
    if (text == "/start") {
      bot.sendMessage(chat_id, "Привет, " + from_name + "!", "");
      bot.sendMessage(chat_id, "Я контроллер Йотик 32. Я принимаю следующие команды:", "");
      String sms = "/sensors - считать значения с датчиков\n";
      sms += "/lightedge - изменить порог освещения\n";
      sms += "/waterperiod - изменить периодичность полива\n";
      sms += "/watertime - изменить длительность полива\n";
      sms += "/pumppower - изменить мощность насоса\n";
      sms += "/lightbegin - изменить начало светового дня\n";
      sms += "/lightend - изменить конец светового дня\n";
      sms += "/t_max - изменить границу максимальной температуры\n";
      sms += "/h_min - изменить границу минимальной влажности\n";
      bot.sendMessage(chat_id, sms, "");
    }
  }
}

// смена I2C канала
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

// считать время
void getTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%d.%b в %H:%M", &timeinfo);  // получение даты и времени
  String asString(timeStringBuff);
  time_buffer = asString;
  day = timeinfo.tm_mday;
  month = timeinfo.tm_mon + 1;
  hour = timeinfo.tm_hour;
  minute = timeinfo.tm_min;
}

// настройка команд меню в телеграме
void bot_setup() {
  const String commands = F("["
                            "{\"command\":\"watertime\",  \"description\":\"Длительность полива\"},"
                            "{\"command\":\"waterperiod\", \"description\":\"Периодичность полива\"},"
                            "{\"command\":\"lightedge\",  \"description\":\"Изменить порог освещения\"},"
                            "{\"command\":\"pumppower\",  \"description\":\"Изменить мощность насоса\"},"
                            "{\"command\":\"lightbegin\",  \"description\":\"Изменить начало светового дня\"},"
                            "{\"command\":\"lightend\",  \"description\":\"Изменить конец светового дня\"},"
                            "{\"command\":\"t_max\",  \"description\":\"Изменить границу максимальной температуры\"},"
                            "{\"command\":\"h_min\",  \"description\":\"Изменить границу минимлаьной влажности\"},"
                            "{\"command\":\"info\",\"description\":\"Текущие настройки\"}"
                            "]");
  bot.setMyCommands(commands);
}

// функция отправки данных датчиков в гугл таблицу
void sheet_write(void) {
  setBusChannel(0x07);
  float l = lightMeter.readLightLevel();
  setBusChannel(0x07);
  float t = bme280.readTemperature();
  float h = bme280.readHumidity();
  float p = bme280.readPressure() / 133.3F;
  float adc0 = analogRead(SOIL_MOISTURE);
  float adc1 = analogRead(SOIL_TEMPERATURE);
  float t1 = ((adc1 / 4095.0 * 6.27) - 0.5) * 100.0;  // АЦП разрядность (12) = 4095 и коэф. для напряжения ~4,45В
  float h1 = map(adc0, air_value, water_value, moisture_0, moisture_100);

  if (WiFi.status() == WL_CONNECTED) {
    static bool flag = false;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%d %m %Y %H:%M:%S", &timeinfo);  // получение даты и времени
    String asString(timeStringBuff);
    asString.replace(" ", "-");
    HTTPClient http;
    http.begin("https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?");
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // собираем сообщение для публикации
    String message = "date=" + asString;
    message += "&light=" + String(l, 0);
    message += "&temp=" + String(t, 1);
    message += "&hum=" + String(h, 0);
    message += "&press=" + String(p, 0);
    message += "&tempH=" + String(t1, 1);
    message += "&humH=" + String(h1, 0);
    // данный запрос вызывает функцию doPost() в скрипте
    int httpResponseCode = http.POST(message);
    //---------------------------------------------------------------------
    String payload;
    if (httpResponseCode > 0) {
      payload = http.getString();
      //  Serial.println("Payload: " + payload);
    }
    //---------------------------------------------------------------------
    http.end();
  }
}
