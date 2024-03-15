#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
WiFiClientSecure secured_client;
unsigned long bot_lasttime;
const unsigned long BOT_MTBS = 1000; // период обновления сканирования новых сообщений

// параметры сети
#define WIFI_SSID "MGBot"
#define WIFI_PASSWORD "Terminator812"
// токен вашего бота
#define BOT_TOKEN "917994990:AAEzww06oDul4JbgaWpO7ooDvr-RPiM4PUc"
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// ссылка для поста фотографии
String test_photo_url = "https://mgbot.ru/upload/logo-r.png";
// отобразить кнопки перехода на сайт с помощью InlineKeyboard
String keyboardJson1 = "[[{ \"text\" : \"Ваш сайт\", \"url\" : \"https://mgbot.ru\" }],[{ \"text\" : \"Перейти на сайт IoTik.ru\", \"url\" : \"https://www.iotik.ru\" }]]";

#include <Wire.h>

#define pump 13  // пин насоса 
#define led 4    //  пин светодиодной ленты

#include <BH1750.h>  // библиотека датчика освещенности 
BH1750 lightMeter;   

#include <Adafruit_Sensor.h>  // библиотека датчика температуры, влажности и давления 
#include <Adafruit_BME280.h>  
Adafruit_BME280 bme280;      

#define SOIL_MOISTURE 32     // пины датчика температуры и влажности почвы
#define SOIL_TEMPERATURE 33  
// откалиброванные значения
const float air_value = 1587.0;
const float water_value = 800.0;
const float moisture_0 = 0.0;
const float moisture_100 = 100.0;

void setup() {
  Wire.begin();

  pinMode(pump, OUTPUT);
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
}

// обработчик сообщений от клиента
void handleNewMessages(int numNewMessages) {
  Serial.print("Обработка сообщения ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    text.toLowerCase();
    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";
    // выполняем действия в зависимости от пришедшей команды
    if (text == "/sensors")  // измеряем данные
    {
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

    if (text == "/photo") {  // пост фотографии
      bot.sendPhoto(chat_id, test_photo_url, "а вот и фотка!");
    }

    if (text == "/pumpon") {
      digitalWrite(pump, HIGH);
      delay(1000);
      bot.sendMessage(chat_id, "Насос включен на 1 сек", "");
      digitalWrite(pump, LOW);
    }
    if (text == "/ledon") {
      digitalWrite(led, HIGH);
      bot.sendMessage(chat_id, "Освещение включено", "");
    }
    if (text == "/ledoff") {
      digitalWrite(led, LOW);
      bot.sendMessage(chat_id, "Освещение выключено", "");
    }

    if (text == "/site")  // отобразить кнопки в диалоге для перехода на сайт
    {
      bot.sendMessageWithInlineKeyboard(chat_id, "Выберите действие", "", keyboardJson1);
    }
    if (text == "/options")  // клавиатура для управления теплицей
    {
      String keyboardJson = "[[\"/sensors\"],[\"/pumpon\", \"/ledon\", \"/ledoff\"]]";
      bot.sendMessageWithReplyKeyboard(chat_id, "Выберите команду", "", keyboardJson, true);
    }

    if (text == "/start" || text == "/help")  // команда для вызова помощи
    {
      String sms = "Привет, " + from_name + "!\n\n";
      sms += "Я контроллер Йотик 32. Команды смотрите в меню слева от строки ввода\n";
      sms = "Команды:\n";
      sms += "/options - пульт управления теплицей\n";
      sms += "/site - перейти на сайт\n";
      sms += "/photo - запостить фото\n";
      sms += "/help - вызвать помощь\n";
      bot.sendMessage(chat_id, sms, "");
    }
  }
}

void loop()  // вызываем функцию обработки сообщений через определенный период
{
  if (millis() - bot_lasttime > BOT_MTBS) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
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