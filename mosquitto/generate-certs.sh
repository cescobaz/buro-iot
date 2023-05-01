#!/bin/bash

docker run --rm -it \
  -v /root/edge/data/letsencrypt.json:/acme.json \
  -v /root/buro-iot/mosquitto/certs:/target \
  ldez/traefik-certs-dumper:latest file --version v2 --domain-subdir --crt-ext=.pem --key-ext=.pem --source /acme.json --dest /target
