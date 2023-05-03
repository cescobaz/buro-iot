#!/bin/bash

unique_id="$object_id"

topic_config="homeassistant/camera/$object_id/config"
topic="$prefix/camera/$object_id/image"

payload="{\"topic\": \"$topic\",\"object_id\":\"$object_id\",\"unique_id\":\"$unique_id\",\"name\":\"$entity_name\"}"

echo "INFO:[$(date)] register camera at topic $topic_config with payload $payload"

mosquitto_pub --retain --id $mqtt_client_id --url "$mqtt_url/$topic_config" -m "$payload"
