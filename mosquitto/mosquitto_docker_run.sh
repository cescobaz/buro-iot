#!/bin/bash

docker run -d \
  --restart always \
  --network burellixyz \
  --name mqtt \
  -p 1883:1883 \
  -v "$(pwd)/certs":'/mosquitto/certs' \
  -v "$(pwd)/mosquitto.conf":/mosquitto/config/mosquitto.conf \
  -v "$(pwd)/passfile":/mosquitto/config/passfile \
  eclipse-mosquitto
