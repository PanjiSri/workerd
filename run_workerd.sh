#!/bin/bash

if [ -z "$1" ]; then
    echo "./run_workerd.sh custom_samples/helloworld/myconfig.capnp"
    exit 1
fi

CONFIG_PATH="$1"

if [ ! -f "$CONFIG_PATH" ]; then
    echo "File '$CONFIG_PATH' not found"
    exit 1
fi

# echo "Konfigurasi: $CONFIG_PATH"
./bazel-bin/src/workerd/server/workerd serve "$CONFIG_PATH"
