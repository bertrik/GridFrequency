#include <stdbool.h>
#include <stddef.h>
#include <FS.h>

typedef struct {
    const char *filename;
    const unsigned char *data;
    size_t length;
} fsimage_entry_t;

void fsimage_unpack(FS &fs, bool force);

// reference to the file table in the generated fsimage_data.cpp file
extern const fsimage_entry_t fsimage_table[];

