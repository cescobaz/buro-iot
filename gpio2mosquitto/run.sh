#!/bin/sh

set -e

script_dir=$(dirname $(realpath $0))

sudo ${script_dir}/gpio2mosquitto \
	--host 'burelli.xyz' --port 8883 \
	--username "buro.$(hostname)" --password "$(cat ${script_dir}/mqtt_pass)" \
	--gpio-input 27
