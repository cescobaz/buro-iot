#!/bin/bash

stream() {
  libcamera-vid -n -t 0 --flush 1 -o - |
    ffmpeg -i - -vcodec copy -an -f rtsp -rtsp_transport tcp rtsp://arch-macbook:8554/$object_id
}

stream &

echo "INFO:[$(date)] streaming started"

mosquitto_pub --id $mqtt_client_id --url "$mqtt_url/$state_topic" -m 'streaming'
