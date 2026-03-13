#!/bin/sh
#export DOCKER_DEFAULT_PLATFORM=linux/amd64
#export DOCKER_NETWORK="--network=host"

# first clean images older than 24h, you will run out of disk space one day
docker image prune -a --force --filter "until=24h"


IMAGE_NAME="robzyl"
rm "${PWD}/compiled-firmware/*"
echo "Building docker image $IMAGE_NAME"
if ! docker build -t $DOCKER_NETWORK $IMAGE_NAME .
then
    echo "Failed to build docker image"
    exit 1
fi

PL() {
    echo "PL compilation..."
    docker run --rm -v "${PWD}/compiled-firmware:/app/compiled-firmware" $IMAGE_NAME /bin/bash -c "cd /app && make -s \
        ENABLE_FULL_BAND=0 \
        ENABLE_PL_BAND=1 \
        TARGET=robzyl.pl \
        && cp robzyl.pl.packed* compiled-firmware/"
}

FR() {
    echo "FR compilation..."
    docker run --rm -v "${PWD}/compiled-firmware:/app/compiled-firmware" $IMAGE_NAME /bin/bash -c "cd /app && make -s \
        ENABLE_FULL_BAND=1 \
        ENABLE_PL_BAND=0 \
        TARGET=robzyl.fr \
        && cp robzyl.fr.packed* compiled-firmware/"
}





case "$1" in
    FR)
        FR
        ;;
    PL)
        PL
        ;;
    all)
        PL
        FR
        ;;
    *)
        echo "Usage: $0 {FR|PL|all}"
        exit 1
        ;;
esac