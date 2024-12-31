#!/usr/bin/env python3

import subprocess

def get_git_version():
    """Retrieve the current version from `git describe --tags`."""
    try:
        # Run the git command to get the version string
        version = subprocess.check_output(
            ["git", "describe", "--tags", "--dirty", "--always"],
            stderr=subprocess.STDOUT,
            text=True
        ).strip()
        return version
    except subprocess.CalledProcessError:
        # Return a default value if the command fails
        return "unknown"

def generate_version_header(output_file):
    """Generate the version.h file with the Git version."""
    git_version = get_git_version()
    with open(output_file, "w") as header:
        header.write('// automatically generated using create_fwversion.py\n')
        header.write(f'const char* FW_VERSION = "{git_version}";\n\n')
    print(f"Generated C file with with GIT_VERSION={git_version}: {output_file}")

# Create the version file
output_file = "fwversion.h"
generate_version_header(output_file)
