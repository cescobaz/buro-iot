#!/bin/sh

mqtt_host=192.168.1.102
mqtt_username=buro
mqtt_password=ciao
mqtt_topic=prova

if [ "$#" -lt 1 ]; then
  echo "Usage $0 GPIOX [GPIOY] [GPIOZ] ... [GPION]"
  exit 1
fi

bitmask=0
for arg do
  bitmask=$((1 << $arg | $bitmask))
done
bitmask=$(printf '%X' $bitmask)
echo "bitmask: $bitmask"

handle=$(pigs no)
pigs nb $handle "0x$bitmask"
notifications_pipe="/dev/pigpio$handle"

cat $notifications_pipe |
  ./pig2log/pig2log "0x$bitmask" |
  stdbuf -i0 -o0 sed 's/\([0-9]\+\) \([0-9]\+\)/{"gpio":\1,"value":\2}/' |
  xargs -I M bash -c "mosquitto_pub -h "$mqtt_host" -t "$mqtt_topic" -u "$mqtt_username" -P "$mqtt_password" -m 'M'"

pigs nc $handle
