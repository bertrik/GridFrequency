#include <Arduino.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>

#include <WiFiManager.h>
#include <PubSubClient.h>
#include <MiniShell.h>
#include <AsyncEventSource.h>
#include <ArduinoJson.h>

#include "measure.h"

#define printf Serial.printf

#define BASE_FREQUENCY      50.0
#define PIN_50HZ_INPUT      0
#define PIN_LED_D4          12
#define PIN_LED_D5          13

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

static StaticJsonDocument < 1024 > jsonDoc;

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

static String template_processor(const String & string)
{
    // replace according to pattern"propName:propValue?replacement"
    int i = string.indexOf("?");
    if (i >= 0) {
        String property = string.substring(0, i);
        i = property.indexOf(":");
        if (i >= 0) {
            // split property in name and matching value
            String propName = property.substring(0, i);
            String propValue = property.substring(i + 1);
            String actual = jsonDoc[propName] | "";
            if (actual == propValue) {
                return string.substring(i + 1);
            }
        }
    }

    // try regular match
    return jsonDoc[string] | "";
}

static void handleGetConfig(AsyncWebServerRequest *request)
{
    // read the config file in preparation of presentation in the web UI
    File configFile = SPIFFS.open("/config.json", "r");
    deserializeJson(jsonDoc, configFile);
    configFile.close();

    // serve the config page from flash
    request->send(SPIFFS, "/config.html", "text/html", false, template_processor);
}

static void handlePostConfig(AsyncWebServerRequest *request)
{
    // put all parameters in the JSON document
    for (size_t i = 0; i < request->params(); i++) {
        AsyncWebParameter *param = request->getParam(i);
        String name = param->name();
        String value = param->value();
        jsonDoc[name] = value;
    }

    // write the config file
    File configFile = SPIFFS.open("/config.json", "w");
    serializeJson(jsonDoc, configFile);
    configFile.close();

    // redirect to page with values
    request->redirect("/config");
}

void setup(void)
{
    Serial.begin(115200);
    Serial.println("\nESP32PHASE");

    pinMode(PIN_LED_D4, OUTPUT);
    digitalWrite(PIN_LED_D4, 0);
    pinMode(PIN_LED_D5, OUTPUT);
    digitalWrite(PIN_LED_D5, 0);

    measure_init(PIN_50HZ_INPUT, BASE_FREQUENCY);

    WiFiManager wm;
    wm.autoConnect(esp_id);

    // set up web server
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    events.onConnect(on_event_connect);

    SPIFFS.begin();
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
    server.addHandler(&events);
    server.on("/config", HTTP_GET, handleGetConfig);
    server.on("/config", HTTP_POST, handlePostConfig);
    server.begin();

    MDNS.begin("gridfrequency");
    MDNS.addService("_http", "_tcp", 80);
}

void loop(void)
{
    // run the frequency algorithm continuously
    static double prev_phase = 0.0;
    double phase, ampl;
    if (measure_loop(phase, ampl)) {
        digitalWrite(PIN_LED_D4, !digitalRead(PIN_LED_D4));

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
