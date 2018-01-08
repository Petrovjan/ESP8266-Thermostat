#define BLYNK_PRINT Serial
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>
#include <Time.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <Wire.h>
#include <RCSwitch.h>

char auth[] = "YourBlynkAuthCode123456abcdefgh";
char ssid[] =  "yourSSID";
char pass[] =  "yourPassword ";
char zapni[] = "111010001010101000";      //radio signal to turn the heating on
char vypni[] = "111010001010100000";      //radio signal to turn the heating off
char Date[16];
char Time[16];

long startseconds;
long stopseconds;
long nowseconds;

float celsius = 25;
int poslsignal = 2;
int timer1 = 0;               //weekend
int timer2 = 0;               //weekday morning
int timer3 = 0;               //weekday evening
int ciltepl;
int zaptepl;
int vyptepl;
int pouztepl;
SimpleTimer timer;
WidgetRTC rtc;
WidgetTerminal terminal(V10);
OneWire oneWire(D2);
DallasTemperature sensors(&oneWire);

RCSwitch mySwitch = RCSwitch();


void reconnectBlynk() {
  sensors.requestTemperatures();
  if (sensors.getTempCByIndex(0) > 1 && sensors.getTempCByIndex(0) < 50) {
    celsius = sensors.getTempCByIndex(0);
  }
  Blynk.virtualWrite(V7, celsius); //V7 = current temperature
  Blynk.virtualWrite(V17, ciltepl); //V17 = target temperature
  if (!Blynk.connected()) {
    digitalWrite(D5, LOW);
    digitalWrite(D4, HIGH);
    if (Blynk.connect()) {
      BLYNK_LOG("Reconnected");
    }
    else {
      BLYNK_LOG("Not reconnected");
      zaptepl = 18;     //NOVE
      vyptepl = 18;     //NOVE
      digitalWrite(D5, HIGH);
      digitalWrite(D4, LOW);
    }

  }
}

void refreshsignal() {
  if (poslsignal == 0) {
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

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(D4, OUTPUT);
  pinMode(D5, OUTPUT);
  digitalWrite(D5, HIGH);
  mySwitch.enableTransmit(D1);
  mySwitch.setRepeatTransmit(10);
  mySwitch.setProtocol(5);    //protocol 5 in the rcswitch.cpp
  Blynk.begin(auth, ssid, pass);
  while (Blynk.connect() == false) {    // Wait until connected
    if (millis() > 300000) {    //unless Blynk connects within 5 minutes, keep temp at 18 degrees
      vyptepl = 18;   
      zaptepl = 18;   
      break;
    }
  }
  ArduinoOTA.begin();
  rtc.begin();
  sensors.begin();          //DS18B20, resolution 11 = accuracy of 0.125C
  sensors.setResolution(11);
  sensors.requestTemperatures();
  if (sensors.getTempCByIndex(0) > 1 && sensors.getTempCByIndex(0) < 50) { //ignore temperatures way off the expected values
    celsius = sensors.getTempCByIndex(0);
  }
  if (Blynk.connected()) {
    digitalWrite(D4, HIGH);   //turn LEDs off if Blynk connects
    digitalWrite(D5, LOW);
  }
  timer.setInterval(61000L, activetoday);     // check every minute if schedule should run today
  timer.setInterval(29000L, reconnectBlynk);  // check every 30s if still connected to server, also get temperature
  timer.setInterval(123000L, refreshsignal);  // refresh last transmitted signal every 2 minutes (the numbers are slightly altered to reduce the number of collisions)
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
  else {
    digitalWrite(D4, LOW);
    digitalWrite(D5, HIGH);
  }
}

BLYNK_WRITE(V5) {     // slider of "active" target temperature
  zaptepl = param.asInt();
}
BLYNK_WRITE(V6) {     //slider of "inactive" target temperatrure
  vyptepl = param.asInt();
}

BLYNK_WRITE(V1) {  //test for weekend daytime, if so set timer1 to 1

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


BLYNK_WRITE(V2) {   //test for weekday morning, if so set timer2 to 1

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

BLYNK_WRITE(V3) {  //test for weekday evening, if so set timer3 to 1

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


void loop() {

  if (timer1 == 1 || timer2 == 1 || timer3 == 1) { //if one of the timers is set to 1, use active target temperature = zaptepl
    ciltepl = zaptepl;
  }
  else {
    ciltepl = vyptepl;  //inactive time temperature, so that it doesn't get too cold
  }

  if (celsius <= (ciltepl - 0.5) && poslsignal != 1) {
    mySwitch.send(zapni);
    poslsignal = 1;
    Blynk.virtualWrite(V4, 255);         //turn on virtual LED
    digitalWrite(D4, HIGH);              // turn on physical red LED, and turn off built-in blue LED (it's off when HIGH)
    digitalWrite(D5, HIGH);
    terminal.print("heating turned on at "); //Blynk terminal messages to monitor strange behaviour
    terminal.print(Time);
    terminal.print(" on ");
    terminal.print(Date);
    terminal.print(", due to temperature ");
    terminal.print(celsius);
    terminal.print(" lower than the target ");
    terminal.println(ciltepl);
    terminal.flush();
  }

  if (celsius >= (ciltepl + 0.5) && poslsignal != 0) {
    mySwitch.send(vypni);
    poslsignal = 0;
    Blynk.virtualWrite(V4, 0);
    digitalWrite(D4, LOW);            // turn on built-in blue LED, turn off red LED
    digitalWrite(D5, LOW);
  }

  if (Blynk.connected()) {
    Blynk.run();
    ArduinoOTA.handle();

  }

  timer.run();
}
