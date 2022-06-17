// Your GPRS credentials (leave empty, if not needed)
const char apn[] = "everywhere";    // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = "eesecure"; // GPRS User
const char gprsPass[] = "secure";   // GPRS Password

// SIM card PIN (leave empty, if not defined)
const char simPIN[] = "";

// Server details
// The server variable can be just a domain name or it can have a subdomain. It depends on the service you are using
const char server[] = "walkabout.azurewebsites.net"; // domain name: example.com, maker.ifttt.com, etc
const char post[] = "/saveLocation";
const char resource[] = "?Longitude=tlsesp32&Latitude=tlsesp32";  // resource path, for example: /post-data.php
const int  port = 443;                               // server port number

// Keep this API Key value to be compatible with the PHP code provided in the project page.
// If you change the apiKeyValue value, the PHP file /post-data.php also needs to have the same key
// String apiKeyValue = "";

// TTGO T-Call pins
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#define SMS_TARGET ""

// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS

#include <Wire.h>
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
    StreamDebugger debugger(SerialAT, SerialMon);
    TinyGsm modem(debugger);
#else
    TinyGsm modem(SerialAT);
#endif

// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);

// TinyGSM Client for Internet connection
// TinyGsmClient client(modem);

TinyGsmClientSecure secureClient(modem, 0);
HttpClient http_client = HttpClient(secureClient, server, port);

#define uS_TO_S_FACTOR 1000000UL /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 300        /* Time ESP32 will go to sleep (in seconds) 3600 seconds = 1 hour */

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool setPowerBoostKeepOn(int en)
{
    I2CPower.beginTransmission(IP5306_ADDR);
    I2CPower.write(IP5306_REG_SYS_CTL0);
    if (en)
    {
        I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
    }
    else
    {
        I2CPower.write(0x35); // 0x37 is default reg value
    }
    return I2CPower.endTransmission() == 0;
}

void setup()
{
    // Set serial monitor debugging window baud rate to 115200
    SerialMon.begin(115200);

    uint32_t I2C_A = 400000;
    // Start I2C communication
    I2CPower.begin(I2C_SDA, I2C_SCL, I2C_A);

    // Keep power when running from battery
    bool isOk = setPowerBoostKeepOn(1);
    SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

    // Set modem reset, enable, power pins
    pinMode(MODEM_PWKEY, OUTPUT);
    pinMode(MODEM_RST, OUTPUT);
    pinMode(MODEM_POWER_ON, OUTPUT);
    digitalWrite(MODEM_PWKEY, LOW);
    digitalWrite(MODEM_RST, HIGH);
    digitalWrite(MODEM_POWER_ON, HIGH);

    // Set GSM module baud rate and UART pins
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(3000);

    // Restart SIM800 module, it takes quite some time
    // To skip it, call init() instead of restart()
    SerialMon.println("Initializing modem...");
    modem.restart();
    // use modem.init() if you don't need the complete restart
    // Unlock your SIM card with a PIN if needed
    if (strlen(simPIN) && modem.getSimStatus() != 3)
    {
        modem.simUnlock(simPIN);
    }

    // Configure the wake up source as timer wake up
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}

String urlencode(const String &s)
{
    static const char lookup[] = "0123456789abcdef";
    String result;
    size_t len = s.length();

    for (size_t i = 0; i < len; i++)
    {
        const char c = s[i];
        if (('0' <= c && c <= '9') || ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || (c == '-' || c == '_' || c == '.' || c == '~'))
        {
            result += c;
        }
        else
        {
            result += "%" + String(lookup[(c & 0xf0) >> 4]) + String(lookup[c & 0x0f]);
        }
    }
    return result;
}

//**************************************************************************************************
void Post(const char* method, const String & path , const String & data, HttpClient* http) {
  String response;
  int statusCode = 0;
  http->connectionKeepAlive(); // Currently, this is needed for HTTPS
  
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
  String url;
  if (path[0] != '/') {
    url = "/";
  }
  url += path; 
  Serial.print("POST:");
  Serial.println(url);
  Serial.print("Data:");
  Serial.println(data);
  
  Serial.print(server);
  Serial.print(url);
 

  String contentType = "application/json";
  http->post(server + url, contentType, data);
  
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
  // read the status code and body of the response
  //statusCode-200 (OK) | statusCode -3 (TimeOut)
  statusCode = http->responseStatusCode();
  Serial.print("Status code: ");
  Serial.println(statusCode);
  response = http->responseBody();
  Serial.print("Response: ");
  Serial.println(response);
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN

  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
  if (!http->connected()) {
    Serial.println();
    http->stop();// Shutdown
    Serial.println("HTTP POST disconnected");
  }
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
}


void loop()
{
    SerialMon.print("Connecting to APN : ");
    SerialMon.print(apn);
    if (!modem.gprsConnect(apn, gprsUser, gprsPass))
    {
        SerialMon.println(" fail");
    }
    else
    {
        delay(5000);
        SerialMon.println();
        Post("POST",post, resource, &http_client);
        // Close client and disconnect
        secureClient.stop();
        SerialMon.println(F("Server disconnected"));
        modem.gprsDisconnect();
        SerialMon.println(F("GPRS disconnected"));
    }

    // Put ESP32 into deep sleep mode (with timer wake up)
    esp_deep_sleep_start();
}

