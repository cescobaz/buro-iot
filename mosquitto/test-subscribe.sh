#!/bin/bash

set -e

mqtt_host=mqtt.burelli.xyz
mqtt_port=8883
mqtt_username=buro.raspi-2
mqtt_password=RoqfKVeJjoV8baTFEE4b6PSqbtFG6KgfwgUitbpGkZmWn67BZDqb2fbytwD964g4
mqtt_client_id=76EE3F37-3258-431A-BDEA-6856625CA9F0
mqtt_topic=test-topic

echo "topic: $mqtt_topic"

mosquitto_sub --insecure -v \
  -h "$mqtt_host" -p $mqtt_port \
  -u "$mqtt_username" -P "$mqtt_password" \
  -i "$mqtt_client_id" \
  -t "$mqtt_topic"
