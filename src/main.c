#include <stdio.h>
#include "image_io.h"
#include "my_vector.h"

// Приклад обробки: малюємо сітку або ключові точки
void process_pixels(image_t* img, vector_t* keypoints) {
    // 1. Попіксельне затемнення (як у твоєму прикладі)
    for (int i = 0; i < img->width * img->height; i++) {
        img->pixels[i] = (uint8_t)(img->pixels[i] * 0.5f);
    }

    // 2. Малювання точок з вектора
    for (size_t i = 0; i < keypoints->size; i++) {
        pixel_coord_t* p = vector_get(keypoints, i);
        if (p->x < img->width && p->y < img->height) {
            // В Grayscale зображенні індекс це y * width + x
            img->pixels[p->y * img->width + p->x] = 255; 
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <input.png>\n", argv[0]);
        return -1;
    }

    // 1. Зчитування PNG
    image_t* my_img = image_load_gray(argv[1]);
    if (!my_img) {
        fprintf(stderr, "Could not load image\n");
        return -1;
    }

    // 2. Створення фейкових точок для тесту
    vector_t* pts = vector_create(sizeof(pixel_coord_t));
    for (uint16_t i = 0; i < 100; i++) {
        pixel_coord_t p = {i * 5, i * 5};
        vector_push_back(pts, &p);
    }

    // 3. Обробка
    process_pixels(my_img, pts);

    // 4. Вивід у файл
    if (image_save_png("output.png", my_img) == OK) {
        printf("Success! Saved to output.png\n");
    } else {
        printf("Failed to save image\n");
    }

    // 5. Очищення
    vector_destroy(pts);
    image_free(my_img);

    return 0;
}