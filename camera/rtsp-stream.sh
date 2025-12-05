#!/bin/bash

stream() {
  libcamera-vid -n -t 0 --flush 1 -o - |
    ffmpeg -i - -vcodec copy -an -f rtsp -rtsp_transport tcp rtsp://{{ rtsp_server_host_public }}:{{ rtsp_server_port }}/$object_id
}

stream &

echo "INFO:[$(date)] streaming started"

mosquitto_pub --retain --id $mqtt_client_id --url "$mqtt_url/$state_topic" -m 'streaming'
