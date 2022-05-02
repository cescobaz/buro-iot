#!/bin/sh

set -e

script_dir=$(dirname $(realpath $0))

${script_dir}/gpio2mosquitto \
	--host 'burelli.xyz' --port 8883 \
	--username "$(hostname)" --password "$(cat ${script_dir}/mqtt_pass)" \
	--gpio-input 27
