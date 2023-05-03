#!/bin/bash

dir=$(dirname $(realpath $0))

media_dir=$dir/media
mkdir -p "$media_dir"

rsync -v 'buro@arch-macbook:/home/buro/dw-setup/nodered/data/media/*' "$media_dir/"
