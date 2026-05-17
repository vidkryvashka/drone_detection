# course work drone detection

## compile:
```sh
bear -- make # helps to setup clang lints
```

## use
```
-i --input <char *filename>	image file path, default in Makefile
-i --output_dir <char *output_dir>	folder for outputs
-t --threshold <uint8_t>	fast9 threshold, 	default 70
-d --dim-coef <uint8_t>		0 - 12 value, 0 is black img output, points only, default 3
-h --help
```
examples:
```sh
$ binary -i expl.png -t 70 -d 3
$ binary expl.png
```

For now main case is to have a folder INPUT_DIR with images aka 1.jpg, 2.jpg, ...
```sh
make custom_run INPUT_DIR=$INPUT_DIR FLAGS="-t 80 -d 0" OUTPUT_DIR=$output_DIR OUTPUT_VIDEONAME="../expl.mp4"
```

## dependencies
single file libs in include/foreign/

	- stb_image.h
	- stb_image_write.h
