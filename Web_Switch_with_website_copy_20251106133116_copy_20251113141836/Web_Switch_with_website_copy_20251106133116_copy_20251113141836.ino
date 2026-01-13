/************************************************************
**  Web-Switch 6fach                                       **
**  Arduino MKR ZERO                                       **
**   SAMD21 Cortex-M0+ 32bit low power ARM                 **
**   DI/O 22 (PWM12)                                       **
**   AI 7 (ADC 8/10/12 bit)                                **
**   AO 1 (DAC 10bit)                                      **
**   UART, SPI, I2C                                        **
**   Flash 256KB                                           **
**   SRAM	32KB                                             **
**  Arduino MKS ETH Shield                                 **
**  L. Gehrig 18.11.2025                                   */
char Version[] = "V0.1";  // Anzeige SW-Version auf Webseite
/************************************************************



Versionen:
----------
V0.1            Prototype-Software


Hardware:
---------

Anschlüsse Taster:
---------------------------------------------
Arduino Pin:  Funktion:                 Typ:
15            Taster T1                 D in
16            Taster T2                 D in
17            Taster T3                 D in
18            Taster T4                 D in
19            Taster T5                 D in
20            Taster T6                 D in

Anschlüsse Relais:
---------------------------------------------
Arduino Pin:  Funktion:                 Typ:
0             Relais Z1                 D out
1             Relais Z2                 D out
2             Relais Z3                 D out
3             Relais Z4                 D out
6             Relais Z5                 D out
7             Relais Z6                 D out



*********************************************************
*********************************************************
********************************************************/

/************************************************************
** Debug-Einstellung                                       
************************************************************/
#define FLASH_DEBUG 0


/*******************************************************
* Librarys
*******************************************************/
#include <SPI.h>                 // SPI-Schnittstelle für Ethernet
#include <Ethernet.h>            // Ethernet Stack für W5x00-Chips
#include <FlashAsEEPROM_SAMD.h>  // EEPROM-Emulation für SAMD21

/*******************************************************
* Variabeln, Definitionen, Klassen
*******************************************************/
#define ETH_CS 5 // Chip-Select Pin des Ethernet-Controllers
#define EEPROM_EMULATION_SIZE 4096
class Relay{
  public:
    bool state[6]; // Status der sechs Relais (true = ein / false = aus)
    static inline constexpr  int pin[6]  = { 0, 1, 2, 3, 6, 7 };  // Zuordnung der digitalen Pins zu den Relais
    String tglex; // Hilfsstring zum Erkennen von "toggle-relayX"
    void power();
    void toggle(EthernetClient &client, int i);
} relay;

class Button{
  public:
    static inline constexpr  int pin[6] = { 15, 16, 17, 18, 19, 20 }; // Zuordnung der digitalen Pins zu den Tastern
    int pressed[6];  // Entprell-Status der Taster (0 = nicht gedrückt, 1 = gedrückt)
    void detect(Relay& relay);
} button;

// Standard-Netzwerkparameter
class IPS {
  public:
    byte ip[4];
    byte gateway[4];
    byte subnet[4];
    IPS() {}
    IPS(byte i[4], byte g[4], byte s[4] ) {
      memcpy(ip, i, 4);
      memcpy(gateway, g, 4);
      memcpy(subnet, s, 4);
    }
};
IPS standart((byte[4]){192, 168, 1, 177}, (byte[4]){192, 168, 1, 1}, (byte[4]){255, 255, 255, 0});
IPS temp; // Temporäre Speicher für neue Netzwerkkonfigurationen
byte mac[] = { 0x00, 0x04, 0xA3, 0x00, 0x00, 0x06 };  // MAC-Adresse des Ethernet-Shields

class Cfg {
  public:
    void change(EthernetClient &client, int i);
    void updte(EthernetClient &client, IPS &temp, byte mac[6]);
    int reset();
} config;

// Interne Hilfsvariablen
int chnge = 0;
char c;
String cfgexp[2] = { "ip", "subnet" };  // Strings für Konfigurationserkennung im HTTP-POST
byte first = 1;                                       // Flag für „erste Initialisierung“
int resetq = 0;                                       // Variablen für Reset-Erkennung
String readString;                                    // Puffer für eingehende HTTP-Anfragen

/************************************************************
** Ethernet-Server Instanz auf Port 80                      
************************************************************/
EthernetServer server(80);

/*******************************************************
* Prototypen
*******************************************************/
void printWEB();
void sendStatus(EthernetClient &client);

/*******************************************************
* Setup — läuft einmal beim Start 
*******************************************************/
void setup() {
  SerialUSB.begin(115200); // Start der seriellen Schnittstelle
  //while (!SerialUSB);
  delay(1000);
  SerialUSB.println("SerialUSB begin");
  // Setzen aller Relay- und Taster-Pins
  for (int i = 0; i < 6; i++) {
    pinMode(relay.pin[i], OUTPUT);
    pinMode(button.pin[i], INPUT);
  }
  delay(1000);
  //SerialUSB.println("pins assigned");
  //SerialUSB.println(EEPROM.length());  // Prüfen der EEPROM-Länge
  byte initFlag = EEPROM.read(63);
  byte val = EEPROM.read(63);
  int elngth = (EEPROM.length() - 1);
  //uint8_t val = 256; //EEPROM.read(elngth);
  if (digitalRead(button.pin[0]) == HIGH && digitalRead(button.pin[1]) == HIGH) { // Prüfen ob beim Start Taster 1 UND 2 gedrückt werden → Reset vorbereiten
    SerialUSB.print("reset");
    resetq = 1;
  }  
  for(int i = 0; i < 4; i++){
      SerialUSB.println(standart.ip[i]);
  }
  if (val == 0xFF || config.reset() == 1) {   // Prüfen ob EEPROM leer oder Reset verlangt
    SerialUSB.println("first");
    int eA = 0; // EEPROM-Addresspointer
    EEPROM.put(eA, standart.ip); // EEPROM-Addresspointer
    eA += sizeof(standart.ip);
    EEPROM.put(eA, standart.subnet); // Subnet speichern
    eA += sizeof(standart.subnet);
    EEPROM.put(eA, standart.gateway); // Gateway speichern
    eA += sizeof(standart.gateway);
    EEPROM.commit(); // Änderungen übernehmen
    delay(500);
    EEPROM.write(63, first); // „initialisiert“-Marker schreiben
    EEPROM.commit();
  }
  if (1 > 0) {  // Aktuelle Konfiguration aus EEPROM lesen
    int eA = 0;
    EEPROM.get(eA, standart.ip);
    for(int i = 0; i < 4; i++){
      SerialUSB.println(standart.ip[i]);
    }
    eA += sizeof(standart.ip);
    EEPROM.get(eA, standart.subnet);
    //SerialUSB.println(standart.subnet);
    eA += sizeof(standart.subnet);
    EEPROM.get(eA, standart.gateway);
    //SerialUSB.println(standart.gateway);
    eA += sizeof(standart.gateway);
  }
  delay(500);
  // Ethernet starten
  Ethernet.begin(mac, standart.ip, standart.gateway, standart.subnet); 
  server.begin();
  // Link-Status ausgeben
  if (Ethernet.linkStatus() == Unknown) {
    SerialUSB.println("Link status unknown. Link status detection is only available with W5200 and W5500.");
  }

  else if (Ethernet.linkStatus() == LinkON) {
    SerialUSB.println("Link status: On");
  }

  else if (Ethernet.linkStatus() == LinkOFF) {
    SerialUSB.println("Link status: Off");
  }
  relay.tglex = "relay0";
}

/*******************************************************
*
* Hauptprogramm
*
*******************************************************/

/************************************************************
** LOOP — Hauptprogramm, läuft dauerhaft                 
************************************************************/
void loop() {
  EthernetClient client = server.available(); // Überprüft, ob ein neuer Ethernet-Client (Browser) verbunden wurde.
  if (client) {
    //
    // Ein Client ist verbunden – ab hier beginnt die Verarbeitung
    // des HTTP-Requests.
    //
    String currentLine = ""; // Zwischenspeicher für die gerade eingelesene Zeile
    while (client.connected()) {
      
      // Prüfen, ob Daten vom Client empfangen wurden
      if (client.available()) {
        boolean currentLineIsBlank = true;
        c = client.read();  // Einzelnes Zeichen aus dem HTTP-Puffer lesen
        if (readString.length() < 1000) { // Schutz vor Speicherüberlauf (max. 1000 Zeichen)
          readString += c;   // kompletten HTTP-Request sammeln
          currentLine += c;  // aktuelle Zeile sammeln
        }
        // Wurde eine neue Zeile empfangen?
        if (c == '\n' && currentLineIsBlank) { 
          //
          // LEERE ZEILE SIGNALISIERT:
          // → Header des HTTP-Requests ist zu Ende
          // → Jetzt kann die Anfrage ausgewertet werden
          //
          SerialUSB.println(readString);
          if (readString.indexOf("toggle") >= 0) { // Prüfen ob der Request einen Toggle-Befehl enthält
            for (int i = 0; i < 6; i++) {  // Für alle Relais prüfen, welches angesprochen wurde
              relay.tglex[5] = i + '0'; // erzeugt z.B. "relay0", "relay1" ...
              if (readString.indexOf(relay.tglex) >= 0) {
                SerialUSB.println("toggleBtn");
                relay.toggle(client, i); // Relais umschalten
                readString = "";
                break;
              }
            }
          } else if (readString.indexOf("POST") >= 0) { // Prüfen ob ein POST für Netzwerkkonfiguration
            for (int i = 0; i < 2; i++) {
              if (readString.indexOf(cfgexp[i]) >= 0) {
                SerialUSB.print("changeCfg ");
                SerialUSB.println(cfgexp[i]);
                config.change(client, i); // neue Parameter interpretieren
                break;
              }
            }
          } else if (readString.indexOf("status") >= 0) { // Status-Anfrage (AJAX /status)
            sendStatus(client);
          } else { // Wenn nichts davon passt → Webseite ausgeben
            SerialUSB.println("printWEB");
            readString = "";
            printWEB(client, Version);
            break;
          }
          readString = "";
        }
        if (c == '\n') { // Prüfen ob Zeilenende erreicht
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
      button.detect(relay); // *** Taster zyklisch prüfen ***
    }
    delay(1);
    client.stop(); // Verbindung trennen
  }
  button.detect(relay); // Auch ohne Client Buttons überwachen
}
/*******************************************************
* Hauptprogramm Ende
*******************************************************/

/*******************************************************
* Funktionen
*******************************************************/

/************************************************************
** Tastererkennung + Entprellung                         
************************************************************/
void Button::detect(Relay& relay) {
  for (int i = 0; i < 6; i++) {
    if (digitalRead(button.pin[i]) == HIGH && button.pressed[i] == 0) { // Taster wurde gedrückt (HIGH) UND war vorher nicht gedrückt?
      delay(30); // einfache Entprellung
      if (relay.state[i] == 0) { // Wenn das Relais aus war → einschalten
        relay.state[i] = 1;
      } else {
        relay.state[i] = 0;
      }
      button.pressed[i] = 1; // Merken: Taster gilt als gedrückt
    } else if (digitalRead(button.pin[i]) == LOW && button.pressed[i] == 1) { // Wenn losgelassen → Reset der gedrückten Markierung
      button.pressed[i] = 0;
    }
  }
  relay.power(); // Nach jeder Änderung Relais physisch schalten
}

/************************************************************
** Überträgt relaystate[] auf die tatsächlichen Pins      
************************************************************/
void Relay::power() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(relay.pin[i], relay.state[i]);
  }
}

/************************************************************
** Umschalten eines einzelnen Relais per Web             
************************************************************/
void Relay::toggle(EthernetClient &client, int i) {
  relay.state[i] = !relay.state[i];  // Zustand invertieren
  relay.power();                    // Hardware aktualisieren
}

/************************************************************
** printWEB()
**
** Gibt die komplette HTML-Webseite an den Client aus.
** Diese Funktion erzeugt:
**  - HTML Grundgerüst
**  - CSS Styling
**  - Buttons zum Umschalten der Relais
**  - LED-Anzeige für jeden Relay-Zustand
**  - Settings-Popup (IP/Subnet)
**  - JavaScript-Logik für AJAX, Buttons, Popup
************************************************************/
void printWEB(EthernetClient &client, char Version[5]) {
  // ----------------------------
  // HTTP-Header
  // ----------------------------
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  // HTML seite
  client.println("<!DOCTYPE html>");
  client.println("<html>");
  client.println("    <head>");
  client.println("        <meta charset='utf-8'>");
  client.println("        <title>Web-Switch</title>");
  // ----------------------------------------------------
  // CSS — Styling der gesamten Oberfläche
  // ----------------------------------------------------
  client.println("        <style>");
  client.println("            body {"); // Körper der Seite
  client.println("              background-color: white");
  client.println("              color:white;");
  client.println("              font-family:'Segoe UI', Arial, sans-serif;");
  client.println("              text-align:center;");
  client.println("              margin:0;");
  client.println("              padding:20px;");
  client.println("            }");
  client.println("            h1, h2, h3 {"); // Überschriften
  client.println("              color: #FF0000; /* example: light blue-gray for headings */");
  client.println("            }");
  client.println("");
  client.println("            p, label {"); // Text
  client.println("              color: black; /* example: white for paragraphs and labels */");
  client.println("            }");
  client.println("            .led {"); // Stylische LED-Anzeige
  client.println("              display:inline-block;width:20px;height:20px;border-radius:50%;");
  client.println("              background:#555;box-shadow:inset 0 1px 2px rgba(255,255,255,0.4),0 1px 3px rgba(0,0,0,0.5);");
  client.println("              position:relative;margin-right:10px;vertical-align:middle;transition:all .2s;");
  client.println("            }");
  client.println("            .led.on {"); // LED = EIN
  client.println("              background:radial-gradient(circle at 30% 30%, #00c0ff, #007bb5);");
  client.println("              box-shadow:inset 0 1px 2px rgba(255,255,255,0.4),");
  client.println("                         0 0 8px rgba(0,192,255,0.8),");
  client.println("                         0 2px 4px rgba(0,0,0,0.5);");
  client.println("            }");
  client.println("            .led.off { background-color: gray; }"); // LED = AUS
  client.println("");
  client.println("            button {"); // Buttons (Relais)
  client.println("              font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;");
  client.println("              font-size:13px;");
  client.println("              color:#000;");
  client.println("              padding:2px 28px;");
  client.println("              margin:5px;");
  client.println("              min-width:90px;");
  client.println("              height:26px;");
  client.println("              border:1px solid #6d6d6d;");
  client.println("              border-radius:3px;");
  client.println("              background:linear-gradient(to bottom,#ffffff 0%,#dcdcdc 100%);");
  client.println("              box-shadow:inset 0 1px 0 rgba(255,255,255,0.7),0 1px 2px rgba(0,0,0,0.25);");
  client.println("              cursor:pointer;");
  client.println("              transition:all 0.15s ease-in-out;");
  client.println("            }");
  client.println("            button:hover {");
  client.println("              background:linear-gradient(to bottom,#f2f9ff 0%,#d4e7ff 100%);");
  client.println("              border-color:#3a7bd5;");
  client.println("            }");
  client.println("			      button:active {");
  client.println("              background:linear-gradient(to bottom,#d4e7ff 0%,#f2f9ff 100%);");
  client.println("              box-shadow:inset 0 2px 4px rgba(0,0,0,0.3);");
  client.println("            }");
  client.println("");
  client.println("            #overlay {"); // Overlay für Einstellungen
  client.println("              display:none;position:fixed;inset:0;");
  client.println("              background:rgba(0,0,0,0.55);z-index:999;");
  client.println("           }");
  client.println("");
  client.println("            #configPopup {");  // Popup Einstellungen → Aero Glass Effekt
  client.println("              display:none;position:fixed;top:50%;left:50%;");
  client.println("              transform:translate(-50%,-50%);");
  client.println("              width:380px;border-radius:10px;overflow:hidden;z-index:1000;");
  client.println("              background:rgba(255,255,255,0.005);");
  client.println("              border:1px solid rgba(255,255,255,0.15);");
  client.println("              box-shadow:0 0 30px rgba(0,0,0,0.7);");
  client.println("              backdrop-filter:blur(40px) saturate(180%) brightness(140%);");
  client.println("              -webkit-backdrop-filter:blur(40px) saturate(180%) brightness(140%);");
  client.println("            }");
  client.println("            #configPopup::before {");
  client.println("              content:'';display:block;height:40px;");
  client.println("              background:linear-gradient(to bottom,rgba(255,255,255,0.25) 0%,rgba(160,220,255,0.08) 100%);");
  client.println("              border-bottom:1px solid rgba(255,255,255,0.15);");
  client.println("              box-shadow:inset 0 1px 0 rgba(255,255,255,0.6),0 1px 10px rgba(255,255,255,0.4);");
  client.println("            }");
  client.println("            #configPopup h2 {");
  client.println("              position:absolute;top:8px;left:0;right:0;");
  client.println("              font-size:16px;color:#fff;");
  client.println("              text-shadow:0 0 10px rgba(255,255,255,0.8);");
  client.println("              pointer-events:none;");
  client.println("            }");
  client.println("");
  client.println("            #configContent {");  // Content im Popup
  client.println("              padding:10px 20px 15px 20px;");
  client.println("              margin-top:0; ");
  client.println("              text-align:left;");
  client.println("            }");
  client.println("            #configPopup label {");
  client.println("              display:block;");
  client.println("              margin:8px 0 4px;");
  client.println("              color:#fff;");
  client.println("              text-shadow:0 0 6px rgba(255,255,255,0.5);");
  client.println("            }");
  client.println("            #configPopup input {"); // Eingabefelder Styling
  client.println("              width:100%;");
  client.println("              padding:5px;");
  client.println("              border:none;");
  client.println("              border-radius:4px;");
  client.println("              background:rgba(255,255,255,0.9);");
  client.println("              color:#000;");
  client.println("              font-size:14px;");
  client.println("              box-shadow:inset 0 1px 3px rgba(0,0,0,0.3);");
  client.println("            }");
  client.println("            #configPopup input:focus {outline:2px solid #64a0f2;}");
  client.println("            ");
  client.println("            #applyBtn:disabled {");
  client.println("              opacity: 0.6; /* increase from default ~0.4 */");
  client.println("              cursor: not-allowed;");
  client.println("              box-shadow: none;");
  client.println("              background: linear-gradient(to bottom, #d6d6d6 0%, #bfbfbf 50%, #e4e4e4 100%);");
  client.println("              border-color: #999;");
  client.println("              color: #333;");
  client.println("            }");
  client.println("            ");
  client.println("            .configContent {");
  client.println("              display: flex;");
  client.println("              flex-direction: column;");
  client.println("              align-items: flex-start; /* everything left-aligned */");
  client.println("              gap: 8px;");
  client.println("              padding: 12px;");
  client.println("            }");
  client.println("");
  client.println("            .configContent label {");
  client.println("              display: block;");
  client.println("            }");
  client.println("");
  client.println("            .configContent input {");
  client.println("              width: 100%;");
  client.println("              padding: 4px 6px;");
  client.println("              border-radius: 4px;");
  client.println("              border: 1px solid rgba(255,255,255,0.3);");
  client.println("              background: rgba(255,255,255,0.05);");
  client.println("              color: white;");
  client.println("            }");
  client.println("");
  client.println("");
  client.println("            #buttonBar {"); // Buttonbereich im Popup
  client.println("              display:flex;");
  client.println("              justify-content:flex-end;");
  client.println("             padding:10px 20px 15px;");
  client.println("              gap:8px;");
  client.println("              border-top:1px solid rgba(255,255,255,0.15);");
  client.println("              background:linear-gradient(to top, rgba(255,255,255,0.06), rgba(255,255,255,0.01));");
  client.println("            }");
  client.println("");
  client.println("            #header {"); // Header oben auf der Seite
  client.println("              display: flex;");
  client.println("              justify-content: space-between;");
  client.println("              align-items: center;");
  client.println("              margin-bottom: 10px;");
  client.println("              padding: 5px 10px;");
  client.println("              background: rgba(0,0,0,0.5); /* semi-transparent tilebar effect */");
  client.println("              border-bottom: 1px solid rgba(255,255,255,0.2);");
  client.println("            }");
  client.println("");
  client.println("            #leftInfo {");
  client.println("              display: flex;");
  client.println("              align-items: center;");
  client.println("              gap: 10px; /* space between logo and network info */");
  client.println("            }");
  client.println("");
  client.println("            #logo {");
  client.println("              height: 32px;");
  client.println("            }");
  client.println("");
  client.println("            #networkInfo {");
  client.println("              display: flex;");
  client.println("              gap: 10px;");
  client.println("              color: #D0D0D0;");
  client.println("              font-size: 18px;");
  client.println("              white-space: nowrap; /* prevent wrapping */");
  client.println("              margin-top: 5%;");
  client.println("              flex-direction: column; ");
  client.println("              align-items: flex-start; ");
  client.println("              gap: 6px; ");
  client.println("            }");
  client.println("");
  client.println("            #headerCenter {");
  client.println("              flex-grow: 1;");
  client.println("              text-align: center;");
  client.println("            }");
  client.println("");
  client.println("            #headerText {");
  client.println("              font-size: 18px;");
  client.println("              font-weight: bold;");
  client.println("              color: white;");
  client.println("            }");
  client.println("");
  client.println("            #settingsBtn {");
  client.println("              margin: 0;");
  client.println("            }");
  client.println("            #mainContent {");
  client.println("              margin-top: 10%; /* adjust value to push content down */");
  client.println("            }");
  client.println("        </style>");
  client.println("    </head>");
  // ---------------------------------------------------------
  // HTML-Body
  // ---------------------------------------------------------
  client.println("    <body>");
  // ---------------------------------------------------------
  // HEADERBEREICH MIT TITEL & EINSTELLUNGSBUTTON
  // ---------------------------------------------------------
  client.println("        <div id='header'>");
  client.println("          ");
  client.println("          <span id='headerText'>Web-Switch Walker Josef Altdorf</span>");
  client.println("          <button id='settingsBtn'>⚙️ Settings</button>");
  client.println("        </div>");
  client.println("");
  client.println("        <h1> </h1>"); // Abstand
  client.println("        <h1> </h1>");
  client.println("        <h1> </h1>");
  client.println("        <div id='mainContent'>"); //hauptinhalt
  // ---------------------------------------------------------
  // RELAIS–BUTTONS + LED's
  // ---------------------------------------------------------
  client.println("<div class='relay'>"); // 6 Relais + 6 LEDs generieren
    for (int i = 0; i < 6; i++) {
      client.print("<button id='Button");
      client.print(i);
      client.print("'>Steckdose ");
      client.print(i + 1);
      client.println("</button>");
      client.print("<div id='led");
      client.print(i);
      client.println("' class='led off'></div>");
    }
  client.println("</div>");
  client.println("");
  // ---------------------------------------------------------
  // NETZWERKINFORMATIONEN (Version, IP, MAC, Subnet)
  // ---------------------------------------------------------
  client.println("            <div id='leftInfo'>");
  client.println("                <div id='networkInfo' style='display: flex; flex-direction: column; align-items: flex-start; gap: 4px;'>");
  client.print("                    <p style='margin:0;'>Version: ");
  client.print(Version);
  client.println("</p>");
  client.println("                    <p id='ipInfo' style='margin:0;'>IP address: [replace]</p>");
  client.println("                    <p id='subnetInfo' style='margin:0;'>Subnet: [replace]</p>");
  client.println("                    <p id='macInfo' style='margin:0;'>MAC: [replace]</p>");
  client.println("                </div>");
  client.println("            </div>");
  client.println("");
  // ---------------------------------------------------------
  // SETTINGS POPUP — wird per JavaScript geöffnet
  // ---------------------------------------------------------
  client.println("            <!-- Overlay & Popup -->");
  client.println("            <div id='overlay'></div>");
  client.println("            <div id='configPopup'>");
  client.println("              <div id='configContent'>");
  client.println("                <h2>Network Configuration</h2>");
  client.println("                <label for='ip'>IP Address:</label>");
  client.println("                <input type='text' id='ip' value=''>");
  client.println("                <label for='subnet'>Subnet Mask:</label>");
  client.println("                <input type='text' id='subnet' value=''>");
  client.println("              </div>");
  client.println("              <div id='buttonBar'>");
  client.println("                <button id='okBtn'>OK</button>");
  client.println("                <button id='cancelBtn'>Cancel</button>");
  client.println("                <button id='applyBtn'>Apply</button>");
  client.println("              </div>");
  client.println("            </div>");
  client.println("        </div>");
  client.println("");
  // ---------------------------------------------------------
  // JAVASCRIPT — Buttons, AJAX, Popup
  // ---------------------------------------------------------
  client.println("        <script>");
  // -------------------------------------------------------
  // Beim Seitenstart: aktuelle Relais-Zustände einbetten
  // -------------------------------------------------------
  client.println("window.onload = function(){");
    client.println("            const relayStates=[");
    for (int i = 0; i < 6; i++) {
      client.print(relay.state[i]);
      if (i < 5) client.print(",");
    }
  client.println("];");
  client.println("            const buttons=[...Array(6).keys()].map(i=>document.getElementById('Button' + i));"); // Buttons erfassen
  client.println("            const busy=[false,false,false,false,false,false];"); // Busy-Flags (um mehrfachklick zu verhindern)
  client.println("            relayStates.forEach((s,i)=>document.getElementById('led' + i).classList.toggle('on',s));");
  client.println("            buttons.forEach((b,i)=>{");
  client.println("              b.addEventListener('pointerdown',e=>{");
  client.println("                e.preventDefault(); if(busy[i])return; busy[i]=true;");
  client.println("                fetch(`/toggle-relay${i}`,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay:i})})");
  client.println("                  .catch(console.error)");
  client.println("                  .finally(()=>setTimeout(()=>busy[i]=false,200));");
  client.println("              });");
  client.println("            });");
  // ---------------------------------------------------------
  // AJAX: Periodisch Status abfragen
  // ---------------------------------------------------------
  client.println("            function updateData(){");
  client.println("              fetch('/status').then(r=>r.json()).then(status=>{");
  client.println("              for(let i=0;i<status.relayStates.length;i++){");
  client.println(" let led=document.getElementById('led'+i);");
  client.println("                if(status.relayStates[i]){led.classList.add('on');led.classList.remove('off');}");
  client.println("                else {led.classList.add('off');led.classList.remove('on');}");
  client.println("               }});");
  client.println("            }");
  client.println("            setInterval(updateData, 1000);");
  client.println("            updateData();");
  client.println("");
  client.println("            const overlay=document.getElementById('overlay');");
  client.println("            const popup=document.getElementById('configPopup');");
  client.println("            const okBtn=document.getElementById('okBtn');");
  client.println("            const cancelBtn=document.getElementById('cancelBtn');");
  client.println("            const applyBtn=document.getElementById('applyBtn');");
  client.println("");
  client.println("            const ip=document.getElementById('ip');");
  client.println("            const subnet=document.getElementById('subnet');");
  client.println("");
  client.println("            let initialConfig={ip:'', mac:'', subnet:''};");
  client.println("            let currentConfig = {"); // Aktuelle Werte einbetten
    client.print(R"(              ip: ")");
    for (int i = 0; i < 4; i++) {
      client.print(standart.ip[i]);
      if (i < 3) client.print(".");
    }
    client.println(R"(",)");
    client.print(R"(mac: ")");
    for (int i = 0; i < 6; i++) {
      if (mac[i] <= 16) { client.print(0); }
      client.print(mac[i], HEX);
      if (i < 5) client.print("-");
    }
    client.println(R"(",)");
    client.print(R"(subnet: ")");
    for (int i = 0; i < 4; i++) {
      client.print(standart.subnet[i]);
      if (i < 3) client.print(".");
    }
    client.println(R"("};)");
  client.println("");
  // ---------------------------------------------------------
  // Settings Popup Logik
  // ---------------------------------------------------------
  client.println("            function openPopup() {"); // Öffnen des Popups
  client.println("              overlay.style.display = 'block';");
  client.println("              popup.style.display = 'block';");
  client.println("");
  client.println("              // Fill input fields with current values");
  client.println("              ip.value = currentConfig.ip;");
  client.println("              subnet.value = currentConfig.subnet;");
  client.println("");
  client.println("              // Store initial config for Apply button tracking");
  client.println("              initialConfig = {...currentConfig};");
  client.println("              applyBtn.disabled = true;");
  client.println("            }");
  client.println("");
  // ---------------------------------------------------------
  // Eventhandler für Button-Klicks → sendet POST /toggle-relayX
  // ---------------------------------------------------------
  client.println("            function checkChanges() {");
  client.println("              const dirty = ip.value !== initialConfig.ip ||");
  client.println("                            subnet.value !== initialConfig.subnet;");
  client.println("              applyBtn.disabled = !dirty;");
  client.println("            }");
  client.println("");
  client.println("            function saveConfig() {");
  client.println("              applyBtn.disabled = true;");
  client.println("              const cfg = {");
  client.println("                ip: ip.value,");
  client.println("                subnet: subnet.value");
  client.println("              };");
  client.println("              currentConfig = {");
  client.println("                ip: ip.value,");
  client.println("                subnet: subnet.value");
  client.println("              };");
  client.println("              fetch(`/ip=${currentConfig.ip}`, {method:'POST'})");
  client.println("                  .catch(console.error);");
  client.println("              fetch(`/subnet=${currentConfig.subnet}`, {method:'POST'})");
  client.println("                  .catch(console.error);");
  client.println("            }");
  client.println("");
  client.println("            // Event listeners");
  client.println("            document.getElementById('settingsBtn').onclick = openPopup;");
  client.println("			      cancelBtn.onclick = ()=>{overlay.style.display='none';popup.style.display='none';};"); // Cancel
  client.println("            okBtn.onclick = ()=>{"); // OK: Speichern und Seite neu laden
  client.println("              saveConfig();");
  client.println("              window.location.href = `http://${currentConfig.ip}`;");
  client.println("              overlay.style.display='none';");
  client.println("              popup.style.display='none';");
  client.println("            };");
  client.println("            applyBtn.onclick = saveConfig;");
  client.println("");
  client.println("            // Track changes in inputs");
  client.println("            [ip, subnet].forEach(input=>{");
  client.println("              input.addEventListener('input', checkChanges);");
  client.println("            });");
  client.println("");
  client.println("");
  client.println("            document.getElementById('ipInfo').textContent = `IP Address: ${currentConfig.ip}`;"); // Anzeigen der Netzwerkdaten
  client.println("            document.getElementById('macInfo').textContent = `MAC: ${currentConfig.mac}`;");
  client.println("            document.getElementById('subnetInfo').textContent = `Subnet: ${currentConfig.subnet}`;");
  client.println("};");
  client.println("        </script>");
  client.println("    </body>");
  client.println("</html>");
}


/************************************************************
** changeCfg()
**
** Wird aufgerufen, wenn per POST ein neuer Netzwerkparameter
** gesendet wird (ip=... oder subnet=...).
**
** Aufgabe:
**  - Auslesen der gesendeten Zeichenkette
**  - Umwandeln in ein IPAddress Objekt
**  - Aufruf von updtCfg() zur Speicherung im EEPROM
************************************************************/
void Cfg::change(EthernetClient &client, int i) {
  //
  // i = 0 → IP-Adresse ändern
  // i = 1 → Subnet ändern
  //
  int cl = 4;
  int clw = 0;
  int cache[cl] = { 0, 0, 0, 0 };
  int y = readString.indexOf("=") + 1; // Den relevanten Teil des Strings auslesen, z.B. "ip=192.168.1.10"
  int length = readString.indexOf(' ', y - 1) + 1;
  for (y; y < length; y++) { 
    if (readString[y] == '.' || readString[y] == ' ') { // Neue IP zusammenbauen
      if (i == 0) {
        temp.ip[clw] = (byte)cache[clw];
        //SerialUSB.println(temp.ip);
      } else if (i == 1) {
        temp.subnet[clw] = (byte)cache[clw];
        //SerialUSB.println(temp.subnet);
      } else if (i == 2) {
        temp.gateway[clw] = (byte)cache[clw];
        //SerialUSB.println(temp.gateway);
      }
      clw++;
    } else { // IP-String in einzelne Zahlen trennen
      SerialUSB.print(readString[y]);
      SerialUSB.print(" ");
      int a = readString[y] - '0';
      SerialUSB.print(a);
      SerialUSB.print(" ");
      cache[clw] = (cache[clw] * 10) + a;
      SerialUSB.println(cache[clw]);
    }
    if (y >= readString.length()) break;
  }
  //SerialUSB.println(temp.ip);
  //SerialUSB.println(temp.subnet);
  //SerialUSB.println(temp.gateway);
  chnge++;
  if (chnge == 2) { // wenn beide gekommen IP & Subnet updaten
    SerialUSB.println("updateCfg");
    config.updte(client, temp, mac); // Werte updaten
   // SerialUSB.println(standart.ip);
    chnge = 0;
  }
}

/************************************************************
** updtCfg()
**
** Schreibt neue IP-, Subnet- und Gatewaydaten in das
** EEPROM und setzt den MKR Zero so neu, dass die neuen
** Parameter beim nächsten Start aktiv sind.
************************************************************/
void Cfg::updte(EthernetClient &client, IPS &temp, byte mac[6]) {
  client.stop();
  //SerialUSB.println(temp.ip);
  //SerialUSB.println(temp.subnet);
  //SerialUSB.println(temp.gateway);
  Ethernet.begin(mac, temp.ip, temp.gateway, temp.subnet);
  server.begin();
  if (Ethernet.linkStatus() == Unknown) {
    SerialUSB.println("Link status unknown. Link status detection is only available with W5200 and W5500.");
  } else if (Ethernet.linkStatus() == LinkON) {
    SerialUSB.println("Link status: On");
  } else if (Ethernet.linkStatus() == LinkOFF) {
    SerialUSB.println("Link status: Off");
  }
  int aE = 0;
  if (temp.ip != standart.ip) {
    EEPROM.put(aE, temp.ip); // IP sichern
    SerialUSB.println("ip written");
  }
  aE += sizeof(temp.ip);
  if (temp.subnet != standart.subnet) {
    EEPROM.put(aE, temp.subnet); // Subnet sichern
    SerialUSB.println("subnet written");
  }
  aE += sizeof(temp.subnet);
  if (temp.gateway != standart.gateway) {
    EEPROM.put(aE, temp.gateway); // Gateway sichern
    SerialUSB.println("gateway written");
  }
  EEPROM.commit(); // Änderungen übernehmen
  //IPS* prt;
  IPS* prt = new IPS;

  EEPROM.get(0, prt->ip);
  SerialUSB.print("EEPROM written: ");
  //SerialUSB.println(prt.ip);
  memcpy(standart.ip, temp.ip, 4);
  memcpy(standart.subnet, temp.subnet, 4);
  memcpy(standart.gateway, temp.gateway, 4);
}

/************************************************************
** resetConfig()
**
** Wird im Setup() abgefragt.
** Wenn Taster 1 UND 2 gedrückt werden, löscht der Controller
** das EEPROM und schreibt beim nächsten Start Default-Werte.
************************************************************/
int Cfg::reset() {
  if (resetq == 1) {
    long time = millis() + 500;
    int i = 0;
    while (millis() < time) {
      if (digitalRead(button.pin[0]) == HIGH && digitalRead(button.pin[1]) == HIGH) {
        SerialUSB.println("both pressed");
        i = 1;
      } else {
        SerialUSB.println("not both pressed");
        i = 0;
        break;
      }
    }

    digitalWrite(relay.pin[5], HIGH);
    delay(1000);
    while (digitalRead(button.pin[0]) == HIGH && digitalRead(button.pin[1]) == HIGH)
      ;
    digitalWrite(relay.pin[5], LOW);
    delay(1000);
    time = millis() + 500;
    while (millis() < time) {
      if (digitalRead(button.pin[0]) == HIGH && digitalRead(button.pin[1]) == HIGH) {
        SerialUSB.println("both pressed");
        i = 1;
      } else {
        SerialUSB.println("not both pressed");
        i = 0;
        break;
      }
    }
    if (i == 1) {
      SerialUSB.println("resetting Config");
      for (int i = 0; i < 10; i++) {
        digitalWrite(relay.pin[5], HIGH);
        delay(50);
        digitalWrite(relay.pin[5], LOW);
        delay(50);
      }
      return 1; // Signal: Defaultwerte neu schreiben
    } else {
      return 0; // Kein Reset
    }
  } else {
    return 0; // Kein Reset
  }
}

/************************************************************
** Liefert JSON mit Relay-Zuständen an die Webseite      
** → wird jede Sekunde über AJAX abgefragt
************************************************************/
void sendStatus(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.print("{\"relayStates\":[");
  for (int i = 0; i < 6; i++) {
    client.print(relay.state[i] ? "true" : "false");
    if (i < 5) client.print(",");
  }
  client.println("]}");
  client.stop();
}


/*******************************************************
* Sourcecode Ende
*******************************************************/
