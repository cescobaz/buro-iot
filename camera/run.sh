#!/bin/bash

dir=$(dirname $(realpath $0))

set -ae

source $dir/mqtt-camera-state-env.sh

$dir/mqtt-camera.sh
$dir/mqtt-camera-state.sh

while true; do
  echo "INFO:[$(date)] listening commands from command_topic: $command_topic"
  command=$(mosquitto_sub --id $mqtt_client_id --url "$mqtt_url/$command_topic" -C 1)
  echo "INFO:[$(date)] received command: $command, from topic: $command_topic"
  case $command in
  snapshot)
    $dir/stop-stream.sh
    mosquitto_pub --id $mqtt_client_id --url "$mqtt_url/$state_topic" -m $command
    $dir/mqtt-upload-image.sh
    ;;
  streaming)
    $dir/rtsp-stream.sh
    ;;
  ready)
    $dir/stop-stream.sh
    ;;
  *)
    echo "ERROR: unknown command $command"
    ;;
  esac
done
