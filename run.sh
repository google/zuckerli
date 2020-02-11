#!/bin/bash -e

source gbash.sh || exit 1


INPUT=$1
shift

OUTPUT_DIR=$1
shift

LOCAL_DIR=$1
shift


cleanup() {
  rm -rf $LOCAL_DIR/*
}

trap cleanup EXIT

ENCODER="$RUNFILES/google3/third_party/zuckerli/encoder"
DECODER="$RUNFILES/google3/third_party/zuckerli/decoder"

fileutil cp $INPUT $LOCAL_DIR/input.bin

$ENCODER --input_path $LOCAL_DIR/input.bin --output_path $LOCAL_DIR/compr.hc $* 2>&1 | grep -o Compressed.* > $LOCAL_DIR/enc.log
/usr/bin/time $DECODER --undefok allow_random_access --input_path $LOCAL_DIR/compr.hc $* &> $LOCAL_DIR/dec.log

fileutil mkdir -p $OUTPUT_DIR
fileutil cp -f $LOCAL_DIR/{enc.log,dec.log,compr.hc} $OUTPUT_DIR/


