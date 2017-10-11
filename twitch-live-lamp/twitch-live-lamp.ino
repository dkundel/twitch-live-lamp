#include "ESP8266WiFi.h"
#include "WiFiClientSecure.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "Config.h"

#define PIN 5

/* WiFi SSID and Password */
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

/* 
 *  Sync Settings
 *  
 *  Enter a Sync Key & Password, your document unique name, 
 *  and the device name
 */
const char *sync_key = TWILIO_SYNC_KEY;
const char *sync_password = TWILIO_SYNC_PASSWORD;
const char *sync_document = TWILIO_SYNC_DOCUMENT;
const char *sync_device_name = TWILIO_SYNC_DEVICE_NAME;

/* Sync server and MQTT setup; you probably don't have to change these. */
const char *mqtt_server = "mqtt-sync.us1.twilio.com";
const uint16_t mqtt_port = 8883;
const uint16_t maxMQTTpackageSize = 512;

/* Values for the LED matrix */
const int CIRCLE[8][8] = {
    {0, 0, 0, 1, 1, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 0},
    {0, 1, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0},
    {0, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 1, 1, 0, 0, 0}};

const int MIN_BRIGHTNESS = 10;
const int MAX_BRIGHTNESS = 80;

void callback(char *, byte *, unsigned int);
WiFiClientSecure espClient;
PubSubClient client(mqtt_server, mqtt_port, callback, espClient);

/* 
 * Only use the fingerprint if you have a certificate rotation strategy in place.
 * It can be enabled by setting the use_fingerprint boolean to 'true'
 *  
 * SHA1 Fingerprint valid as of August 2017, but if it expires:
 * On *NIX systems with openssl installed, you can check the fingerprint with
 * 
 * echo | openssl s_client -connect mqtt-sync.us1.twilio.com:8883 | openssl x509 -fingerprint
 * 
 * ... and look for the 'SHA1 Fingerprint' line.
 */
const char *fingerprint =
    "4E:D6:B4:16:83:9F:86:86:0E:8B:BA:47:F6:FC:3F:65:29:B6:E1:13";
const bool use_fingerprint = false;

/* Flag used to determine whether we should pulse the LEDs or not */
bool isOnline = false;

/* Matrix instance */
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, PIN,
                                               NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
                                                   NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
                                               NEO_GRB + NEO_KHZ800);

/*
 * Draws a red circle on the LED matrix
 */
void drawCircle(int brightness)
{
        for (int row = 0; row < 8; row++)
        {
                for (int column = 0; column < 8; column++)
                {
                        if (CIRCLE[row][column] == 1)
                        {
                                matrix.drawPixel(column, row, matrix.Color(255, 0, 0));
                        }
                }
        }
        matrix.setBrightness(brightness);
        matrix.show();
}

/*
 * Makes the LED matrix pulse by looping and changing the brightness of the LEDs
 * between MIN_BRIGHTNESS and MAX_BRIGHTNESS
 */
void pulseCircle()
{
        for (int brightness = MIN_BRIGHTNESS; brightness <= MAX_BRIGHTNESS; brightness++)
        {
                drawCircle(brightness);
                delay(10);
        }
        for (int brightness = MAX_BRIGHTNESS; brightness >= MIN_BRIGHTNESS; brightness--)
        {
                drawCircle(brightness);
                delay(10);
        }
}

/* 
 * Our Twilio Connected Devices message handling callback.  This is passed as a 
 * callback function when we subscribe to the document, and any messages will 
 * appear here.
 */
void callback(char *topic, byte *payload, unsigned int length)
{
        std::unique_ptr<char[]> msg(new char[length + 1]());
        memcpy(msg.get(), payload, length);

        StaticJsonBuffer<maxMQTTpackageSize> jsonBuffer;
        JsonObject &root = jsonBuffer.parseObject(msg.get());
        String led_command = root["led"];

        if (led_command == "ON")
        {
                // Light up the board LED as a verification
                digitalWrite(BUILTIN_LED, LOW);
                isOnline = true;
        }
        else
        {
                // Turn off both the board LED and the matrix
                digitalWrite(BUILTIN_LED, HIGH);
                matrix.fillScreen(0);
                matrix.show();
                isOnline = false;
        }
}

/* 
 * This function connects to Sync via MQTT. We connect using the key, password, and 
 * device name defined as constants above, and immediately check the server's 
 * certificate fingerprint (if desired).
 * 
 * If everything works, we subscribe to the document topic and return.
 */
void connect_mqtt()
{
        while (!client.connected())
        {
                if (client.connect(sync_device_name, sync_key, sync_password))
                {
                        /* Verify you are talking to Twilio */
                        if (!use_fingerprint || espClient.verify(fingerprint, mqtt_server))
                        {
                                client.subscribe(sync_document);
                        }
                        else
                        {
                                client.disconnect();
                                while (1)
                                        ;
                        }
                }
                else
                {
                        delay(10000);
                }
        }
}

/* In setup, we configure our LED for output, turn it off, and connect to WiFi */
void setup()
{
        /* Initialize the the matrix with some default values and turn it to white */
        matrix.begin();
        matrix.setBrightness(30);
        matrix.setTextColor(matrix.Color(255, 255, 255));
        matrix.setTextWrap(false);
        matrix.fillScreen(matrix.Color(255, 225, 255));
        matrix.show();

        /* Configure board LED */
        pinMode(BUILTIN_LED, OUTPUT);
        digitalWrite(BUILTIN_LED, HIGH); // Active LOW LED

        WiFi.begin(ssid, password);

        while (WiFi.status() != WL_CONNECTED)
        {
                delay(1000);
        }

        /* Turn off the matrix as soon as the WiFi is connected */
        matrix.fillScreen(0);
        matrix.show();

        randomSeed(micros());
}

/*
 * Our loop constantly checks we are still connected.  On disconnects we try again.
 * It will also pulse the LED if isOnline is active.
 */
void loop()
{
        if (!client.connected())
        {
                connect_mqtt();
        }
        client.loop();

        if (isOnline)
        {
                pulseCircle();
        }
}
