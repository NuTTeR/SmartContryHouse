#define SERIAL_RX_GSM_PIN 17 //для GSM платы - U_TXD
#define SERIAL_TX_GSM_PIN 16 //для GSM платы - U_RXD

//Переменные, которые отвечают за настройки
char* sms_addr = "+79205072565"; //Кому отправлять смс-уведомления

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
  Serial2.readString(); //Заглушки, чтобы получить ответ от модема (без него затыкается)
  Serial2.print(F("AT+CMGS="));
  Serial2.write(0x22); // символ "
  Serial2.print(sms_addr);
  Serial2.write(0x22); // символ "
  Serial2.write(0x0D); // символ перевода каретки 
  Serial2.readString(); //Заглушки, чтобы получить ответ от модема (без него затыкается)
  Serial2.print(sms_text);
  Serial2.write(0x1A); // символ CTRL+Z  
  Serial2.setTimeout(50);
  for (int i=0;i<150;) {
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

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  /*
  Serial2.begin(115200);

  Serial.println("Goodnight moon!");

  // set the data rate for the SoftwareSerial port
  Serial2.println("AT"); */
    Serial.println("ready to get sms");
}

void loop() { // run over and over
  /*
  if (Serial2.available()) {
    Serial.write(Serial2.read());
  }
  if (Serial.available()) {
    Serial2.write(Serial.read());
  } */
  if (Serial.available()) {
    if (send_sms(Serial.readString()))
      Serial.println("True");
    else
      Serial.println("False");
  }

  
}

