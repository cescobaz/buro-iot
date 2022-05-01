#!/bin/bash

set -e

src_path=$(dirname $(realpath $0))

gcc -o "$src_path/pig2log" "$src_path/pig2log.c" -pthread
