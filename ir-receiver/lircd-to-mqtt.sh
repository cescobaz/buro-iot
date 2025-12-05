#!/bin/sh

set -e

MQTT_HOST={{ mqtt_host }}
MQTT_PORT={{ mqtt_port }}
MQTT_USERNAME={{ mqtt_user }}
MQTT_PASSWORD={{ mqtt_password }}
MQTT_CLIENT_ID={{ mqtt_client_id }}
MQTT_TOPIC=ir-receiver

sudo irw |
  stdbuf -i0 -o0 awk -W interactive '{ print $3 }' |
  xargs -I M bash -c "mosquitto_pub -h "$MQTT_HOST" -p "$MQTT_PORT" -t "$MQTT_TOPIC" -u "$MQTT_USERNAME" -P "$MQTT_PASSWORD" -m 'M'"
