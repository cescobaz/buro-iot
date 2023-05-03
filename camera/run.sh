#!/bin/bash

dir=$(dirname $(realpath $0))

set -ae

source $dir/mqtt-camera-state-env.sh

$dir/mqtt-camera.sh
$dir/mqtt-camera-state.sh

update_state() {
  state=$1
  mosquitto_pub --id $mqtt_client_id --url "$mqtt_url/$state_topic" -m "$state"
}

$dir/stop-stream.sh
update_state 'ready'

while true; do
  echo "INFO:[$(date)] listening commands from command_topic: $command_topic"
  command=$(mosquitto_sub --id $mqtt_client_id --url "$mqtt_url/$command_topic" -C 1)
  echo "INFO:[$(date)] received command: $command, from topic: $command_topic"

  case $command in
  snapshot)
    update_state $command
    $dir/stop-stream.sh
    $dir/mqtt-upload-image.sh
    update_state 'ready'
    ;;
  streaming)
    $dir/rtsp-stream.sh
    ;;
  ready)
    $dir/stop-stream.sh
    update_state 'ready'
    ;;
  *)
    echo "ERROR: unknown command $command"
    ;;
  esac
done
