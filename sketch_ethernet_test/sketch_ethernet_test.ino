#include <SPI.h>
#include <Ethernet2.h>
#include <MemoryFree.h>
#include <EEPROM.h>


// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:


// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

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

//Сохранение данных, полученных через web-интерфейс в память EEPROM
void web_postHandle(const String& data) {
  String dataValue;
  Serial.println(data);
  if (data == "") return;
  //Обработка номера телефона
  dataValue = web_getParam(data, F("phone"));
  if (dataValue != "" && dataValue.length() == 10) {
      for(int i = 0; i < dataValue.length(); i++) {
         EEPROM.update(i + EEPROM_PHONE_START, dataValue.substring(i * 2, (i * 2) + 2).toInt());
      }
  }  
}

//Вытащить из EEPROM byte(0..255) значение
byte EEPROM_getInt(unsigned int addr) {
  return EEPROM[addr];
}

//Вытащить из EEPROM булево значение
bool EEPROM_getBool(unsigned int addr) {
  if (EEPROM[addr])
    return true;
  return false;
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

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);


  // start the Ethernet connection and the server:
  Ethernet.begin(mac);
  server.begin();
  Serial.print(F("Webserver is at "));
  Serial.println(Ethernet.localIP());
}


void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {    
    Serial.println(F("new client"));
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    String curStr = "";
    boolean dataAccepted = false;
    unsigned int loadCycle = 0;
    while (client.connected()) {
      if (client.available()) {
        //Обработка ответа построчно
        char c = client.read();
        //Serial.print(c);        
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
        client.println(F("Content-Type: text/html"));
        client.println(F("Connection: close"));  // the connection will be closed after completion of the response
        client.println();
        
        //Тело ответа
        client.println(F("<!DOCTYPE HTML>"));
        client.println(F("<html>"));
        client.println(F("<head><meta charset='utf-8'><title>Smart country house v0.1</title></head>"));
        client.println(F("<body>"));



        //Формы (Настройки)
        client.println(F("<br><br><h2>Настройки</h2>"));
        client.print(F("<form action='' method='post'>"));
        //Телефон
        client.print(F("<div><label for='phone'>Телефон для СМС уведомлений (без 8 и +7): </label>"));
        client.print(F("<input type='text' pattern='[0-9]{10}' name='phone' value='"));
        client.print(EEPROM_getPhone());
        client.print(F("'></div>"));
        
        client.print(F("<br><div><button>Сохранить</button></div></form>"));
        
        client.print(F("<br><div>Свободная память: "));
        client.print(freeMemory());
        client.print(F("</div>"));


        
        client.println(F("</body></html>"));

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

