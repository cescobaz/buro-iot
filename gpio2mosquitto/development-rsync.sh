#!/bin/bash

script_dir=$(dirname $(realpath $0))

file_pattern=${script_dir}/

rsync -vt $file_pattern pi@raspi-1:/opt/buro-iot/gpio2mosquitto/

fswatch $file_pattern -I --exclude '.*' --include '\.c$' --include '\.h$' --include '\.sh' --include '\.service' |
  xargs -I % rsync -vt % pi@raspi-1:/opt/buro-iot/gpio2mosquitto/
