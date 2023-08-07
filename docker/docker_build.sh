#!/bin/bash

set -e
NEW_UUID=ns3_tlt_rdma_docker_prepare
timestamp=$(date '+%Y%m%d.%H%M%S')
TARGET_DIR=/tmp/$NEW_UUID
TAG_NAME=docker.ina.kaist.ac.kr/ns3-tlt-rdma-experiment
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

mkdir -p $TARGET_DIR

cp $DIR/Dockerfile $TARGET_DIR
cp $DIR/main.py $TARGET_DIR
cp $DIR/*.config $TARGET_DIR
cd $TARGET_DIR

rsync -a --exclude=.git --exclude=.conf_* --exclude=c4che --exclude=bindings --exclude=utils --exclude=src --exclude=*.o --exclude=ns3 --exclude= $DIR/../build .
rsync -a $DIR/../config .
rsync -a $DIR/../workloads .
rsync -a $DIR/../mix .

docker build -t $TAG_NAME:$timestamp .
docker push $TAG_NAME:$timestamp
docker tag $TAG_NAME:$timestamp $TAG_NAME:latest
docker push $TAG_NAME:latest