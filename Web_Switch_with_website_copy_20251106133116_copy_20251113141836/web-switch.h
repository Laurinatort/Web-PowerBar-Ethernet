#include <SPI.h>                 // SPI interface for Ethernet
#include <Ethernet.h>            // Ethernet Stack for W5x00 chips
#include <FlashAsEEPROM_SAMD.h>  // EEPROM emulation for SAMD21

#define ETH_CS 5 // Ethernet controller Chip-Select pin
#define EEPROM_EMULATION_SIZE 4096

class Relay{
  public:
    bool state[6]; // Status of the six relays (true = on / false = off)
    static inline constexpr  int pin[6]  = { 0, 1, 2, 3, 6, 7 };  // Mapping the digital pins to the relays
    String tglex; // Helper string for detecting "toggle-relayX"
    void power();
    void toggle(EthernetClient &client, int i);
} relay;

class Button{
  public:
    static inline constexpr  int pin[6] = { 15, 16, 17, 18, 19, 20 }; // Mapping the digital pins to the buttons
    int pressed[6];  // Debounce status of the buttons (0 = not pressed, 1 = pressed)
    void detect(Relay& relay);
} button;

// Standard network parameters
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
IPS temp; // Temporary storage for new network configurations
byte mac[] = { 0x00, 0x04, 0xA3, 0x00, 0x00, 0x06 };  // MAC address of the Ethernet shield

class Cfg {
  public:
    void change(EthernetClient &client, int i);
    void updte(EthernetClient &client, IPS &temp, byte mac[6]);
    int reset();
} config;

// Internal helper variables
int chnge = 0;
char c;
String cfgexp[2] = { "ip", "subnet" };  // Strings for configuration detection in HTTP-POST
byte first = 1;                         // Flag for "first initialization"
int resetq = 0;                         // Variables for reset detection
String readString;                      // Buffer for incoming HTTP requests

/************************************************************
** Ethernet Server Instance on Port 80                      
************************************************************/

EthernetServer server(80);
