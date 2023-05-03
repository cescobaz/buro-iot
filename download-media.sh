#!/bin/bash

dir=$(dirname $(realpath $0))

media_dir=$dir/media
mkdir -p "$media_dir"

scp 'buro@arch-macbook:dw-setup/home-assistant/media/*' "$media_dir/"
