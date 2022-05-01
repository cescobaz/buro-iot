#!/bin/bash

set -e

src_path=$(dirname $(realpath $0))

gcc -O3 -o "$src_path/gpio2mosquitto" "$src_path/gpio2mosquitto.c" -pthread -lpigpio -lmosquitto
