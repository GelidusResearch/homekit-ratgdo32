#!/usr/bin/env python3
#
# This script runs patch command on specified files.
#
# Copyright (c) 2023 David Kerr, https://github.com/dkerr64
#
import os

home_dir = os.environ.get('HOME') or os.environ.get('USERPROFILE')

os.system("patch -N " + home_dir + "/.platformio/packages/framework-arduinoespressif32/libraries/WebServer/src/WebServer.cpp url_not_found_log.patch")