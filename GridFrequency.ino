#include <Arduino.h>
#include <ESPmDNS.h>

#include <WiFiManager.h>
#include <PubSubClient.h>
#include <MiniShell.h>
#include "AsyncEventSource.h"

#include "measure.h"

#define printf Serial.printf

#define BASE_FREQUENCY      50.0
#define PIN_50HZ_INPUT      0
#define PIN_LED             12
#define SAMPLE_FREQUENCY    5000

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

static void show_help(const cmd_t * cmds)
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
    { "reboot", do_reboot, "Reboot"},
    { "help", do_help, "Show help" },
    { NULL, NULL, NULL }
};

static int do_help(int argc, char *argv[])
{
    show_help(commands);
    return 0;
}

void setup(void)
{
    pinMode(PIN_LED, OUTPUT);
    measure_init(PIN_50HZ_INPUT, BASE_FREQUENCY);

    Serial.begin(115200);
    Serial.println("\nESP32PHASE");

    WiFiManager wm;
    wm.autoConnect(esp_id);

    // set up web server
    events.onConnect([](AsyncEventSourceClient *client){
        printf("Client connected, last id: %d\n", client->lastId());
    });
    server.addHandler(&events);    
    server.begin();
    
    MDNS.begin("gridfrequency");
    MDNS.addService("http", "tcp", 80);
}

void loop(void)
{
    // run the frequency algorithm continuously
    static double prev_angle = 0.0;
    double angle, ampl;
    if (measure_loop(angle, ampl)) {
        double d = angle - prev_angle;
        if (d < -180.0) {
            d += 360.0;
        } else if (d > 180.0) {
            d -= 360.0;
        }
        double t = 1.0 - (d / 360.0) / BASE_FREQUENCY;
        double freq = BASE_FREQUENCY / t;
        prev_angle = angle;

        printf("Angle:%8.3f, dAngle:%7.3f, Freq:%7.3f, Ampl:%5.1f\n", angle, d, freq, ampl);

        char value[64];
        sprintf(value, "{\"frequency\":%.3f,\"phase\":%.3f,\"amplitude\":%.1f}", freq, angle,
                ampl);
        events.send(value, "measurement", event_id++);
    }

    // command line processing
    shell.process(">", commands);
}

