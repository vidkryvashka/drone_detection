# video stream drone detection

## compile:
```sh
bear -- make # helps to setup clang lints
```

## use
```
-h --help
-i --input                              path to image file or directory containing images path, default in Makefile
-i --output_dir                         path to folder for frames output. video goes to default "output" folder
-d --dim_coef                           0 - 16 value, 0 is black img output, points only, default 2
   --output_video                       path to video saving location, default output/dildo.mp4

   --fast9_threshold                    default 40

   --dbscan_max_distance                max 2D distance (img diagonal percent) between points to attribute the point to the cluster, default 4
   --dbscan_min_cluster_size            min points number in cluster, default 3
   --dbscan_min_clusters_count_merge	min cluster count to recursively merge some of them reducing count, dafault 24
   --dbscan_enable_geometric_filtering  no arg. may be excessive for optical flow calculate, default off

   --track_max_distance                 the maximum distance in pixels that an object can move in 1 frame, default 30
   --track_max_missed                   how many frames to wait before deleting a lost track, default 5
   --track_deviation_squared_threshold  to find point with too deviative trajectory, default 8

   --dbg_lvl                            0 | 1 | 2
```
examples:
```sh
$ ./bin -i expl.jpg --fast9_threshold 70
$ ./bin expl.png

$ ./bin -i $INPUT -o $INTERMEDIATE_OUTPUT_DIR \
	--fast9_threshold 40                        \
	--dbscan_max_distance 4                     \
	--dbscan_min_cluster_size 3                 \
	--dbscan_min_clusters_count_merge 16        \
	--track_max_distance 50                     \
	--track_max_missed 5                        \
	--track_deviation_squared_threshold 15
	# -- dbscan_enable_geometric_filtering
```

## After processing examples. The red dot is designated as a target.
![small gif of FPV processing](./assets/fpv.gif)
![larger gif with bomber, pls wait](./assets/bober.gif)
<!-- ![](./assets/output_frame_example.jpg) -->

Folder with images should contain numbered filenames aka 1.jpg, 2.jpg, ...

Then ffmpeg glues video in output folder nearby binary.


## dependencies for loading & saving (io) image
single file libs in include/foreign/

	- stb_image.h
	- stb_image_write.h
	- ffmpeg installed
