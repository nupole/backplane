#!/bin/bash

device="backplane"

insmod ../src/$device.ko
major=$(awk -v mod="$device" '$2==mod{print $1}' /proc/devices)
mknod /dev/${device} c $major 0
chown nupole: /dev/${device}
chmod u+w /dev/${device}
