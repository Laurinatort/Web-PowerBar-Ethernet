#include <SPI.h>                 // SPI-Schnittstelle für Ethernet
#include <Ethernet.h>            // Ethernet Stack für W5x00-Chips
#include <FlashAsEEPROM_SAMD.h>  // EEPROM-Emulation für SAMD21

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