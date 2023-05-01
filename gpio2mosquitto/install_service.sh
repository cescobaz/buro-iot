#!/bin/sh

set -e

script_dir=$(dirname $(realpath $0))

sudo cp "${script_dir}/gpio2mosquitto.service" /etc/systemd/system/gpio2mosquitto.service
