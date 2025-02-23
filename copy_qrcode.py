import shutil
import os

source = os.path.join(".pio", "libdeps", "ratgdo_esp32dev", "qrcodejs", "qrcode.js")
destination = os.path.join("src", "www", "qrcode.js")

shutil.copyfile(source, destination)
print(f"Copied {source} to {destination}")