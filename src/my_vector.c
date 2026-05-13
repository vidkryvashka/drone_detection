#include <stdio.h>

#include "my_vector.h"

#define INIT_CAPACITY 4


vector_t* vector_create(
    size_t sizeof_element
) {
    vector_t* vec = (vector_t *)malloc(sizeof(vector_t));
    if (!vec) return NULL;
    
    vec->data = calloc(INIT_CAPACITY, sizeof_element);
    vec->size = 0;
    vec->capacity = INIT_CAPACITY;
    vec->sizeof_element = sizeof_element;

    return vec;
}


errno_t vector_reserve(
    vector_t *vec,
    const size_t new_capacity
) {
    if (!vec) {
        printf("vector_reserve !vec\n");
        return EINVAL;
    }

    if (new_capacity <= vec->capacity) {
        printf("vector_reserve expected bigger capacity, not <=. new_cap: %ld curr: %ld\n", new_capacity, vec->capacity);
        return EINVAL;
    }
    
    void *new_data = realloc(vec->data, new_capacity * vec->sizeof_element);
    if (!new_data) {
        printf("vector_reserve realloc lost data\n");
        return ENOSPC;
    }
    
    vec->data = new_data;           // looks like memory safe
    vec->capacity = new_capacity;

    return OK;
}


errno_t vector_push_back(
    vector_t *vec,
    const void *element
) {
    if (!vec || !element) {
        printf("vector_push_back (!vec || !element)\n");
        return EINVAL;
    }
    
    if (vec->size >= vec->capacity) {
        size_t new_capacity = (vec->capacity == 0) ? 8 : vec->capacity * 1.4f;
        vector_reserve(vec, new_capacity);
    }
    
    void *dest = (char*)vec->data + vec->size * vec->sizeof_element;
    memcpy(dest, element, vec->sizeof_element);
    vec->size++;

    return OK;
}


void* vector_get(
    const vector_t *vec,
    const size_t index
) {
    if (!vec || index >= vec->size) return NULL;
        return (char*)vec->data + index * vec->sizeof_element;
}


errno_t vector_set(vector_t *vec, const size_t index, void *val)
{
    if (index >= vec->capacity)
        vector_reserve(vec, index + 2);

    void *dest = (char*)vec->data + index * vec->sizeof_element;
    if (!memcpy(dest, val, vec->sizeof_element)) {
        printf("failed to set element in vec, sizeof_element %ld\n", vec->sizeof_element);
        return ENOSPC;
    }
    if (index >= vec->size)
        vec->size = index + 1;
    
    return OK;
}


// errno_t vector_print(
//     const vector_t *vec
// ) {
// 
//     switch (vec->sizeof_element) {
//         
//         case sizeof(uint8_t):           // 1
//         case sizeof(uint16_t):          // 2
//             printf("uintX_t vec size: %ld cap: %ld { ", vec->size, vec->capacity);
//             for (size_t i = 0; i < vec->size; ++i)
//                 printf("%d ", *(uint16_t*)vector_get(vec, i));
//             printf("}\n");
//             break;
// 
//         // case sizeof(pixel_coord_t):     // 4 like int, int32
//         //     printf("pixel_coord_t vec size: %ld cap: %ld { ", vec->size, vec->capacity);
//         //     for (size_t i = 0; i < vec->size; ++i)
//         //         printf("(%d %d) ", ((pixel_coord_t *)vector_get(vec, i))->x, ((pixel_coord_t *)vector_get(vec, i))->y);
//         //     printf("}\n");
//         //     break;
//         
//         default:
//             printf("vector_print not implemented for this vector size %ld\n", vec->sizeof_element);
//             return ESP_FAIL;
//     }
// 
//     return OK;
// }


errno_t vector_clear(
    vector_t *vec
) {
    if (vec) {
        free(vec->data);
        vec->data = NULL;
        vec->size = 0;
        vec->capacity = 0;
        return OK;
    } else {
        printf("no vec\n");
        return EINVAL;
    }
}


errno_t vector_destroy(
    vector_t *vec
) {
    if (vec) {
        free(vec->data);
        free(vec);
        return OK;
    } else {
        printf("no vec\n");
        return EINVAL;
    }
}


errno_t vector_resize(
     vector_t *vec, size_t new_size
) {
    if (!vec) {
        printf("vector_resize no vec!\n");
        return EINVAL;
    }
    
    if (new_size > vec->capacity) {
        vector_reserve(vec, new_size);
    }
    
    if (new_size > vec->size) {
        void *start = (char*)vec->data + vec->size * vec->sizeof_element;
        memset(start, 0, (new_size - vec->size) * vec->sizeof_element);
    }
    
    vec->size = new_size;

    return OK;
}