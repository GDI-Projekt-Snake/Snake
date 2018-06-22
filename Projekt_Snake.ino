#include <SoftwareSerial.h>
#include "DumbServer.h"

SoftwareSerial esp_serial(3, 2);
EspServer esp_server;

#include <LedControl.h>

/* Joystick PINs */
#define VRX     A0
#define VRY    A1
#define SW      2

/* Display PINs */
#define CLK     8
#define CS      9
#define DIN     10

/* Allgemeine Spielinfos */
#define GROESSE    8
#define SPIEL_GESCHWINDIGKEIT 20


int schlange[GROESSE*GROESSE][2];
int length;
int apfel[2], v[2];
bool is_game_over = false;
long aktueller_zeitpunkt;
long naechste_aktion;
int blink_zaehler;


/* Ansteuerung der Matrix */
LedControl lc = LedControl(DIN, CLK, CS, 1);

void init_spiel() {
  naechste_aktion = aktueller_zeitpunkt = 0;
  blink_zaehler = 3;
  int haelfte = GROESSE / 2;
  length = GROESSE / 3;

/*Festlegung der Startposition*/
  for (int i = 0; i < length; i++) {
      schlange[i][0] = haelfte - 1;
      schlange[i][1] = haelfte + i;
  }
/*Sartposition vom Apfel*/
  apfel[0] = haelfte + 1;
  apfel[1] = haelfte - 1;

  v[0] = 0;
  v[1] = -1;
}

/*Darstellung der LEDs auf der Matrix von der Schlange und von dem Apfel*/
void darstellung() {
  for (int i = 0; i < length; i++) {
    lc.setLed(0, schlange[i][0], schlange[i][1], 1);
  }
  lc.setLed(0, apfel[0], apfel[1], 1);
}

/*Wenn die Größe 8 auf der x- und y-Achse überschritten wird, bleiben die LEDs auf der letzten Position stehen*/
void neuerScreen() {
  for (int x = 0; x < GROESSE; x++) {
    for (int y = 0; y < GROESSE; y++) {
      lc.setLed(0, x, y, 0);
    }
  }
}

/* Die Schlange wird vorwärts bewegt und das Spiel wird beendet,
wenn der Spielfeldrand berührt wird. Es wird eine Sekunde gewartet und true wird zurückgegeben.*/
bool vorwaerts() {
  int kopf[2] = {schlange[0][0] + v[0], schlange[0][1] + v[1]};

  if (kopf[0] < 0 || kopf[0] >= GROESSE) {

            delay(1000);
      return true;
  }

  if (kopf[1] < 0 || kopf[1] >= GROESSE) {

            delay(1000);
      return true;
  }
/* Wenn die Schlange sich selbst berührt ist das Spiel beendet */
  for (int i = 0; i < length; i++) {
      if (schlange[i][0] == kopf[0] && schlange[i][1] == kopf[1]) {
            delay(1000);
          return true;
      }
  }
/*Wenn die Schlange auf einen Apfel trifft wird sie um einen größer*/
  bool wachsen = (kopf[0] == apfel[0] && kopf[1] == apfel[1]);
  if (wachsen) {
      length++;  
      randomSeed(aktueller_zeitpunkt);    
      apfel[0] = random(GROESSE);
      apfel[1] = random(GROESSE);
  }
/*Schlange bliebt in der Richtigen Länge zusammen und Länge wird angehängt*/
  for (int i = length - 1; i >= 0; i--) {
      schlange[i + 1][0] = schlange[i][0];
      schlange[i + 1][1] = schlange[i][1];
  }
  schlange[0][0] += v[0];
  schlange[0][1] += v[1];
  return false;
}

void setup() {
    Serial.begin(9600);
  esp_serial.begin(9600);
  Serial.println("Starting server...");
  esp_server.begin(&esp_serial, "Arduino", "password", 30303);
  Serial.println("...server is running");
    char ip[16];
  esp_server.my_ip(ip, 16);

  Serial.print("My ip: ");
  Serial.println(ip);
  
  pinMode(SW, INPUT_PULLUP);
  pinMode(VRX, INPUT);
  pinMode(VRY, INPUT);
  attachInterrupt(digitalPinToInterrupt(SW), neustart, RISING);

  lc.shutdown(0, false);
  lc.setIntensity(0, 8);

  init_spiel();
  darstellung();
}

void loop() {
  if (!is_game_over) {
    neuerScreen();
    darstellung();

/*Wenn die Geschwindigkeit sehr groß wird (sobald eine Wand oder die Schlange selbst berührt wird) 
  geht das Spiel in den blink_zaehler, also Game Over. Ansonsten läuft es normal weiter.*/
    if (aktueller_zeitpunkt - naechste_aktion > SPIEL_GESCHWINDIGKEIT) {
      is_game_over = vorwaerts();
      naechste_aktion = aktueller_zeitpunkt;    
    }
    /*Wenn das Spiel Game Over ist, blinkt die Schlange an der letzten Position drei mal*/
  } else {
    while (blink_zaehler > 0) {
      neuerScreen();
      delay(300);
      darstellung();
      delay(300);
      blink_zaehler--;     
         
    }
  }
  readControls();
  aktueller_zeitpunkt++;
  
  
}
/*Neustart beim drücken des Joysticks*/
void neustart() {  
  init_spiel();
  is_game_over = false;
}

/*Steuerung der Schlange mit dem Joystick*/
void readControls() {
  int dx = map(analogRead(VRX), 0, 906, 2, -2);
  int dy = map(analogRead(VRY), 0, 906, -2, 2);
  if (dx != 0) {dx = dx / abs(dx);}
  if (dy != 0) {dy = dy / abs(dy);}

  if (dy != 0 && v[0] != 0) {
    v[0] = 0;
    v[1] = dy;    
  }

  if (dx != 0 && v[1] != 0) {
    v[0] = dx;
    v[1] = 0;
  }
  /*Steurung über Python, welche über das Wlan-Shield empfangen wird*/
  if(esp_server.available()){
    String command=esp_server.readStringUntil('\n');

    if(command == "up"){
      v[0] = -1;
      v[1] = 0; 
      }
    else if(command == "down"){
      v[0] = 1;
      v[1] = 0; 
    }
    else if(command == "right"){
      v[0] = 0;
      v[1] = 1; 
    }
    else if(command == "left"){
      v[0] = 0;
      v[1] = -1; 
    }
  }

}
