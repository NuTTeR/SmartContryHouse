#include <OneWire.h>
#include <MemoryFree.h>
#include <Wire.h>
#include <BH1750.h>
#include <DHT.h>
#include <SPI.h>
#include <Ethernet2.h>
#include <EEPROM.h>


//Пины
#define DHT_PIN 7 // Датчик температуры\влажности уличный
#define DS18B20_PIN 6 //Датчик температуры воды (4.7 кОм между датой и +5В)
#define DETECTOR220_PIN 4 //Датчик 220 (Обратная логика! 0 - 220 есть, 1 - 220 нет) (между входом и землей - 10 или 20 кОм, но это не точно)
#define WATER_LEVEL_PIN 5 //Датчик уровня воды (1 - наполнен) (между входом и землей - 10 или 20 кОм)
#define AC_LIGHT1_PIN 9 //Вывод для контроллера света 1
#define AC_LIGHT2_PIN 11 //Вывод для контроллера света 2 
#define SERIAL_RX_GSM_PIN 17 //для GSM платы - U_TXD
#define SERIAL_TX_GSM_PIN 16 //для GSM платы - U_RXD
#define WEB_CS_PIN 10 //SCS пин для Ethernet модуля (W5500)
#define WEB_MISO_PIN 50 //MISO для ethernet (между ним и землей 10ком)
#define WEB_MOSI_PIN 51 //MOSI для ethernet (между ним и землей 10ком)
#define WEB_SCK_PIN 52 //SCLK для ethernet (между ним и землей 10ком)
#define I2C_SDA_PIN 20 //SDA для датчика света BH1750
#define I2C_SCL_PIN 21 //SCL для датчика света BH1750

 
//EEPROM адреса настроек
#define EEPROM_PHONE_START 0 //Стартовый адрес хранения телефона
#define EEPROM_PHONE_END 4 //Конечный адрес хранения телефона
#define EEPROM_LIGHT_DIMMERS 2 //Кол-во диммеров (модулей управления светом
#define EEPROM_LIGHT_LEVELS 5 //Кол-во значений(порогов) света
const byte EEPROM_LIGHT[EEPROM_LIGHT_DIMMERS][EEPROM_LIGHT_LEVELS] = {{5,7,9,11,13},{15,17,19,21,23}}; //Хранение настроек порога срабатывания регулировки света в uint для 1 и 2 датчика
const byte EEPROM_AUTOLIGHT[2] = {25,26}; //Включен ли автосвет
const byte EEPROM_LIGHT_MANUAL[2] = {27,28}; //Уровень ручной регулировки (% яркости света)

//Переменные, которые отвечают за настройки
char* sms_addr = "+79205072565"; //Кому отправлять смс-уведомления
int light[EEPROM_LIGHT_DIMMERS][EEPROM_LIGHT_LEVELS]; //Какой уровень освещенности должен быть от датчика света для n модуля на x процентах light_n_x
bool light_auto[2]; //Авторегулирование света
byte light_manual_level[2] = {100, 100}; //Ручная регулировка света, процент яркости

//Переменные, хранящие текущее значение датчиков и выхходных значений
int cur_luminosity[2] = {-1, -1}; //Освещенность (для обоих датчиков)
byte cur_light_level[2] = {0, 0}; //Текущий уровень света

//Переменные для усреднения данных по датчикам выше (необходимо сбросить при старте в clear_vars()!!)
#define LAST_DATA_NUM 20 //Кол-во усредненных значений, по которому считается текущее
int last_luminosity[2][LAST_DATA_NUM]; //Освещенность(для обоих датчиков)

//Служебные настройки и инициализация
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
EthernetServer server(80);

//ФУНКЦИИ
//Функция для вытаскивания параметров из строк вида "data1=value&data2=value"
String web_getParam(const String& data, const String& param) { 
  unsigned int paramPosition;
  unsigned int dataPosition;
  unsigned int dataEndPosition;
  String returnValue;
  
  paramPosition = data.indexOf(param);

  if (paramPosition == -1)
    return "";

  dataPosition = paramPosition + param.length() + 1;
  dataEndPosition = data.indexOf('&', dataPosition);
  if (dataEndPosition == -1)
    returnValue = data.substring(dataPosition);
  else
    returnValue = data.substring(dataPosition, dataEndPosition);
  return returnValue;
}

//Сделать UPDATE для EEPROM любого типа(не только byte)
template < typename T > void EEPROM_update(int addr, T val) { 
  T readVal;
  EEPROM.get(addr, readVal);
  if (readVal == val) return;
  EEPROM.put(addr, val);  
}

//Вытащить из EEPROM номер телефона для уведомлений
String EEPROM_getPhone() {
  String returnValue;
  for (int i = EEPROM_PHONE_START; i <= EEPROM_PHONE_END; i++) {
    if (EEPROM[i] < 10)
      returnValue += "0" + String(EEPROM[i]);
    else
      returnValue += String(EEPROM[i]);
  }
  return returnValue;
}

//Отправка sms
//не забыть замкнуть vcc_in и pwr пины
bool send_sms(const String& sms_text){ //Огриничение на текст около 160 символов!!
  String gsm_all_output;
  String gsm_cur_output;
  bool gsm_first_check;
  bool gsm_succeed;
  gsm_all_output = "";
  gsm_first_check = false;
  gsm_succeed = false;
  Serial2.begin(115200);
  Serial2.println(F("AT+CMGF=1"));
  Serial2.readString(); //Заглушка, чтобы получить ответ от модема
  Serial2.print(F("AT+CMGS="));
  Serial2.write(0x22); // символ "
  Serial2.print(sms_addr);
  Serial2.write(0x22); // символ "
  Serial2.write(0x0D); // символ перевода каретки 
  Serial2.readString(); //Заглушка, чтобы получить ответ от модема
  Serial2.print(sms_text);
  Serial2.readString(); //Заглушка, чтобы получить ответ от модема
  Serial2.write(0x1A); // символ CTRL+Z  
  Serial2.setTimeout(50);  
  for (int i=0;i<200;) {
    gsm_cur_output = Serial2.readString();
    if (gsm_cur_output == "") { //защита от зацикливания при пустом ответе
      i++;    
    }
    gsm_all_output+=gsm_cur_output;
    if (gsm_all_output.indexOf("+CMGS:") > 0) { // 1 шаг проверки
      gsm_all_output = gsm_all_output.substring(gsm_all_output.indexOf("+CMGS:"));
      gsm_first_check = true;
    }
    if (gsm_first_check && gsm_all_output.indexOf("OK") > 0) { // 2 шаг проверки
      gsm_succeed = true; //Успешная отправка
      break;
    }
  }
  Serial2.end();
  return gsm_succeed;
}

//Выставить уровень света
//МЕЖДУ А0 на слейве и землей конденсатор на 4.7мкФ (uF)
//Между А0 и цифровым выходом мастера (AC_LIGHTn_PIN) - резистор 47к
void set_light_level(byte sensor = 0, byte percent = 0) {
  byte pin;
  byte analogParam;
  //Параметр analogWrite: От 15 до 210 рабочая вилка, 0 - выкл, 250 стабильный вкл  
  if (sensor == 0) {
    pin = AC_LIGHT1_PIN; 
    cur_light_level[0] = percent;
  } else {
      pin = AC_LIGHT2_PIN;
      cur_light_level[1] = percent;
  }  
  if (percent < 5) analogParam = 0;
  else if (percent > 95) analogParam = 250; 
  else {
    analogParam = 15 + (percent * 2);
  }
  analogWrite(pin, analogParam);
}

//Получить с произвольного пина(датчика) 0 или 1
bool get_data(byte pin) {
  pinMode(pin, INPUT);
  int digTrueCount = 0;
  for (int i=0;i<10;i++) {
    if (digitalRead(pin)) digTrueCount++;
    delay(10);
  }
  if (digTrueCount > 5) return true;
  return false;
}

//Получить с внешнего датчика температуру воды DS18B20
//Возвращает 0 если что-то не то с данными
//XXX: ВНИМАНИЕ!!! НУЖЕН РЕЗИСТОР 4.7кОм между 5В и Data
float get_water_temp() {
  OneWire  ds(DS18B20_PIN);  
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;
  if ( !ds.search(addr)) {
    Serial.println(F("[DS18B20] Not found sensor"));
    ds.reset_search();
    return 0;
  }
  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println(F("[DS18B20] CRC is not valid!"));
      return 0;
  }
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:
      Serial.println(F("[DS18B20] Device is not a DS18x20 family device."));
      return 0;
  } 
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }
  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  return celsius;  
}

//Получить с внешнего датчика тем. DHT21 влажность или температуру
//Возвращает 0 если что-то не то с данными
//XXX:делать паузу между вызовом температуры и влажности
float get_dht_value(byte data = 1) {
  DHT dht(DHT_PIN, DHT21);
  dht.begin();
  float return_value = 0;
  if (data == 1)
    return_value = dht.readTemperature();
  else
    return_value = dht.readHumidity();
  if (isnan(return_value)) {
    Serial.println(F("Failed to read from DHT21 sensor!"));
    return 0;
  }
  return return_value;
}
float get_outside_temp() {
  return get_dht_value(1);
}
float get_outside_humidity() {
  return get_dht_value(2);
}

//Получить свет с датчиков света BH1750
//Пины: 20 - SDA, 21 - SCL
//-1 - датчик не работает, иначе - кол-во люкс (сферическое в вакууме)
int get_light(byte sensor = 1) { 
  byte sensor_addr = 0x23;
  if (sensor == 2) sensor_addr = 0x5C;
  BH1750 lightMeter(sensor_addr);
  Wire.begin();
  if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE))
      return -1;
  int lux = (int) lightMeter.readLightLevel();
  Wire.end();
  if ((lux > -1) && (lux < 65535)) return lux;
  return -1;
}

//Обработка действий и сохранение данных, полученных через web-интерфейс
void web_postHandle(const String& data) {
  String dataValue;
  Serial.println(data);
  if (data == "") return;
  //Обработка номера телефона //0-4
  dataValue = web_getParam(data, F("phone"));
  if (dataValue != "" && dataValue.length() == 10) {
      for(int i = 0; i < dataValue.length(); i++) {
         EEPROM.update(i + EEPROM_PHONE_START, dataValue.substring(i * 2, (i * 2) + 2).toInt());
      }
      dataValue.toCharArray(sms_addr, 10);
  }
  //Обработка порога срабатывания света для обоих диммеров //5-24
  for (int i = 0; i < EEPROM_LIGHT_DIMMERS; i++) {
    for (int j = 0; j < EEPROM_LIGHT_LEVELS; j++) {
      dataValue = web_getParam(data, "light_" + String(i) + "_" + String(j));
      if (dataValue != "" && (j == 0 || light[i][j-1] < dataValue.toInt())) {
        light[i][j] = dataValue.toInt();
        EEPROM_update(EEPROM_LIGHT[i][j], light[i][j]);   
      }
    }
  }
  //Уровень ручной регулировки //27-28
  for (int i = 0; i < 2; i++) {
    dataValue = web_getParam(data, "light_manual_level_" + String(i));
    if (dataValue != "" && dataValue.toInt() >= 0 && dataValue.toInt() <= 100) {
        light_manual_level[i] = dataValue.toInt();
        EEPROM.update(EEPROM_LIGHT_MANUAL[i], light_manual_level[i]);
    }
  }
  //Ручное управление светом (срабатывание кнопки вкл или выкл)
  bool manual_light_rcvd[2] = {false, false};
  for (int i = 0; i < 2; i++) {
    dataValue = web_getParam(data, "light_manual_" + String(i));
    if (dataValue == "on") {
      manual_light_rcvd[i] = true;
      set_light_level(i, light_manual_level[i]);
    } else if (dataValue == "off") {
      manual_light_rcvd[i] = true;
      set_light_level(i, 0);
    }
  }  
  //Автосвет(вкл\выкл) //25-26
  for (int i = 0; i < 2; i++) {
    dataValue = web_getParam(data, "autolight_" + String(i));
    if (dataValue != "" || manual_light_rcvd[i]) {
      light_auto[i] = (dataValue == "on");
      if (manual_light_rcvd[i]) light_auto[i] = false; //Отключение автосвета если пришла команда на ручное управление
      EEPROM.update(EEPROM_AUTOLIGHT[i], light_auto[i]);
    }
  }
  EEPROM_readSettings(); //XXX: временно (а может и постоянно)
}

//Основная процедура для работы веб-сервера (прием коннекта)
void web_main() {  
  EthernetClient client = server.available();
  if (client) {    
    Serial.println(F("new client"));
    String curStr = "";
    boolean dataAccepted = false;
    unsigned int loadCycle = 0;
    while (client.connected()) {
      if (client.available()) {
        //Обработка ответа построчно
        char c = client.read();
        Serial.print(c);        
        if (!(c == '\r' || c == '\n')) {
          curStr+=c;
          continue;
        }        
        if (curStr == "") continue;
        
        Serial.println(curStr);        
        dataAccepted = true;
        
        curStr = "";
      }
      else if(dataAccepted) { //Коннект прекратился обычно на этом этапе
        loadCycle++;
        if (loadCycle < 300) {
          delay(1);
          continue;
        }

        //Последние необработанные данные = данные POST запроса (если они есть)
        web_postHandle(curStr);        

        /* ОТВЕТ */
        
        // Header овета
        client.println(F("HTTP/1.1 200 OK"));
        client.println(F("Content-Type: text/html; charset=utf-8"));
        client.println(F("Connection: close"));  // the connection will be closed after completion of the response
        client.println();        
        
        //Тело ответа
        client.print(F("<!DOCTYPE HTML>"));
        client.print(F("<html>"));
        client.print(F("<head><meta charset='utf-8'><title>Smart country house v0.1</title></head>"));
        client.print(F("<body>"));        

        //Текущие показания
        client.print(F("<br><h2>Текущие значения</h2>"));
        //Освещенность
        client.print(F("<div>Освещенность 1: <i>"));
        client.print(cur_luminosity[0]);
        client.print(F(" lx</i>; 2: <i>"));
        client.print(cur_luminosity[1]);
        client.print(F(" lx</i></div>"));
        client.print(F("<div>Мощность света Контур 1: <i>"));
        client.print(cur_light_level[0]);
        client.print(F("%</i>; Контур 2: <i>"));
        client.print(cur_light_level[1]);
        client.print(F("%</i></div>"));       
        
        //Ручное управление
        client.print(F("<br><h2>Ручное управление</h2>"));
        client.print(F("<form action='' method='post'>"));
        client.print(F("<div><h4>Управление светом: </h4>"));
        for(int i = 0; i < 2; i++) {
          client.print(F("<b>Контур "));
          client.print(i+1);
          client.print(F(": </b>"));
          client.print(F("<button name='light_manual_"));
          client.print(i);
          client.print(F("' value='on'>Включить</button>"));
          client.print(F("<label for='light_manual_level_"));
          client.print(i);
          client.print(F("'> мощность </label>"));
          client.print(F("<input type='number' min='1' max='100' name='light_manual_level_"));
          client.print(i);
          client.print(F("' value='"));
          client.print(light_manual_level[i]);
          client.print(F("'>% "));
          client.print(F("<button name='light_manual_"));
          client.print(i);
          client.print(F("' value='off'>Выключить</button><br>"));
        }        
        client.print(F("</form>"));


        //Формы (Настройки)
        client.print(F("<br><br><h2>Настройки</h2>"));
        client.print(F("<form action='' method='post'>"));
        //Телефон
        client.print(F("<div><label for='phone'>Телефон для СМС уведомлений (без 8 и +7): </label>"));
        client.print(F("<input type='text' pattern='[0-9]{10}' name='phone' value='"));
        client.print(EEPROM_getPhone());
        client.print(F("'></div>"));
        //Настройки автосвета
        client.print(F(""));
        client.print(F("<div><h4>Настройка света: </h4>"));
        for (int i = 0; i < 2; i++) {
            client.print(F("<label for='autolight_"));
            client.print(i);
            client.print(F("'>Автосвет контур "));
            client.print(i + 1);                      
            client.print(F("</label>"));                            
            client.print(F("<input type='checkbox' value='on' name='autolight_"));
            client.print(i);
            client.print(F("' "));
            if (light_auto[i])
              client.print(F("checked"));            
            client.print(F("><br>"));
            client.print(F("<input type='hidden' value='off' name='autolight_"));
            client.print(i);
            client.print(F("'>"));      
        }
        for (int i = 0; i < EEPROM_LIGHT_DIMMERS; i++) {
          client.print(F("<br>Диммер "));
          client.print(i + 1);
          client.print(F(": "));
          for (int j = 0; j < EEPROM_LIGHT_LEVELS; j++) {            
            client.print(F("<label for='light_"));
            client.print(i);
            client.print(F("_"));
            client.print(j);
            client.print(F("'> "));
            client.print(j * 25);
            client.print(F("%</label><input type='text' pattern='\\d*' name='light_"));
            client.print(i);
            client.print(F("_"));
            client.print(j);
            client.print(F("' value='"));
            client.print(light[i][j]);
            client.print(F("'>"));
          }
        }
        
        client.print(F("<br><br><div><button>Сохранить</button></div></form>"));
        
        client.print(F("<br><div>Свободная память: "));
        client.print(freeMemory());
        client.print(F("</div>"));


        
        client.print(F("</body></html>"));

        //Очистка данных
        dataAccepted = false;
        loadCycle = 0;
        curStr = "";
        break;        
      }

    }
    // give the web browser time to receive the data
    delay(10);
    // close the connection:
    client.stop();
    Serial.println(F("client disconnected"));
  }
}

//Считывание настроек с памяти в переменные
void EEPROM_readSettings() {
  //получение телефона
  String("+7" + EEPROM_getPhone()).toCharArray(sms_addr, 10);
  //получение зависимости освещенность\яркость света
  for (int i = 0; i < EEPROM_LIGHT_DIMMERS; i++) {
    for (int j = 0; j < EEPROM_LIGHT_LEVELS; j++) {
      EEPROM.get(EEPROM_LIGHT[i][j], light[i][j]);
    }
  }
  //Автосвет(вкл\выкл) 
  for (int i = 0; i < 2; i++) {    
    EEPROM.get(EEPROM_AUTOLIGHT[i], light_auto[i]);
  }
  //Уровень ручной регулировки света
  for (int i = 0; i < 2; i++) {        
        EEPROM.get(EEPROM_LIGHT_MANUAL[i], light_manual_level[i]);
        if (light_manual_level[i] > 100 || light_manual_level[i] <= 0) 
          light_manual_level[i] = 100;
  }
}

//Добавление элемента в массив со смещением самого старого элемента (для массивов last_* с длинной LAST_DATA_NUM)
template < typename T > void array_add(T *arr, T val) { 
  for (int i = LAST_DATA_NUM - 1; i > 0; i--) {
    arr[i] = arr[i - 1];
  }
  arr[0] = val;
}

//Считывание информации со всех датчиков
void get_current() {
  int int_tmp;
  float float_tmp;
  //Датчики освещенности
  for (int lum_num = 0; lum_num < 2; lum_num++) {
    int_tmp = get_light(lum_num + 1);
    array_add(last_luminosity[lum_num], int_tmp);
    float_tmp = 0;
    int_tmp = 0;
    for (int i = 0; i < LAST_DATA_NUM; i++) {
      if (last_luminosity[lum_num][i] >= 0) {
        float_tmp += last_luminosity[lum_num][i]; //Сохраняем все значения, которые не -1
        int_tmp++; //Сохраняем сколько значений мы добавили в переменнную выше
      }
    }
    if (int_tmp > 0 ) {
      cur_luminosity[lum_num] = (int) (float_tmp / int_tmp);
    } else { 
      cur_luminosity[lum_num] = -1; 
    }
  }
  

}

//Очистка переменных (сброс в нулевые значения)
void clear_vars() {
  for (int i = 0; i < 2;i++) {
    for (int j = 0; j < LAST_DATA_NUM; j++) {
      last_luminosity[i][j] = -1;
    }
  }
}

//Управление светом (автоуровень)
void lights_management(){
  if (cur_luminosity[0] == -1) return;
  //Между этими значениями люкс будет высчитан процент света
  int light_level_start; 
  int light_level_end;
  for (int i = 0; i < EEPROM_LIGHT_DIMMERS; i++) {
    if (!light_auto[i]) continue; //Управление автосветом (ручной в управлении в цикле не нуждается) TODO
    for (int j = 0; j < EEPROM_LIGHT_LEVELS; j++) {
      light_level_start = light[i][j];
      //Граничный случай, меньше порогового значения для включения
      if (j == 0 && cur_luminosity[0] < light_level_start) {
        set_light_level(i, 0);
        break;
      }
      //Граничный случай, больше порогового значения для 100% включения
      if (j == EEPROM_LIGHT_LEVELS - 1 /* && cur_luminosity[0] >= light_level_start */) {
        set_light_level(i, 100);
        break;
      }
      light_level_end = light[i][j+1];
      //Если находится в каких-либо границах
      if (cur_luminosity[0] >= light_level_start && cur_luminosity[0] < light_level_end) {
        set_light_level(i, int((( (float)(cur_luminosity[0] - light_level_start) / (float)(light_level_end - light_level_start)) * 25) + 0.5) + (j * 25));
        break;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("System start"));
  //Для того, чтобы свет был выкл. при старте процедуры
  pinMode(AC_LIGHT1_PIN, OUTPUT);
  pinMode(AC_LIGHT2_PIN, OUTPUT);
  set_light_level(0, 0);
  set_light_level(1, 0);
  //Считывание настроек из памяти
  EEPROM_readSettings();
  //Очистка переменных
  clear_vars();
  //Запуск веб-сервера
  //TODO: пока нет коннекта- не запускать процедуру loop
  Serial.println(F("begin ethernet"));
  Ethernet.begin(mac);
  Serial.println(F("begin ethernet2"));
  server.begin();
  Serial.print(F("Webserver is at "));
  Serial.println(Ethernet.localIP());
}

void loop() {
  //Веб-сервер
  web_main();
  //Получение инф-ции для отображения с датчиков
  get_current();
  //Управление светом
  lights_management();
  
  delay(1000);
  

}





