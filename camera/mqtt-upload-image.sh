#!/bin/bash

set -e

topic="$prefix/camera/$object_id/image"

echo "INFO: taking picture and send to topic $topic ..."

libcamera-still --nopreview --immediate --flush 1 --rawfull -o - |
  mosquitto_pub --id $mqtt_client_id --url "$mqtt_url/$topic" --stdin-file

echo "INFO:[$(date)] picture taken, updating state via $state_topic"

mosquitto_pub --id $mqtt_client_id --url "$mqtt_url/$state_topic" -m 'ready'
