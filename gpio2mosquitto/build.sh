#!/bin/bash

set -e

src_path=$(dirname $(realpath $0))

gcc -O3 -o "$src_path/gpio2mosquitto" \
  "$src_path/gpio2mosquitto.c" \
  "$src_path/main.c" \
  -pthread -lpigpio -lrt -lmosquitto -ljson-c

