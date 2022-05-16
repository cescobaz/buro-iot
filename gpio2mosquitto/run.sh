#!/bin/sh

set -e

script_dir=$(dirname $(realpath $0))

sudo ${script_dir}/gpio2mosquitto \
  --username 'buro' --device-name "$(hostname)" \
  --mqtt-host 'burelli.xyz' --mqtt-port 8883 \
  --mqtt-username "buro.$(hostname)" \
  --mqtt-password "$(cat ${script_dir}/mqtt_pass)" \
  --gpio-modes-file "${script_dir}/gpio_modes"
