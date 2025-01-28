#include <Arduino.h>
#include <ArduinoJson.h>

#include "config.h"

static FS *_fs;
static StaticJsonDocument < 1024 > _jsonDoc;
static String _config_name;
static String _config_path;
static String _config_page;
static int _version = 0;

static void handleGetConfig(AsyncWebServerRequest *request)
{
    // read the config file in preparation of presentation in the web UI
    config_load();

    // serve the config page from flash
    request->send(*_fs, _config_page, "text/html", false);
}

static void handlePostConfig(AsyncWebServerRequest *request)
{
    // put all parameters in the JSON document
    for (size_t i = 0; i < request->params(); i++) {
        const AsyncWebParameter *param = request->getParam(i);
        String name = param->name();
        String value = param->value();
        _jsonDoc[name] = value;
    }

    // write the config file
    config_save();

    // redirect to page with values
    request->redirect(_config_path);
}

void config_begin(FS & fs, String config_name)
{
    _fs = &fs;
    _config_name = config_name; // e.g. "config.json"
}

void config_serve(AsyncWebServer & server, const char *config_path, const char *config_page)
{
    _config_path = config_path; // e.g. "/config"
    _config_page = config_page; // e.g "/config.html"

    // register ourselves with the server
    server.on(config_path, HTTP_GET, handleGetConfig);
    server.on(config_path, HTTP_POST, handlePostConfig);
}

bool config_load(void)
{
    File file = _fs->open(_config_name, "r");
    DeserializationError error = deserializeJson(_jsonDoc, file);
    file.close();
    return error == DeserializationError::Ok;
}

bool config_save(void)
{
    File file = _fs->open(_config_name, "w");
    size_t size = serializeJson(_jsonDoc, file);
    file.close();
    _version++;
    return size > 0;
}

void config_set_value(String propName, String propValue)
{
    _jsonDoc[propName] = propValue;
}

String config_get_value(String propName)
{
    return _jsonDoc[propName] | "";
}

int config_get_version(void)
{
    return _version;
}

