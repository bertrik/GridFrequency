#include <stdbool.h>
#include <Arduino.h>
#include "fsimage.h"

#define printf Serial.printf

static void unpack_file(FS &fs, const fsimage_entry_t *entry)
{
    uint8_t buf[1024];
    File file = fs.open(entry->filename, "w");
    if (file) {
        const unsigned char *p = entry->data;
        size_t remain = entry->length;
        for (size_t block; remain > 0; remain -= block, p += block) {
            block = remain > sizeof(buf) ? sizeof(buf) : remain;
            memcpy_P(buf, p, block);
            file.write(buf, block);
        }
        file.close();
    }
}

static bool verify_contents(File & file, const fsimage_entry_t *entry)
{
    uint8_t buf[1024];
    const uint8_t *p = entry->data;
    size_t remain = entry->length;
    for (size_t block; remain > 0; remain -= block, p += block) {
        block = remain > sizeof(buf) ? sizeof(buf) : remain;
        file.read(buf, block);
        if (memcmp_P(buf, p, block) != 0) {
            return false;
        }
    }
    return true;
}

// returns true if file contents are equal to data from file entry
static bool verify_file(FS & fs, const fsimage_entry_t *entry)
{
    bool result = false;

    // check existence
    File file = fs.open(entry->filename, "r");
    if (file) {
        result = (file.size() == entry->length) && verify_contents(file, entry);
        file.close();
    }
    return result;
}

void fsimage_unpack(FS & fs, bool force)
{
    printf("Unpacking files...\n");
    for (const fsimage_entry_t * entry = fsimage_table; *entry->filename; entry++) {
        printf(" * %12s: ", entry ->filename);
        if (force || !verify_file(fs, entry)) {
            printf("writing %d bytes ...", entry->length);
            unpack_file(fs, entry);
            printf("done\n");
        } else {
            printf("skipped\n");
        }
    }
    printf("All files unpacked\n");
}

