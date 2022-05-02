#!/bin/bash

script_dir=$(dirname $(realpath $0))

docker run -d \
  --restart always \
  --network burellixyz \
  --name mqtt \
  -p 8883:8883 \
  -v "${script_dir}/certs":/mosquitto/certs \
  -v "${script_dir}/mosquitto.conf":/mosquitto/config/mosquitto.conf \
  -v "${script_dir}/passfile":/mosquitto/config/passfile \
  eclipse-mosquitto
