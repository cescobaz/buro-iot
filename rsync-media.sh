#!/bin/bash

mkdir -p ./data/media

rsync -avz --progress root@burelli.xyz:buro-iot/nodered/data/media/ ./data/media/
