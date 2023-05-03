#!/bin/bash

export select_object_id="$object_id-state"
topic_config="homeassistant/select/$select_object_id/config"

export command_topic="$prefix/select/$select_object_id/commands"
export state_topic="$prefix/select/$select_object_id/state"
