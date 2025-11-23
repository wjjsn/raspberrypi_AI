#!/bin/bash

PI_USER="wjjsn"
PI_HOST="raspberrypi.local"
SRC_ROOT="/mnt/g/code/raspberrypi/sysroot/usr"

function sync_dir() {
    local SRC=$1
    local DEST=$2

    rsync -avz \
        -e ssh \
        --rsync-path="sudo rsync" \
        "$SRC" \
        ${PI_USER}@${PI_HOST}:"$DEST"
}

echo "同步 include..."
sync_dir "${SRC_ROOT}/include/" "/usr/include/"

echo "同步 lib..."
sync_dir "${SRC_ROOT}/lib/" "/usr/lib/"

echo "同步 share..."
sync_dir "${SRC_ROOT}/share/" "/usr/share/"

echo "完成！"
