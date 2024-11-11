#include <stdbool.h>

#include <FS.h>
#include <ESPAsyncWebServer.h>

void config_begin(FS &fs, String config_name);

bool config_load(void);
bool config_save(void);

void config_serve(AsyncWebServer &server, const char *config_path, const char *config_page);

void config_set_value(String propName, String propValue);
String config_get_value(String propName);

int config_get_version(void);
