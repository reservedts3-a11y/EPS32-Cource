# Прошиває тільки ту плату, яка зараз підключена.
# Тип плати визначається запитом до самого чипа (через esptool),
# тому неважливо, через який USB-роз'єм / перехідник вона підключена.
import os
import re
import subprocess
Import("env")
from serial.tools import list_ports

CHIPS = {
    "esp32dev": "ESP32",
    "esp32s3": "ESP32-S3",
}
# Вбудований USB ESP32-S3 (VID 0x303A) - якщо чип не відповідає на запит,
# вважаємо, що це S3
NATIVE_USB_VID = 0x303A


def detect_chip(port):
    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")
    try:
        # chip_id сам перезавантажує плату назад у звичайний режим
        out = subprocess.run(
            [env.subst("$PYTHONEXE"), esptool, "--port", port, "--connect-attempts", "2", "chip_id"],
            capture_output=True, text=True, timeout=30,
        ).stdout
    except Exception:
        return None
    match = re.search(r"Detecting chip type\.*\s*(\S+)", out)
    return match.group(1) if match else None


def find_port(pioenv):
    for p in list_ports.comports():
        if p.vid is None:  # Bluetooth та інші не-USB порти
            continue
        chip = detect_chip(p.device)
        if chip == CHIPS.get(pioenv):
            return p.device
        if chip is None and p.vid == NATIVE_USB_VID and pioenv == "esp32s3":
            return p.device
    return None


if "upload" in COMMAND_LINE_TARGETS:
    port = find_port(env["PIOENV"])
    if port:
        env.Replace(UPLOAD_PORT=port)
    else:
        msg = "Плата %s не підключена - пропускаю" % env["PIOENV"]
        env.Replace(UPLOAD_PORT="none", UPLOADCMD='"$PYTHONEXE" -c "print(\'%s\')"' % msg)
