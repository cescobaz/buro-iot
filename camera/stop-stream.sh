#!/bin/bash

ps aux |
  grep libcamera-vid |
  grep -v grep |
  awk '{ print $2}' |
  xargs -r kill

echo "INFO:[$(date)] streaming stopped"
