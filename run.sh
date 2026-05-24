#!/bin/sh

INPUT=$1
INTERMEDIATE_OUTPUT_DIR=$2

./bin/program -i $INPUT -o $INTERMEDIATE_OUTPUT_DIR	\
	-d 0											\
	--fast9_threshold 40							\
	--dbscan_max_distance 3							\
	--dbscan_min_cluster_size 3						\
	--dbscan_min_clusters_count_merge 24			\
	--track_max_distance 30							\
	--track_max_missed 5							\
	--track_deviation_squared_threshold 8
