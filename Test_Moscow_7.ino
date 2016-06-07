/*
  Пример работы со всем набором датчиков
  Выводы 4, 10, 11, 12, 13 нельзя использовать из-за Ethernet платы
  Выводы 0 и 1 нежелательно использовать, так как это порт отладки
  Created by Rostislav Varzar
*/

#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

// Датчики DS18B20
#define DS18B20_1 8
#define DS18B20_2 9
OneWire oneWire1(DS18B20_1);
OneWire oneWire2(DS18B20_2);
DallasTemperature ds_sensor1(&oneWire1);
DallasTemperature ds_sensor2(&oneWire2);

// Датчики DHT11
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Датчики влажности почвы и освещенности
#define MOISTURE_1 A0
#define MOISTURE_2 A1
#define LIGHT A2

// Выходы реле
#define RELAY1 4
#define RELAY2 7

// Выход ШИМ
#define PWM_LED 3

// Определение объекта для дисплея
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Сервомоторы
#define SERVO1_PWM 5
#define SERVO2_PWM 6
Servo servo_1;
Servo servo_2;

// Клавиатура
#define KEYBOARD A3

// Настройки сетевого адаптера
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// Local IP if DHCP fails
IPAddress ip(192, 168, 1, 250);
IPAddress dnsServerIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
EthernetClient client;

// Частота опроса клавиатуры
#define KEYBOARD_UPDATE_TIME 50

// Частота опроса датчика DHT11
#define DHT11_UPDATE_TIME 10000

// Частота опроса датчика DS18B20 №1
#define DS1_UPDATE_TIME 10000

// Частота опроса датчика DS18B20 №2
#define DS2_UPDATE_TIME 10000

// Частота опроса аналоговых датчиков
#define ANALOG_UPDATE_TIME 1000

// Частота вывода данных на LCD дисплей
#define LCD_UPDATE_TIME 1000

// Частота вывода данных на сервер IoT
#define IOT_UPDATE_TIME 5000

// Таймер обновления опроса клавиатуры
long timer_keyboard = 0;

// Таймер обновления опроса датчика DHT11
long timer_dht11 = 0;

// Таймер обновления опроса датчика DS18B20 №1
long timer_ds1 = 0;

// Таймер обновления опроса датчика DS18B20 №1
long timer_ds2 = 0;

// Таймер обновления опроса аналоговых датчиков
long timer_analog = 0;

// Таймер обновления вывода на LCD дсиплей
long timer_lcd = 0;

// Таймер обновления вывода на сервер IoT
long timer_iot = 0;

// Состояние помпы
int pump_state = 0;

// Состояние нагревателя
int heater_state = 0;

// Состояние крышки
int window_state = 0;

// Состояние освещения
int light_state = 0;

// Номер страницы, отображения данных на экране
int page_number = 0;

// Измерения с датчиков
float light1 = 0;
float soil_moisture1 = 0;
float soil_moisture2 = 0;
float soil_temperature1 = 0;
float soil_temperature2 = 0;
float air_temperature1 = 0;
float air_humidity1 = 0;

// Параметры IoT сервера
char iot_server[] = "cttit5402.cloud.thingworx.com";           // Name address for Google (using DNS)
IPAddress iot_address(52, 87, 101, 142);
char appKey[] = "dacefda9-bca7-47bb-b2dd-c57c7651749d";   // APP access key for ThingWorx
char thingName[] = "SmartGreen";                          // Name of your Thing in ThingWorx
char serviceName[] = "setAll";                            // Name of your Service (see above)

// Параметры сенсоров для IoT сервера
#define sensorCount 7                                     // How many values you will be pushing to ThingWorx
char* sensorNames[] = {"temp", "humidity", "light", "obj1_temp", "obj1_humidity", "obj2_temp", "obj2_humidity"};
float sensorValues[sensorCount];

// Максимальное время ожидания ответа от сервера
#define IOT_TIMEOUT1 5000
#define IOT_TIMEOUT2 100

// Таймер ожидания прихода символов с сервера
long timer_iot_timeout = 0;

// Размер приемного буффера
#define BUFF_LENGTH 256

// Приемный буфер
char buff[BUFF_LENGTH] = "";

void setup()
{
  // Инициализация последовательного порта
  Serial.begin(115200);

  // Инициализация датчиков температуры DS18B20
  ds_sensor1.begin();
  ds_sensor2.begin();

  // Инициализация датчика DHT11
  dht.begin();

  // Инициализация выхода реле
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  // Инициализация выхода на ШИМ
  pinMode(PWM_LED, OUTPUT);

  // Инициализация дисплея
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Инициализация портов для управления сервомоторами
  servo_1.attach(SERVO1_PWM);
  servo_2.attach(SERVO2_PWM);

  // Инициализация сетевой платы
  if (Ethernet.begin(mac) == 0)
  {
    Serial.println("Failed to configure Ethernet using DHCP");
    Ethernet.begin(mac, ip, dnsServerIP, gateway, subnet);
  }
  Serial.print("LocalIP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("SubnetMask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("GatewayIP: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("dnsServerIP: ");
  Serial.println(Ethernet.dnsServerIP());
  Serial.println("");

  // Однократный опрос датчиков
  readSensorDHT11();
  readSensorDS1();
  readSensorDS2();
  readSensorAnalog();

  // Установка сервомоторов в крайнее положение
  servo_1.write(0);
  servo_2.write(0);
}

void loop()
{
  // Опрос клавиатуры
  if (millis() > timer_keyboard + KEYBOARD_UPDATE_TIME)
  {
    switch (read_keyboard())
    {
      case 1:
        // Ожидание отжатия кнопки (для исключения залипания)
        while (read_keyboard()) {};
        // Изменение номера страницы отображения данных на экране
        page_number ++;
        if (page_number > 3)
        {
          page_number = 0;
        }
        break;
      case 2:
        // Ожидание отжатия кнопки (для исключения залипания)
        while (read_keyboard()) {};
        // Форточка
        window_state = 1 - window_state;
        controlDevices();
        break;
      case 3:
        // Ожидание отжатия кнопки (для исключения залипания)
        while (read_keyboard()) {};
        // Освещение
        light_state = 1 - light_state;
        controlDevices();
        break;
      case 4:
        // Ожидание отжатия кнопки (для исключения залипания)
        while (read_keyboard()) {};
        // Помпа
        pump_state = 1 - pump_state;
        controlDevices();
        break;
      case 5:
        // Ожидание отжатия кнопки (для исключения залипания)
        while (read_keyboard()) {};
        // Нагреватель
        heater_state = 1 - heater_state;
        controlDevices();
        break;
      default:
        break;
    }
    // Сброс таймера
    timer_keyboard = millis();
  }

  // Опрос датчика DHT11
  if (millis() > timer_dht11 + DHT11_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensorDHT11();
    // Сброс таймера
    timer_dht11 = millis();
  }

  // Опрос датчика DS18B20 №1
  if (millis() > timer_ds1 + DS1_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensorDS1();
    // Сброс таймера
    timer_ds1 = millis();
  }

  // Опрос датчика DS18B20 №2
  if (millis() > timer_ds2 + DS2_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensorDS2();
    // Сброс таймера
    timer_ds2 = millis();
  }

  // Опрос аналоговых датчиков
  if (millis() > timer_analog + ANALOG_UPDATE_TIME)
  {
    // Опрос датчиков
    readSensorAnalog();
    // Сброс таймера
    timer_analog = millis();
  }

  // Вывод данных на сервер IoT
  if (millis() > timer_iot + IOT_UPDATE_TIME)
  {
    // Вывод данных на сервер IoT
    sendDataIot();
    // Сброс таймера
    timer_iot = millis();
  }

  // Вывод данных на LCD экран
  if (millis() > timer_lcd + LCD_UPDATE_TIME)
  {
    switch (page_number)
    {
      case 0:
        lcd.clear();
        lcd.setCursor(0, 0); lcd_printstr("T = " + String(air_temperature1, 1) + " *C");
        lcd.setCursor(0, 1); lcd_printstr("H = " + String(air_humidity1, 1) + " %");
        break;
      case 1:
        lcd.clear();
        lcd.setCursor(0, 0); lcd_printstr("ST1 = " + String(soil_temperature1, 1) + " *C");
        lcd.setCursor(0, 1); lcd_printstr("ST2 = " + String(soil_temperature2, 1) + " *C");
        break;
      case 2:
        lcd.clear();
        lcd.setCursor(0, 0); lcd_printstr("SM1 = " + String(soil_moisture1, 1) + " %");
        lcd.setCursor(0, 1); lcd_printstr("SM2 = " + String(soil_moisture2, 1) + " %");
        break;
      case 3:
        lcd.clear();
        lcd.setCursor(0, 0); lcd_printstr("LIGHT = " + String(light1, 1) + " %");
        break;
      default:
        break;
    }
    // Сброс таймера
    timer_lcd = millis();
  }
}

// Функция вывода текстовой строки на дисплей (так как в библиотеке криво реализована)
void lcd_printstr(String str1)
{
  for (int i = 0; i < str1.length(); i++)
  {
    lcd.print(str1.charAt(i));
  }
}

// Определение номера нажатой кнопки на клавиатуре
int read_keyboard()
{
  int sensor_data = analogRead(KEYBOARD);
  if (sensor_data < 20)
  {
    return 1;
  }
  else if ((sensor_data > 130) && (sensor_data < 150))
  {
    return 2;
  }
  else if ((sensor_data > 320) && (sensor_data < 340))
  {
    return 3;
  }
  else if ((sensor_data > 495) && (sensor_data < 515))
  {
    return 4;
  }
  else if ((sensor_data > 730) && (sensor_data < 750))
  {
    return 5;
  }
  else
  {
    return 0;
  }
}

// Опрос датчика DHT11
void readSensorDHT11()
{
  // DHT11
  air_humidity1 = dht.readHumidity();
  air_temperature1 = dht.readTemperature();
}

// Опрос датчика DS18B20 №1
void readSensorDS1()
{
  // DS18B20
  ds_sensor1.requestTemperatures();
  soil_temperature1 = ds_sensor1.getTempCByIndex(0);
}

// Опрос датчика DS18B20 №2
void readSensorDS2()
{
  // DS18B20
  ds_sensor2.requestTemperatures();
  soil_temperature2 = ds_sensor2.getTempCByIndex(0);
}

// Опрос аналоговых датчиков
void readSensorAnalog()
{
  // Аналоговые датчики
  light1 = (1023.0 - analogRead(LIGHT)) / 1023.0 * 100.0;
  soil_moisture1 = analogRead(MOISTURE_1) / 1023.0 * 100.0;
  soil_moisture2 = analogRead(MOISTURE_2) / 1023.0 * 100.0;
}

// Подключение к серверу IoT
void sendDataIot()
{
  // Сохраняем в массив данные с датчиков
  sensorValues[0] = air_temperature1;
  sensorValues[1] = air_humidity1;
  sensorValues[2] = light1;
  sensorValues[3] = soil_temperature1;
  sensorValues[4] = soil_moisture1;
  sensorValues[5] = soil_temperature2;
  sensorValues[6] = soil_moisture2;

  // Подключение к серверу
  Serial.println("Connecting to IoT server...");
  //if (client.connect(iot_server, 80))
  if (client.connect(iot_address, 80))
  {
    // Проверка установления соединения
    if (client.connected())
    {
      // Отправка заголовка сетевого пакета
      Serial.println("Sending data to IoT server...\n");
      Serial.print("POST /Thingworx/Things/");
      client.print("POST /Thingworx/Things/");
      Serial.print(thingName);
      client.print(thingName);
      Serial.print("/Services/");
      client.print("/Services/");
      Serial.print(serviceName);
      client.print(serviceName);
      Serial.print("?appKey=");
      client.print("?appKey=");
      Serial.print(appKey);
      client.print(appKey);
      Serial.print("&method=post&x-thingworx-session=true");
      client.print("&method=post&x-thingworx-session=true");
      //Serial.print("<");
      //client.print("<");
      // Отправка данных с датчиков
      for (int idx = 0; idx < sensorCount; idx ++)
      {
        Serial.print("&");
        client.print("&");
        Serial.print(sensorNames[idx]);
        client.print(sensorNames[idx]);
        Serial.print("=");
        client.print("=");
        Serial.print(sensorValues[idx]);
        client.print(sensorValues[idx]);
      }
      // Закрываем пакет
      //Serial.print(">");
      //client.print(">");
      Serial.println(" HTTP/1.1");
      client.println(" HTTP/1.1");
      Serial.println("Accept: application/json");
      client.println("Accept: application/json");
      Serial.print("Host: ");
      client.print("Host: ");
      Serial.println(iot_server);
      client.println(iot_server);
      Serial.println("Content-Type: application/json");
      client.println("Content-Type: application/json");
      Serial.println();
      client.println();

      // Ждем ответа от сервера
      timer_iot_timeout = millis();
      while ((client.available() == 0) && (millis() < timer_iot_timeout + IOT_TIMEOUT1));

      // Выводим ответ о сервера, и, если медленное соединение, ждем выход по таймауту
      int iii = 0;
      bool currentLineIsBlank = true;
      bool flagJSON = false;
      timer_iot_timeout = millis();
      while ((millis() < timer_iot_timeout + IOT_TIMEOUT2) && (client.connected()))
      {
        while (client.available() > 0)
        {
          char symb = client.read();
          Serial.print(symb);
          if (symb == '{')
          {
            flagJSON = true;
          }
          else if (symb == '}')
          {
            flagJSON = false;
          }
          if (flagJSON == true)
          {
            buff[iii] = symb;
            iii ++;
          }
          timer_iot_timeout = millis();
        }
      }
      buff[iii] = '}';
      buff[iii + 1] = '\0';
      Serial.println(buff);
      // Закрываем соединение
      client.stop();

      // Расшифровываем параметры
      StaticJsonBuffer<BUFF_LENGTH> jsonBuffer;
      JsonObject& json_array = jsonBuffer.parseObject(buff);
      //Serial.println(json_array.success());
      //Serial.println(jsonBuffer.capacity());
      //Serial.println(jsonBuffer.size());
      pump_state = json_array["pump"];
      window_state = json_array["roof"];
      heater_state = json_array["temp"];
      light_state = json_array["light"];
      Serial.println("Pump state:   " + String(pump_state));
      Serial.println("Window state: " + String(window_state));
      Serial.println("Heater state: " + String(heater_state));
      Serial.println("Light state:  " + String(light_state));
      Serial.println();

      // Делаем управление устройствами
      controlDevices();
    }
  }
  Serial.println("Packet successfully sent...");
}

// Управление исполнительными устройствами
void controlDevices()
{
  // Форточка
  if (window_state)
  {
    servo_1.write(0);
    servo_2.write(0);
  }
  else
  {
    servo_1.write(90);
    servo_2.write(90);
  }
  // Освещение
  analogWrite(PWM_LED, light_state * 255);
  // Помпа
  digitalWrite(RELAY1, pump_state);
  // Освещение
  digitalWrite(RELAY2, heater_state);
}

