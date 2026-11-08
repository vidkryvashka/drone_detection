#!/bin/sh

if [ -z "$1" ]; then
	printf "\033[0;31m%s\033[0;0m\n" "wrong usage"
	echo "./run.sh [input dir] [optional output dir] [dbg lvl {0 1 2}]"
	exit 0
fi

INPUT=$1
INTERMEDIATE_OUTPUT_DIR=${2:-output}
mkdir -p $INTERMEDIATE_OUTPUT_DIR

DBG_LVL=${3:-0}

./bin/program -c config.toml                                       \
              -i "$INPUT"                                          \
              -o "$INTERMEDIATE_OUTPUT_DIR"                        \
              --dbg_lvl $DBG_LVL \
              --output_video "$INTERMEDIATE_OUTPUT_DIR/vidos.mp4"
