#!/bin/bash

unique_id="$object_id-xjdhw"

topic_config="homeassistant/camera/$object_id/config"
topic="$prefix/camera/$object_id/image"

payload="{\"topic\": \"$topic\",\"unique_id\":\"$unique_id\"}"

mosquitto_pub --retain --id $mqtt_client_id --url "$mqtt_url/$topic_config" -m "$payload"
