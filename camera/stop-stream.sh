#!/bin/bash

ps aux |
  grep libcamera-vid |
  grep -v grep |
  awk '{ print $2}' |
  xargs kill

echo "INFO:[$(date)] streaming stopped"
