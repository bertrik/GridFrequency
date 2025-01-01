#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#include <WiFiManager.h>
#include <PubSubClient.h>
#include <MiniShell.h>
#include <AsyncEventSource.h>
#include <ArduinoJson.h>

#include "measure.h"
#include "config.h"
#include "fsimage.h"
#include "fwupdate.h"

#define printf Serial.printf

#define BASE_FREQUENCY      50.0
#define PIN_50HZ_INPUT      0
#define PIN_LED_D4          12
#define PIN_LED_D5          13

#define HOST_NAME   "gridfrequency"

static char line[120];
static char esp_id[64];

static AsyncWebServer server(80);
static AsyncEventSource events("/events");

static MiniShell shell(&Serial);
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static struct {
    char host[64];
    uint16_t port;
    char user[32];
    char pass[32];
    char topic[64];
    char format[16];
    bool retained;
} mqtt_config;

static int event_id = 0;

static void show_help(const cmd_t *cmds)
{
    for (const cmd_t * cmd = cmds; cmd->cmd != NULL; cmd++) {
        printf("%10s: %s\r\n", cmd->name, cmd->help);
    }
}

static int do_help(int argc, char *argv[]);

static int do_start(int argc, char *argv[])
{
    measure_start();
    return 0;
}

static int do_stop(int argc, char *argv[])
{
    measure_stop();
    return 0;
}

static int do_reboot(int argc, char *argv[])
{
    ESP.restart();
    return 0;
}

const cmd_t commands[] = {
    { "start", do_start, "Start sample process"},
    { "stop", do_stop, "Stop sample process"},
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
    // init low-level
    pinMode(PIN_LED_D4, OUTPUT);
    digitalWrite(PIN_LED_D4, 0);
    pinMode(PIN_LED_D5, OUTPUT);
    digitalWrite(PIN_LED_D5, 0);

    // init serial output
    Serial.begin(115200);
    Serial.println("\ngridfrequency");

    // esp id
    uint32_t chipId = ESP.getEfuseMac() & 0xFFFFFFFFL;
    snprintf(esp_id, sizeof(esp_id), "esp32-%08X", chipId);

    // unpack local files
    LittleFS.begin(true);
    fsimage_unpack(LittleFS, false);
    fwupdate_begin(LittleFS);

    // load settings, save defaults if necessary
    config_begin(LittleFS, "/config.json");
    if (!config_load()) {
        config_set_value("mqtt_broker_host", "");
        config_set_value("mqtt_broker_port", "1883");
        config_set_value("mqtt_user", "");
        config_set_value("mqtt_pass", "");
        config_set_value("mqtt_topic", "");
        config_set_value("mqtt_format", "json");
        config_set_value("mqtt_retained", "true");
        config_save();
    }

    // connect to WiFi
    WiFi.setHostname(HOST_NAME);
    WiFiManager wm;
    wm.autoConnect(HOST_NAME);

    // set up web server
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    events.onConnect(on_event_connect);
    server.addHandler(&events);

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    config_serve(server, "/config", "/config.html");
    fwupdate_serve(server, "/update", "/update.html");
    server.begin();

    MDNS.begin(HOST_NAME);
    MDNS.addService("_http", "_tcp", 80);

    measure_init(PIN_50HZ_INPUT, BASE_FREQUENCY);
    measure_start();
}

static void check_mqtt(void)
{
    static int last_version = -1;

    int version = config_get_version();
    if (version != last_version) {
        printf("Found new config version %d, reloading...\n", version);
        last_version = version;

        // update config struct
        strlcpy(mqtt_config.host, config_get_value("mqtt_broker_host").c_str(), sizeof(mqtt_config.host));
        mqtt_config.port = atoi(config_get_value("mqtt_broker_port").c_str());
        strlcpy(mqtt_config.user, config_get_value("mqtt_user").c_str(), sizeof(mqtt_config.user));
        strlcpy(mqtt_config.pass, config_get_value("mqtt_pass").c_str(), sizeof(mqtt_config.pass));
        strlcpy(mqtt_config.topic, config_get_value("mqtt_topic").c_str(), sizeof(mqtt_config.topic));
        strlcpy(mqtt_config.format, config_get_value("mqtt_format").c_str(), sizeof(mqtt_config.format));
        mqtt_config.retained = config_get_value("mqtt_retained").equals("true");

        // trigger re-connect
        if (mqttClient.connected()) {
            mqttClient.disconnect();
            printf("Disconnected MQTT\n");
        }
    } else if ((WiFi.status() == WL_CONNECTED) && *mqtt_config.host && !mqttClient.connected()) {
        mqttClient.setServer(mqtt_config.host, mqtt_config.port);
        printf("Connecting MQTT %s:%u for topic %s...", mqtt_config.host, mqtt_config.port, mqtt_config.topic);
        bool ok = mqttClient.connect(esp_id, mqtt_config.user, mqtt_config.pass, mqtt_config.topic, 0, mqtt_config.retained, "offline");
        printf("%s\n", ok ? "OK" : "FAIL");
    }
}

void loop(void)
{
    // attempt to connect MQTT if appropriate
    check_mqtt();

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

        // send over SSE
        char value[64];
        sprintf(value, "{\"phase\":%.1f,\"amplitude\":%.1f,\"frequency\":%.3f}", phase, ampl, freq);
        events.send(value, NULL, event_id++);

        // send over MQTT
        if (mqttClient.connected()) {
            if (strcmp("value", mqtt_config.format) == 0) {
                sprintf(value, "%.3f", freq);
            } else if (strcmp("value_unit", mqtt_config.format) == 0) {
                sprintf(value, "%.3f Hz", freq);
            }
            mqttClient.publish(mqtt_config.topic, value, mqtt_config.retained);
        }
    }
    // command line processing
    shell.process(">", commands);

    // process mqtt events
    mqttClient.loop();

    // firmware update
    fwupdate_loop();
}
