#!/bin/bash

set -e

payload="{\"command_topic\": \"$command_topic\",\"state_topic\":\"$state_topic\",\"default_entity_id\":\"select.$select_object_id\",\"unique_id\":\"$select_object_id\",\"name\":\"$entity_name state\",\"options\":[\"ready\", \"snapshot\", \"streaming\"]}"

mosquitto_pub --retain --id $mqtt_client_id --url "$mqtt_url/$topic_config" -m "$payload"

echo "INFO:[$(date)] select device config \"$payload\", topic: $topic_config, command_topic: $command_topic"
