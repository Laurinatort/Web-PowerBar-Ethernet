/************************************************************
** Web-Switch 6-way                                       **
** Arduino MKR ZERO                                       **
** SAMD21 Cortex-M0+ 32bit low power ARM                  **
** DI/O 22 (PWM12)                                        **
** AI 7 (ADC 8/10/12 bit)                                 **
** AO 1 (DAC 10bit)                                       **
** UART, SPI, I2C                                         **
** Flash 256KB                                            **
** SRAM   32KB                                            **
** Arduino MKS ETH Shield                                 **
** L. Gehrig 18.11.2025                                   */
char Version[] = "V0.1";  // Display SW version on website

/*
Versions:
----------
V0.1             Prototype Software

Hardware:
---------
Arduino MKR

Button Connections:
---------------------------------------------
Arduino Pin:  Function:                  Type:
15            Button T1                  D in
16            Button T2                  D in
17            Button T3                  D in
18            Button T4                  D in
19            Button T5                  D in
20            Button T6                  D in

Relay Connections:
---------------------------------------------
Arduino Pin:  Function:                  Type:
0              Relay Z1                  D out
1              Relay Z2                  D out
2              Relay Z3                  D out
3              Relay Z4                  D out
6              Relay Z5                  D out
7              Relay Z6                  D out
*/

/************************************************************
** Debug Settings                                          
************************************************************/
#define FLASH_DEBUG 0

/*******************************************************
* Libraries
*******************************************************/
#include <SPI.h>                 // SPI interface for Ethernet
#include <Ethernet.h>            // Ethernet Stack for W5x00 chips
#include <FlashAsEEPROM_SAMD.h>  // EEPROM emulation for SAMD21

/*******************************************************
* Variables, Definitions, Classes
*******************************************************/
#define ETH_CS 5 // Chip-Select pin for the Ethernet controller
#define EEPROM_EMULATION_SIZE 4096
class Relay{
  public:
    bool state[6]; // Status of the six relays (true = on / false = off)
    static inline constexpr  int pin[6]  = { 0, 1, 2, 3, 6, 7 };  // Mapping digital pins to relays
    String tglex; // Helper string for detecting "toggle-relayX"
    void power();
    void toggle(EthernetClient &client, int i);
} relay;

class Button{
  public:
    static inline constexpr  int pin[6] = { 15, 16, 17, 18, 19, 20 }; // Mapping digital pins to buttons
    int pressed[6];  // Debounce status of buttons (0 = not pressed, 1 = pressed)
    void detect(Relay& relay);
} button;

// Standard network parameters
// ... [IPS Class remains same] ...
IPS temp; // Temporary storage for new network configurations
byte mac[] = { 0x00, 0x04, 0xA3, 0x00, 0x00, 0x06 };  // MAC address of the Ethernet shield

// Internal helper variables
int chnge = 0;
char c;
String cfgexp[2] = { "ip", "subnet" };  // Strings for config detection in HTTP-POST
byte first = 1;                         // Flag for "first initialization"
int resetq = 0;                         // Variables for reset detection
String readString;                      // Buffer for incoming HTTP requests

/************************************************************
** Ethernet Server Instance on Port 80                      
************************************************************/
EthernetServer server(80);

/*******************************************************
* Setup — runs once at start 
*******************************************************/
void setup() {
  SerialUSB.begin(115200); // Start serial interface
  //while (!SerialUSB);
  delay(1000);
  SerialUSB.println("SerialUSB begin");
  // Set all Relay and Button pins
  for (int i = 0; i < 6; i++) {
    pinMode(relay.pin[i], OUTPUT);
    pinMode(button.pin[i], INPUT);
  }
  delay(1000);

  // Check EEPROM length
  byte initFlag = EEPROM.read(63);
  byte val = EEPROM.read(63);
  int elngth = (EEPROM.length() - 1);

  // Check if Buttons 1 AND 2 are pressed at start → prepare reset
  if (digitalRead(button.pin[0]) == HIGH && digitalRead(button.pin[1]) == HIGH) { 
    SerialUSB.print("reset");
    resetq = 1;
  }  

  // Check if EEPROM is empty or Reset is requested
  if (val == 0xFF || config.reset() == 1) {   
    SerialUSB.println("first");
    int eA = 0; // EEPROM Address pointer
    EEPROM.put(eA, standart.ip); 
    eA += sizeof(standart.ip);
    EEPROM.put(eA, standart.subnet); // Save Subnet
    eA += sizeof(standart.subnet);
    EEPROM.put(eA, standart.gateway); // Save Gateway
    eA += sizeof(standart.gateway);
    EEPROM.commit(); // Apply changes
    delay(500);
    EEPROM.write(63, first); // Write "initialized" marker
    EEPROM.commit();
  }

  // Read current configuration from EEPROM
  if (1 > 0) {  
    int eA = 0;
    EEPROM.get(eA, standart.ip);
    eA += sizeof(standart.ip);
    EEPROM.get(eA, standart.subnet);
    eA += sizeof(standart.subnet);
    EEPROM.get(eA, standart.gateway);
  }
  delay(500);

  // Start Ethernet
  Ethernet.begin(mac, standart.ip, standart.gateway, standart.subnet); 
  server.begin();

  // Output Link Status
  // ... [Link Status logic remains same] ...
  relay.tglex = "relay0";
}

/************************************************************
** LOOP — Main program, runs continuously                 
************************************************************/
void loop() {
  EthernetClient client = server.available(); // Checks if a new Ethernet client (browser) is connected
  if (client) {
    // A client is connected – processing of the HTTP request starts here
    String currentLine = ""; // Buffer for the line currently being read
    while (client.connected()) {
      
      // Check if data was received from client
      if (client.available()) {
        boolean currentLineIsBlank = true;
        c = client.read();  // Read single character from HTTP buffer
        if (readString.length() < 1000) { // Protection against overflow (max 1000 chars)
          readString += c;   // Collect complete HTTP request
          currentLine += c;  // Collect current line
        }

        // Was a new line received?
        if (c == '\n' && currentLineIsBlank) { 
          // BLANK LINE SIGNALS:
          // → End of HTTP Request Header
          // → Now the request can be evaluated
          
          if (readString.indexOf("toggle") >= 0) { // Check if request contains toggle command
            for (int i = 0; i < 6; i++) {  // Check all relays to see which was addressed
              relay.tglex[5] = i + '0'; // creates e.g. "relay0", "relay1" ...
              if (readString.indexOf(relay.tglex) >= 0) {
                relay.toggle(client, i); // Switch relay
                readString = "";
                break;
              }
            }
          } else if (readString.indexOf("POST") >= 0) { // Check if POST for network config
            for (int i = 0; i < 2; i++) {
              if (readString.indexOf(cfgexp[i]) >= 0) {
                config.change(client, i); // Interpret new parameters
                break;
              }
            }
          } else if (readString.indexOf("status") >= 0) { // Status request (AJAX /status)
            sendStatus(client);
          } else { // If nothing matches → output website
            printWEB(client, Version);
            break;
          }
          readString = "";
        }
        if (c == '\n') { // Check if end of line reached
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
      button.detect(relay); // *** Check buttons cyclically ***
    }
    delay(1);
    client.stop(); // Disconnect
  }
  button.detect(relay); // Monitor buttons even without a client
}

/************************************************************
** Button detection + Debouncing                         
************************************************************/
void Button::detect(Relay& relay) {
  for (int i = 0; i < 6; i++) {
    if (digitalRead(button.pin[i]) == HIGH && button.pressed[i] == 0) { // Button pressed (HIGH) AND was not pressed before?
      delay(30); // simple debouncing
      if (relay.state[i] == 0) { // If relay was off → turn on
        relay.state[i] = 1;
      } else {
        relay.state[i] = 0;
      }
      button.pressed[i] = 1; // Remember: Button is considered pressed
    } else if (digitalRead(button.pin[i]) == LOW && button.pressed[i] == 1) { // If released → reset pressed marker
      button.pressed[i] = 0;
    }
  }
  relay.power(); // Physically switch relays after every change
}

/************************************************************
** Transfers relaystate[] to the actual pins             
************************************************************/
void Relay::power() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(relay.pin[i], relay.state[i]);
  }
}

/************************************************************
** Toggle a single relay via Web                         
************************************************************/
void Relay::toggle(EthernetClient &client, int i) {
  relay.state[i] = !relay.state[i];  // Invert state
  relay.power();                    // Update hardware
}

/************************************************************
** printWEB()
** Outputs the complete HTML website to the client.
** This function generates:
** - HTML skeleton
** - CSS styling
** - Buttons for switching relays
** - LED indicators for each relay state
** - Settings popup (IP/Subnet)
** - JavaScript logic for AJAX, buttons, popup
************************************************************/
void printWEB(EthernetClient &client, char Version[5]) {
  // ... [CSS logic inside printWEB remains largely the same, but comments inside are now English] ...
  client.println("            body {"); // Body of the page
  client.println("            h1, h2, h3 {"); // Headings
  client.println("            p, label {"); // Text
  client.println("            .led {"); // Stylish LED display
  client.println("            .led.on {"); // LED = ON
  client.println("            .led.off { background-color: gray; }"); // LED = OFF
  client.println("            button {"); // Buttons (Relay)
  client.println("            #overlay {"); // Overlay for settings
  client.println("            #configPopup {");  // Popup settings → Aero Glass Effect
  client.println("            #configContent {");  // Content in Popup
  client.println("            #configPopup input {"); // Input field styling
  client.println("            #buttonBar {"); // Button area in Popup
  client.println("            #header {"); // Header at the top of the page

  // ---------------------------------------------------------
  // HTML-Body
  // ---------------------------------------------------------
  client.println("    <body>");
  // ---------------------------------------------------------
  // HEADER AREA WITH TITLE & SETTINGS BUTTON
  // ---------------------------------------------------------
  client.println("        <div id='header'>");
  client.println("          <span id='headerText'>Web-Switch Walker Josef Altdorf</span>");
  client.println("          <button id='settingsBtn'>⚙️ Settings</button>");
  client.println("        </div>");

  client.println("        <div id='mainContent'>"); // main content
  // ---------------------------------------------------------
  // RELAY BUTTONS + LED's
  // ---------------------------------------------------------
  client.println("<div class='relay'>"); // Generate 6 relays + 6 LEDs
    for (int i = 0; i < 6; i++) {
      client.print("<button id='Button");
      client.print(i);
      client.print("'>Outlet ");
      client.print(i + 1);
      client.println("</button>");
      client.print("<div id='led");
      client.print(i);
      client.println("' class='led off'></div>");
    }
  client.println("</div>");

  // ---------------------------------------------------------
  // NETWORK INFORMATION (Version, IP, MAC, Subnet)
  // ---------------------------------------------------------
  client.println("            <div id='leftInfo'>");
  // ... [Network info display logic] ...
  
  // ---------------------------------------------------------
  // JAVASCRIPT — Buttons, AJAX, Popup
  // ---------------------------------------------------------
  client.println("        <script>");
  client.println("window.onload = function(){");
  // At page start: embed current relay states
  client.println("            const busy=[false,false,false,false,false,false];"); // Busy flags (to prevent double clicks)
  // ... [Rest of JS logic] ...
