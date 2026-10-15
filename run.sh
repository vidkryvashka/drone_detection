#!/bin/sh

if [ -z "$1" ]; then
	printf "\033[0;31m%s\033[0;0m\n" "wrong usage"
	echo "./run.sh [input dir] [optional output dir]"
	exit 0
fi

INPUT=$1
INTERMEDIATE_OUTPUT_DIR=${2:-output}
mkdir -p $INTERMEDIATE_OUTPUT_DIR

DBG_LVL=${3:-0}

./bin/program -i "$INPUT"							\
	-o "$INTERMEDIATE_OUTPUT_DIR"					\
	--dbg_lvl $DBG_LVL								\
	-d 0											\
	--fast9_threshold 40							\
	--dbscan_max_distance 3							\
	--dbscan_min_cluster_size 3						\
	--dbscan_min_clusters_count_merge 24			\
	--track_max_distance 30							\
	--track_max_missed 5							\
	--track_deviation_squared_threshold 8			\
	--output_video $INTERMEDIATE_OUTPUT_DIR/vidos.mp4
