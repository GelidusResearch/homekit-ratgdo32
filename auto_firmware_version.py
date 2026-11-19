import subprocess
import json
import os.path

Import("env")

def get_firmware_specifier_build_flag(tag = ""):
    # Installer manifests describe published assets; VERSION describes this source.
    with open('VERSION', encoding='utf-8') as version_file:
        build_version = version_file.read().strip()
    build_flag = "-D AUTO_VERSION=\\\"" + build_version + tag +"\\\""
    print ("Firmware Version: " + build_version + tag)
    return (build_flag)


# look for "-D VERSION_TAG=" in build flags
result = next((bflags for bflags in env.get("BUILD_FLAGS") if "-D VERSION_TAG=" in bflags), None)
verTag = ""
if result:
    # create version tag "-xxx"
    verTag = "-" + result.split("=")[1].strip('"')

# set AUTO_VERSION build flag with optional version tag appended
env.Append(BUILD_FLAGS=[get_firmware_specifier_build_flag(verTag)])
