#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>
#include <Time.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <Wire.h>
#include <RCSwitch.h>

char auth[] = "blynkauthcode";
char ssid[] =  "ssid";
char pass[] =  "wifi-password";
char zapni[] = "1100001110101110";   //binary code to turn on the heating
char vypni[] = "1100010110101110";   //binary code to turn off the heating
char Date[16];
char Time[16];

IPAddress arduino_ip (XXX,XXX,XXX,XXX); //ESP static IP
IPAddress gateway_ip ( 192,168,2,50);
IPAddress subnet_mask(255,255,255,0);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

long startseconds;            // start time in seconds
long stopseconds;             // stop  time in seconds
long nowseconds;              // time now in seconds
float measurement;
float celsius = 25;
int poslsignal = 2;
int timer1 = 0;               //weekend
int timer2 = 0;               //weekday morning
int timer3 = 0;               //weekday evening
int ciltepl = 0;
int zaptepl = 0;
int vyptepl = 0;
int norefresh;
SimpleTimer timer; 
WidgetRTC rtc;
WidgetTerminal terminal(V10);
OneWire oneWire(D2);
DallasTemperature sensors(&oneWire);

RCSwitch mySwitch = RCSwitch();



void reconnectBlynk() {
  sensors.requestTemperatures();
  measurement = sensors.getTempCByIndex(0);
  if (measurement > 1 && measurement < 50) {
    celsius = measurement;      
  }
  Blynk.virtualWrite(V7, celsius); //V7 = current temperature
  Blynk.virtualWrite(V17, ciltepl); //V17 = target temperature
  if (!Blynk.connected()) {
    digitalWrite(D5, LOW);
    digitalWrite(D4, HIGH);
    if (Blynk.connect()) {
    terminal.print("Blynk reconnected, cas ");
    terminal.print(Time);
    terminal.print(" dne ");
    terminal.println(Date);
    terminal.flush(); 
    }
    else {
      digitalWrite(D5, HIGH);
      digitalWrite(D4, LOW);
    }

  }
}

void refreshsignal() {
  if (poslsignal == 0 && norefresh == 0) {
    digitalWrite(D4, HIGH);  
    mySwitch.send(vypni);
    Blynk.virtualWrite(V4, 0);    
    digitalWrite(D4, LOW);      
  }
  if (poslsignal == 1) {
    digitalWrite(D5, LOW);  
    mySwitch.send(zapni);
    Blynk.virtualWrite(V4, 255);  
    digitalWrite(D5, HIGH);      
  }
}

void handleRoot() {
  String message = "Wifi termostat\n\n";
  message += "\nIn order to update the firmware, go to http://XXX.XXX.XXX.XXX/update";
  httpServer.send(200, "text/plain", message);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  digitalWrite(D5, HIGH);
  mySwitch.enableTransmit(D6);        //D6 = 433MHz
  mySwitch.setRepeatTransmit(15);     //repeat the signal 15 times
  mySwitch.setProtocol(6);    //radio signal encoding, for more info check the library rcswitch
  Serial.println("IT'S ALIVE!");
  WiFi.config(arduino_ip, gateway_ip, subnet_mask);  
  WiFi.begin(ssid, pass);    
  while (WiFi.status() != WL_CONNECTED  && (millis() < 30000)) {
    delay(250);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect");
  }
  else {
    Serial.println("WiFi connected");    
  }
  Blynk.config(auth, IPAddress(192,168,2,71), 8090);
  while (Blynk.connect() == false && (millis() < 300000)) {
  }
  if (Blynk.connected()) {
    Serial.println("Blynk connected");
  }  
  else {
    Serial.println("Blynk failed, maintaning 18 degrees");    
  }  
  rtc.begin();
  httpUpdater.setup(&httpServer);
  httpServer.begin();  
  sensors.begin();          //thermometer DS18B20, resolution 11 = precision of 0.125C
  sensors.setResolution(11);    
  sensors.requestTemperatures(); 
  measurement = sensors.getTempCByIndex(0);
  if (measurement > 1 && measurement < 50) {  // ignore incorrect temperature measurements
    celsius = measurement;
  }     
  digitalWrite(D4, HIGH);   //turn off the LEDs if Blynk connects
  digitalWrite(D5, LOW);
  timer.setInterval(61000L, activetoday);     // check every minute if schedule should run today
  timer.setInterval(29000L, reconnectBlynk);  // check every 30s if still connected to server
  timer.setInterval(369000L, refreshsignal);  // refresh last transmitted signal every 6 minutes
  httpServer.on("/", handleRoot);
}

void activetoday() {  // check if schedule should run today
  sprintf(Date, "%02d/%02d/%04d",  day(), month(), year());
  sprintf(Time, "%02d:%02d:%02d", hour(), minute(), second());
  if (year() != 1970) {
    Blynk.syncAll();
    // Blynk.syncVirtual(V1); // sync timeinput widget - weekend
    // Blynk.syncVirtual(V2); // sync timeinput widget - weekday morning
    // Blynk.syncVirtual(V3); // sync timeinput widget - weekday evening
  }
  else{
      terminal.print("Invalid time ");
      terminal.print(Time);
      terminal.print(" and date ");
      terminal.println(Date);
      terminal.flush();      
  }
}

BLYNK_WRITE(V5) {     // slider to set the active target temp
  zaptepl = param.asInt();
}
BLYNK_WRITE(V6) {     //slider to set the inactive target temp
  vyptepl = param.asInt();
}

BLYNK_WRITE(V1) {  //test whether it's weekend, if it is, set timer1 to 1

  TimeInputParam t(param);
  int dayadjustment = -1;

  if (weekday() == 1) {
    dayadjustment =  6; // needed for Sunday - in Time library is day 1 (Sunday) and Blynk is day 7
  }

  if (t.isWeekdaySelected((weekday() + dayadjustment))) { //Time library starts week on Sunday, Blynk on Monday

    nowseconds = ((hour() * 3600) + (minute() * 60) + second());
    startseconds = (t.getStartHour() * 3600) + (t.getStartMinute() * 60);
    stopseconds  = (t.getStopHour() * 3600) + (t.getStopMinute() * 60);

    if (nowseconds >= startseconds && nowseconds <= stopseconds) {
        timer1 = 1;
      }
      else {
        timer1 = 0;
      }
    }
}


BLYNK_WRITE(V2) {   //test whether it's weekday morning, if it is, set timer2 to 1

  TimeInputParam t(param);
  int dayadjustment = -1;

  if (weekday() == 1) {
    dayadjustment =  6; // needed for Sunday, Time library is day 1 and Blynk is day 7
  }

  if (t.isWeekdaySelected((weekday() + dayadjustment))) { //Time library starts week on Sunday, Blynk on Monday

    nowseconds = ((hour() * 3600) + (minute() * 60) + second());
    startseconds = (t.getStartHour() * 3600) + (t.getStartMinute() * 60);
    stopseconds  = (t.getStopHour() * 3600) + (t.getStopMinute() * 60);

    if (nowseconds >= startseconds && nowseconds <= stopseconds) {
        timer2 = 1;
      }
      else {
        timer2 = 0;
      }
    }
}

BLYNK_WRITE(V3) {  //test whether it's weekday evening, if it is, set timer3 to 1

  TimeInputParam t(param);
  int dayadjustment = -1;

  if (weekday() == 1) {
    dayadjustment =  6; // needed for Sunday, Time library is day 1 and Blynk is day 7
  }

  if (t.isWeekdaySelected((weekday() + dayadjustment))) { //Time library starts week on Sunday, Blynk on Monday

    nowseconds = ((hour() * 3600) + (minute() * 60) + second());
    startseconds = (t.getStartHour() * 3600) + (t.getStartMinute() * 60);
    stopseconds = (t.getStopHour() * 3600) + (t.getStopMinute() * 60);

    if (nowseconds >= startseconds && nowseconds <= stopseconds) {
        timer3 = 1;
      }
      else {
        timer3 = 0;
      }
    }
}

BLYNK_WRITE(V12) {
  if(param.asInt()==1){
    norefresh = 1;     
  }
  else{
    norefresh = 0;    
  } 
}

BLYNK_WRITE(V11) {
  if(param.asInt()==1){
    mySwitch.send(zapni);
    Blynk.virtualWrite(V4, 255); 
    poslsignal = 1;
    terminal.print("heating turned on manually at ");
    terminal.print(Time);
    terminal.print(" on ");
    terminal.print(Date);
    terminal.print(", at temperature ");
    terminal.println(celsius);
    terminal.flush();       
    digitalWrite(D5, HIGH);
    Blynk.virtualWrite(V11, 0);    
    }
}     
 
void loop() {

  if (timer1 == 1 || timer2 == 1 || timer3 == 1) { //if one of the timers is active, turn the heating on
    ciltepl = zaptepl;  //what is the target active temperature
  }
  else {
    ciltepl = vyptepl;  //what is the target inactive temperature
  }

  if (Blynk.connected()) {
    if (celsius <= (ciltepl - 0.5) && poslsignal != 1) {
      mySwitch.send(zapni);
      poslsignal = 1;
      Blynk.virtualWrite(V4, 255);         //turn the virtual LED On
      digitalWrite(D4, HIGH);            // D4 = integrated LED, LOW = on, HIGH = off
      digitalWrite(D5, HIGH);
      terminal.print("heating turned on at ");
      terminal.print(Time);
      terminal.print(" on ");
      terminal.print(Date);
      terminal.print(", because temp ");
      terminal.print(celsius);
      terminal.print(" si lower than the target ");
      terminal.println(ciltepl);
      terminal.flush();          
    }

    if (celsius >= (ciltepl + 0.5) && poslsignal != 0) {
      mySwitch.send(vypni);
      poslsignal = 0;
      Blynk.virtualWrite(V4, 0);
      digitalWrite(D4, LOW);            // D4 = integrated LED, LOW = on, HIGH = off
      digitalWrite(D5, LOW);
    }
  }
  else if(poslsignal != 1 && celsius >= 1 && celsius <= 18.5){
    mySwitch.send(zapni);
    poslsignal = 1;
  }
  else if(poslsignal != 0 && celsius >= 21){
    mySwitch.send(vypni);
    poslsignal = 0;    
  }

  if (Blynk.connected()) {
    Blynk.run();
  }
  httpServer.handleClient();  
  timer.run();
}
