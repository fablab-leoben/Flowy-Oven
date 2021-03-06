// Flowy Oven Project. BCN3DTechnologies - Fundacio CIM
// December 2015 - Marc Cobler, Ostap Petrushchack
// DIY Reflow Oven to solder our own electronics boards.


/* The Circuit:
* LCD RS pin to digital pin 34
* LCD Enable pin to digital pin 32
* LCD D4 pin to digital pin 30
* LCD D5 pin to digital pin 36
* LCD D6 pin to digital pin 28
* LCD D7 pin to digital pin 38
* Reset Button to digital pin 1
* Encoder connected to digital pins  15 y 16
* Buzzer to digital pin 20
* Relay to digital pin 17
* Controlled Fan to digital pin 14
*/

#include <SPI.h>
#include <LiquidCrystal.h>
#include "Adafruit_MAX31855.h" //library MX31855 from adafruit

// Declare the LCD display object
LiquidCrystal lcd(34, 32, 30, 36, 28, 38);//LCD pins

//define Thermocouple pins
#define thermoCLK 52
#define thermoCS 50
#define thermoDO 48

//Define the other pins
#define SSR 17
#define input_switch 21
#define fan1 14
#define buzzer 20

//Define target temperatures and times
#define PREHEAT 130
#define PREHEATDURATION 60000 //1 minute preheating
#define REFLOW 245
#define REFLOWDURATION 30000  //30 seconds reflowing
#define RAMPDOWN 0

// Initialize the Thermocouple
// initialize the library with the numbers of the interface pins
Adafruit_MAX31855 thermocouple(thermoCLK, thermoCS, thermoDO);

//------------------VARIABLES----------------------

// Reflow status strings
String reflow_status[4] = {"press to start","preheat","critical","rampdown"};
// List of tones to play
int tones[] = {261, 277, 294, 311, 330, 349, 370, 392, 415, 440};
//            mid C  C#   D    D#   E    F    F#   G    G#   A}
#define TONESSIZE sizeof(tones)/sizeof(int)
double temperature, old_temperature;
double c;
unsigned long start_time=0, old_time;
unsigned long counter=0;
int setpoint = 0, reflow_stage=0, rate;
int i;
//-------------------------------------------------
void setup()
  {
    //Declare pinModes
    pinMode(SSR, OUTPUT);//solidstate switch
    pinMode(input_switch, INPUT);//switch input
    pinMode(fan1, OUTPUT);//fan
    pinMode(buzzer, OUTPUT);//fan

    digitalWrite(buzzer, LOW);
    //pinMode(1, INPUT);//reset
    //digitalWrite(1,HIGH);

    // Initialize the lcd Screen
    lcd.begin(16, 4);
    lcd.clear();
    lcd.setCursor(5,0);
    lcd.print("BCN3DTech");
    lcd.setCursor(4,1);
    lcd.print("Flowy Oven");

    delay(2500);

    Serial.begin(9600);//for monitoring and logging in processing
    Serial.println("Starting the BCN3DTech Flowy Oven");

    Serial.print("Internal Temp: ");
    Serial.println(thermocouple.readInternal());
    temperature = thermocouple.readCelsius();
    while(isnan(temperature)) {
      Serial.println("Error! Please check and reboot");
      delay(1000);
      temperature = thermocouple.readCelsius();
    }
    Serial.print("Oven temperature: ");
    Serial.println(temperature);

}   // end Setup

void loop(){
  get_temp();
  reflow_update();
  lcd_update();
  heat_control();
  switch_check();
  fan_control();

}//loop

void get_temp(){
  delay(50);     //Calm down the sensor and the reading
  temperature = thermocouple.readCelsius();
  Serial.print("Temp = ");
  Serial.println(temperature);
}// end get temp

void reflow_update(){

  if(reflow_stage==0){//idle   idle   idle
    setpoint=0;//don't do anything
  }//0

  if(reflow_stage==1){//preheat   preheat   preheat
    if(start_time == 0 && temperature>120)//start timer for preheat soak
    start_time=millis();

    setpoint=PREHEAT;//regulate here during preheat
    rate = (temperature-old_temperature)/((millis()-old_time)/1000);//get rate of rise
    if(rate>3)//if rising too fast, kill the setpoint so it slows down
    setpoint=0;

    old_time = millis();//used to find rate
    old_temperature=temperature;

    if((millis()-start_time)>PREHEATDURATION && start_time !=0){//this is where it checks to see if it needs to move on to the next stage
      reflow_stage++;
      start_time=0;}
  }//1
  if(reflow_stage==2){//critical  critical  critical
    if(start_time ==0 && temperature>195 )//start timer during this stage
    start_time=millis();

    setpoint=REFLOW;//regulate here
    rate = (temperature-old_temperature)/((millis()-old_time)/1000);//eh, you can regulate the rate here as well if needed
    if(rate>3)
    setpoint=0;

    old_time = millis();//rate stuff again
    old_temperature=temperature;

    if((millis()-start_time)>REFLOWDURATION && start_time !=0){//how long to stay in
      reflow_stage++;
      start_time=0;}//estaba en 20000
  }//2

   if(reflow_stage==3){//rampdown   rampdown   rampdown
    if(start_time ==0)
    start_time=millis();

    setpoint=0;
    rate = (temperature-old_temperature)/((millis()-old_time)/1000);
    if(rate<-20)//if rate is too fast you can turn heater back on
    setpoint=205;
    old_time = millis();
    old_temperature=temperature;
    if((millis()-start_time)>PREHEATDURATION && start_time !=0){
      reflow_stage=0;
      start_time=0;
      playTone();
    }
  }//3


}// end reflow update

void lcd_update()
  {
    lcd.clear();  //Sets cursor position to zero, zero
    lcd.print("Temp=");
    lcd.print(temperature,1);
    lcd.setCursor(0,1);
    lcd.print("Rate=");
    lcd.print(rate); //show the rate as well
    lcd.print("C/s");
    lcd.setCursor(12,1);
    if (reflow_stage > 0 && reflow_stage < 3) {
      lcd.print("t=");
      lcd.print((millis()/1000)-counter);
      lcd.print("s");
    } else {
      lcd.print("idle");
    }
    lcd.setCursor(4,4);
    lcd.print(reflow_status[reflow_stage]);
}// end lcd_update

void heat_control() {
    if(temperature<setpoint) {
      digitalWrite(SSR, HIGH);
    } else  {
      digitalWrite(SSR,LOW);
    }
}//end heat_control

void switch_check(){//could be better, but it works
  int button = digitalRead(input_switch);
  //int reset = digitalRead(1);
  //if(reset==LOW) digitalWrite(buzzer, HIGH);
  //else digitalWrite(buzzer, LOW);
  if(button==LOW){
    lcd.clear();
    if(reflow_stage>0){
      counter = 0;
      lcd.setCursor(4,1);
      lcd.print("Stopping...");
      reflow_stage=0;
      delay(1000);
    } else  {
      counter = millis()/1000;
      lcd.setCursor(4,1);
      lcd.print("Starting...");
      reflow_stage=1;
    }
    delay(1000);
  }

}// end switch check

void fan_control(){
  if(reflow_stage ==0 || reflow_stage ==3)//keep on continuously if idle or ramping down
  digitalWrite(fan1, HIGH);
  else
  digitalWrite(fan1, LOW);

  if(reflow_stage >0  && reflow_stage <3){//give a little 'love tap' if the temp is higher than the setpoint or if it's ramping quickly
    if(temperature>setpoint || rate>1){
    digitalWrite(fan1, LOW);
    delay(500);
    digitalWrite(fan1,HIGH);}
  }

}// end fan

void playTone() {
  for (size_t i = 0; i < TONESSIZE; i++) {
    tone(buzzer, tones[i]);
    delay(250);
  }
  noTone(buzzer);
}// end playTone
