#!/bin/bash

ps aux |
  grep libcamera-vid |
  grep -v grep |
  awk '{ print $2}' |
  xargs kill

echo "INFO:[$(date)] streaming stopped"

mosquitto_pub --id $mqtt_client_id --url "$mqtt_url/$state_topic" -m 'ready'
