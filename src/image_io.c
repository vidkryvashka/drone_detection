#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "image_io.h"

image_t* image_load_gray(const char* filename) {
    image_t* img = malloc(sizeof(image_t));
    // Примусово завантажуємо як 1 канал (Grayscale)
    img->pixels = stbi_load(filename, &img->width, &img->height, &img->channels, 1);
    img->channels = 1; 
    
    if (!img->pixels) {
        free(img);
        return NULL;
    }
    return img;
}

errno_t image_save_png(const char* filename, const image_t* img) {
    if (!img || !img->pixels) return EINVAL;
    // 0 в кінці — це stride (автоматично width * channels)
    if (stbi_write_png(filename, img->width, img->height, img->channels, img->pixels, 0)) {
        return OK;
    }
    return 1;
}

void image_free(image_t* img) {
    if (img) {
        stbi_image_free(img->pixels);
        free(img);
    }
}