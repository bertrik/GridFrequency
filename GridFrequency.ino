#include <Arduino.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <MiniShell.h>
#include "AsyncEventSource.h"

#include "measure.h"

#define printf Serial.printf

#define BASE_FREQUENCY      50.0
#define PIN_50HZ_INPUT      0
#define PIN_LED             12

#define MQTT_HOST   "stofradar.nl"
#define MQTT_PORT   1883
#define MQTT_TOPIC  "bertrik/mains"

static char line[120];
static char esp_id[] = "esp32-50hz";

static AsyncWebServer server(80);
static AsyncEventSource events("/events");

static MiniShell shell(&Serial);
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static int event_id = 0;

static void show_help(const cmd_t *cmds)
{
    for (const cmd_t * cmd = cmds; cmd->cmd != NULL; cmd++) {
        printf("%10s: %s\r\n", cmd->name, cmd->help);
    }
}

static int do_help(int argc, char *argv[]);

static int do_reboot(int argc, char *argv[])
{
    ESP.restart();
    return 0;
}

const cmd_t commands[] = {
    { "reboot", do_reboot, "Reboot" },
    { "help", do_help, "Show help" },
    { NULL, NULL, NULL }
};

static int do_help(int argc, char *argv[])
{
    show_help(commands);
    return 0;
}

static void on_event_connect(AsyncEventSourceClient *client)
{
    IPAddress remoteIP = client->client()->remoteIP();
    printf("Client connected: %s\n", remoteIP.toString().c_str());
}

void setup(void)
{
    Serial.begin(115200);
    Serial.println("\nESP32PHASE");

    pinMode(PIN_LED, OUTPUT);
    measure_init(PIN_50HZ_INPUT, BASE_FREQUENCY);

    WiFiManager wm;
    wm.autoConnect(esp_id);

    // set up web server
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    events.onConnect(on_event_connect);

    SPIFFS.begin();
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    server.addHandler(&events);
    server.begin();

    MDNS.addService("http", "tcp", 80);
    MDNS.begin("gridfrequency");
}

void loop(void)
{
    // run the frequency algorithm continuously
    static double prev_phase = 0.0;
    double phase, ampl;
    if (measure_loop(phase, ampl)) {
        digitalWrite(PIN_LED, !digitalRead(PIN_LED));

        double delta = phase - prev_phase;
        if (delta < -180.0) {
            delta += 360.0;
        } else if (delta > 180.0) {
            delta -= 360.0;
        }
        double t = 1.0 - (delta / 360.0) / BASE_FREQUENCY;
        double freq = BASE_FREQUENCY / t;
        prev_phase = phase;

        printf("Phase:%8.3f, Delta:%8.3f, Ampl:%6.1f, Freq:%7.3f\n", phase, delta, ampl, freq);

        char value[64];
        sprintf(value, "{\"phase\":%.1f,\"amplitude\":%.1f,\"frequency\":%.3f}", phase, ampl, freq);
        events.send(value, NULL, event_id++);
    }
    // command line processing
    shell.process(">", commands);
}
