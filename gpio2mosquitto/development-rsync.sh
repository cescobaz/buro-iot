#!/bin/bash

script_dir=$(dirname $(realpath $0))

file_pattern=${script_dir}/*.c

rsync -vt $file_pattern pi@raspi-1:/opt/buro-iot/gpio2mosquitto/

fswatch $file_pattern |
  xargs -I % rsync -vt % pi@raspi-1:/opt/buro-iot/gpio2mosquitto/
