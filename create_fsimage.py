#!/usr/bin/env python3

import os

def create_c_header(directory, output_file):
    print(f"Creating C file with file contents: {output_file}")
    with open(output_file, 'w') as header:
        header.write(f"// Generated using create_fsimage.py from contents of folder '{directory}'\n\n")
        header.write('#include <Arduino.h>\n')
        header.write('#include "fsimage.h"\n\n')

        # Iterate over all files in the directory
        entries = ""
        for filename in os.listdir(directory):
            filepath = os.path.join(directory, filename)

            # Read file as a byte array
            with open(filepath, 'rb') as file:
                byte_array = file.read()

            # Write the byte array to the header file
            identifier = filename.replace('.', '_').replace('-', '_')
            header.write(f"// Byte array for {filename}\n")
            header.write(f"const unsigned char {identifier}[] PROGMEM = {{\n")
            for i in range(0, len(byte_array), 16):
                chunk = byte_array[i:i+16]
                hex_bytes = ', '.join(f'0x{byte:02x}' for byte in chunk)
                header.write(f"    {hex_bytes},\n")
            header.write("};\n\n")

            # create entry for table
            entries += f'    {{ "/{filename}", {identifier}, sizeof({identifier}) }},\n'

        # write table with file entries
        header.write("const fsimage_entry_t fsimage_table[] = {\n")
        header.write(entries)
        header.write('    {"", NULL, 0}\n')
        header.write('};\n\n')


# Create the data file
create_c_header("data", "fsimage_data.cpp")

