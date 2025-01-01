#include <FS.h>
#include <ESPAsyncWebServer.h>

void fwupdate_begin(FS & fs);
void fwupdate_serve(AsyncWebServer &server, const char *update_path, const char *update_page);
void fwupdate_loop(void);

