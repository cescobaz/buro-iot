#!/bin/sh

set -x

mqtt_host=192.168.1.102
mqtt_username=buro
mqtt_password=ciao
mqtt_topic=prova

handle=$(pigs no)
bitmask=$(printf '%X' $((1 << 27)))
pigs nb $handle "0x$bitmask"
notifications_pipe="/dev/pigpio$handle"

cat $notifications_pipe |
  ./pig2json |
  stdbuf -i0 -o0 sed 's/\([0-9]\+\) \([0-9]\+\)/{"gpio":\1,"value":\2}/' |
  xargs -I M bash -c "mosquitto_pub -h "$mqtt_host" -t "$mqtt_topic" -u "$mqtt_username" -P "$mqtt_password" -m 'M'"

pigs nc $handle
