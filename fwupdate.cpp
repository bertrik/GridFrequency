#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <Arduino.h>
//#include <HttpsOTAUpdate.h>
#include <Update.h>

#include "fwupdate.h"
#include "fwversion.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

static FS *_fs;
// static WiFiClientSecure wifiClientSecure;
static String _update_path;
static String _update_page;
static String _url = "";

static unsigned long update_started = 0;
static int last_chunk = 0;

#define printf Serial.printf

void fwupdate_begin(FS & fs)
{
    _fs = &fs;

    // wifiClientSecure.setInsecure();

    // ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    // ESPhttpUpdate.setLedPin(LED_BUILTIN, 0);
    // ESPhttpUpdate.rebootOnUpdate(true);

    // Update.runAsync(true);
}

static String template_processor(const String & string)
{
    if (string == "fw_version") {
        return FW_VERSION;
    }
    return string;
}

static void handleGet(AsyncWebServerRequest *request)
{
    request->send(*_fs, _update_page, "text/html", false, template_processor);
}

/*
 * Called either at the start of HTTP update, or at the end of file update.
 */
static void handleRequest(AsyncWebServerRequest *request)
{
    for (size_t i = 0; i < request->args(); i++) {
        printf("%s=%s\n", request->getParam(i)->name().c_str(), request->getParam(i)->value().c_str());
    }
    String type = request->getParam("type", true)->value();
    if (type == "http") {
        _url = request->getParam("url", true)->value();
    }
    request->redirect(_update_path);
}

static void handleUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (index == 0) {
        last_chunk = 0;
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        update_started = millis();
        if (!Update.begin(maxSketchSpace, U_FLASH)) {
            printf("Update.begin() failed!\n");
        }
        request->client()->setNoDelay(true);
    }

    Update.write(data, len);
    int chunk = Update.progress() / 4096;
    digitalWrite(LED_BUILTIN, chunk & 1);
    if (chunk != last_chunk) {
        printf(".");
        last_chunk = chunk;
    }

    if (final) {
        Update.end(true);
        unsigned long duration = millis() - update_started;
        printf("done, took %ld ms\n", duration);
    }
}

static void handleReboot(AsyncWebServerRequest *request)
{
    request->redirect(_update_path);
    ESP.restart();
}

void fwupdate_serve(AsyncWebServer &server, const char *update_path, const char *update_page)
{
    _update_path = update_path;
    _update_page = update_page;

    // register ourselves with the server
    server.on(update_path, HTTP_GET, handleGet);
    server.on(update_path, HTTP_POST, handleRequest, handleUpload);
    server.on("/reboot", HTTP_GET, handleReboot);
}

void fwupdate_loop(void)
{
    if (_url != "") {
        // HttpsOTA.begin(_url.c_str(), "");


        // HttpsOTA.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        // ESPhttpUpdate.setLedPin(LED_BUILTIN, 0);
        // ESPhttpUpdate.rebootOnUpdate(true);
        // switch (ESPhttpUpdate.update(wifiClientSecure, _url)) {
        // case HTTP_UPDATE_FAILED:
        //     printf("failed!\n");
        //     break;
        // case HTTP_UPDATE_NO_UPDATES:
        //     printf("no update!\n");
        //     break;
        // case HTTP_UPDATE_OK:
        //     printf("OK!\n");
        //     break;
        // default:
        //     break;
        // }
        _url = "";
    }
}


