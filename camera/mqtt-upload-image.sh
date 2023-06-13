#!/bin/bash

set -e

topic="$prefix/camera/$object_id/image"

echo "INFO: taking picture and send to topic $topic ..."

if ! type 'libcamera-still' > /dev/null; then
  snapshot_command='raspistill'
else
  snapshot_command='libcamera-still --immediate --flush 1 --rawfull'
fi

echo "INFO: using $snapshot_command"

$snapshot_command --nopreview -o - |
  mosquitto_pub --id $mqtt_client_id --url "$mqtt_url/$topic" --stdin-file

echo "INFO:[$(date)] picture taken, updating state via $state_topic"

mosquitto_pub --id $mqtt_client_id --url "$mqtt_url/$state_topic" -m 'ready'
