#define PWM_PIN 3 
#define ZC_PIN 2
#define ANALOG_INPUT_PIN A0 //пин входных данных

//МЕЖДУ А0 и землей конденсатор на 4.7мкФ (uF)
//Между А0 и цифровым выходом мастера - резистор 47к

int dimming = 0;  // Dimming level (0-128)  0 = ON, 128 = OFF
int current_percentage = 0; 

void setup()
{
  Serial.begin(9600);
  pinMode(PWM_PIN, OUTPUT);// Set AC Load pin as output
  attachInterrupt(0, zero_crosss_int, RISING);  // 0 прерывание = 2 пин (ZC_PIN)
  Serial.println(F("System dimming light start"));
}

void zero_crosss_int()  //function to be fired at the zero crossing to dim the light
{
  if (dimming >= 120) {
    digitalWrite(PWM_PIN, HIGH);
    return;
  }
  if (dimming < 5) return;
  // Firing angle calculation : 1 full 50Hz wave =1/50=20ms 
  // Every zerocrossing thus: (50Hz)-> 10ms (1/2 Cycle) 
  // For 60Hz => 8.33ms (10.000/120)
  // 10ms=10000us
  // (10000us - 10us) / 128 = 75 (Approx) For 60Hz =>65

  int dimtime = (75*(128 - dimming));    // For 60Hz =>65    
  delayMicroseconds(dimtime);    // Wait till firing the TRIAC
  digitalWrite(PWM_PIN, HIGH);   // Fire the TRIAC
  delayMicroseconds(10);         // triac On propogation delay (for 60Hz use 8.33)
  digitalWrite(PWM_PIN, LOW);    // No longer trigger the TRIAC (the next zero crossing will swith it off) TRIAC
}

void loop()  {
  int percentage;
  percentage = 0;
  for (int i=0; i<=10;i++) {      
      percentage += analogRead(ANALOG_INPUT_PIN);
      delay(20);
  }
  percentage = percentage / 10; //10 чтений подряд для точности
  if (percentage < 0) percentage = 1;
  if (percentage > 1000) percentage = 1000;
  percentage = percentage / 10; //1000 / 10 = готовый процент.

  if (percentage > current_percentage) {
    for (int i=current_percentage*1.25F; i <= percentage*1.25F; i++){
      dimming=i;
      delay(35);
    }  
  } else if (percentage < current_percentage) {
    for (int i=current_percentage*1.25F; i >= percentage*1.25F; i--){
      dimming=i;
      delay(35);
    }  
  } 
  current_percentage = percentage; 

  Serial.print(F("Input percentage:"));
  Serial.println(percentage);
  Serial.print(F("Dimming:"));
  Serial.println(dimming);


  delay(1000);
}
