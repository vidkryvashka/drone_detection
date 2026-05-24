#ifndef MY_VECTOR_H
#define MY_VECTOR_H

#include <stdlib.h>
#include <string.h>

#include "defs.h"

#define VECTOR_DEFAULT_INIT_CAPACITY 8


/**
 * @brief base vector struct inspired by std::vector
 */
typedef struct {
	void* data;
	size_t size;
	size_t capacity;
	size_t sizeof_element;
} vector_t;


/**
 * @brief Initialize vector with specified element size
 * 
 * @param sizeof_element was convinced to determine by sizeof()
 * @return vector_t* pointer, should be freed
 */
vector_t* vector_create(const size_t sizeof_element, size_t size);


/**
 * @brief Reserve memory for specified number of elements
 * 
 * @param vec 
 * @param new_capacity
 * @return errno_t -1: realloc failed
 */
errno_t vector_reserve(vector_t *vec, const size_t new_capacity);


/**
 * @brief Push element to the back
 * 
 * @param vec 
 * @param element 
 * @return errno_t -1: !vec || !element
 */
errno_t vector_push_back(vector_t *vec, const void *element);


/**
 * @brief Get pointer to element at index
 * 
 * @param vec 
 * @param index 
 * @return void* pointer to wanted data
 */
inline void* vector_get(const vector_t *vec, const size_t index) {
	return (vec && index < vec->size)? (char*)vec->data + index * vec->sizeof_element : NULL;
}


/**
 * @brief Set value at index
 * 
 * @param vec 
 * @param index 
 * @param val
 * @return -1: memcpy returned NULL
 */
errno_t vector_set(vector_t *vec, size_t index, void *val);


/**
 * @brief Resize vector to new size
 * 
 * @param vec
 * @param size_t new size
 * @return errno_t -1: no vec
 */
errno_t vector_resize(vector_t *vec, size_t new_size);


/**
 * @brief Erases element by index, aligns the rest of elements
 * 
 * @param vec
 * @param index
 */
errno_t vector_erase(vector_t *vec, const size_t index);


/**
 * @brief Clear vector (set size to 0)
 * 
 * @param vec 
 * @return errno_t -1: no vec
 */
// errno_t vector_clear(vector_t *vec);
#define vector_clear(vec) vector_resize(vec, 0)


/**
 * @brief Free vector memory
 * 
 * @param vec 
 * @return errno_t -1: no vec
 */
errno_t vector_destroy(vector_t *vec);

#endif