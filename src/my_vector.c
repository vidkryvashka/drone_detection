#include "my_vector.h"
#include "defs.h"

#define TAG "my_vector "


vector_t* vector_create(
	size_t size,
	size_t sizeof_element
) {
	vector_t* vec = (vector_t *)malloc(sizeof(vector_t));
	if (!vec) return NULL;

	vec->data = calloc(size, sizeof_element);
	if (size > 0 && !vec->data) {
        free(vec);
        return NULL;
    }
	vec->size = 0;
	vec->capacity = size;
	vec->sizeof_element = sizeof_element;

	return vec;
}


errno_t vector_reserve(
	vector_t *vec,
	const size_t new_capacity
) {
	assert(vec);

	if (new_capacity <= vec->capacity)
		return OK;

	void *realloced_data = realloc(vec->data, new_capacity * vec->sizeof_element);
	if (!realloced_data) {
		ddloge(TAG, "realloc(...) lost data");
		return ENOMEM;
	}

	vec->data = realloced_data;
	vec->capacity = new_capacity;

	return OK;
}


errno_t vector_push_back(
	vector_t *vec,
	const void *element
) {
	assert(vec);
	assert(element);

	if (vec->size >= vec->capacity) {
		size_t new_capacity = (vec->capacity == 0) ? VECTOR_DEFAULT_INIT_CAPACITY : vec->capacity << 1;
		if (vector_reserve(vec, new_capacity))
			return ENOMEM;
	}

	void *dest = (char*)vec->data + vec->size * vec->sizeof_element;
	memcpy(dest, element, vec->sizeof_element);
	vec->size++;

	return OK;
}


errno_t vector_set(
	vector_t *vec,
	const size_t index,
	void *val
) {
	assert(vec && val);
	if (index >= vec->capacity)
		if (vector_resize(vec, index + 1))
			return ENOMEM;

	void *dest = (char*)vec->data + index * vec->sizeof_element;
	memcpy(dest, val, vec->sizeof_element);
	return OK;
}


errno_t vector_erase(
	vector_t *vec,
	const size_t index
) {
	assert(vec);
	if (index >= vec->size) {
		ddloge(TAG, "index %zu out of bounds (size %zu)", index, vec->size);
		return EINVAL;
	}

	if (index < vec->size - 1) {
		void *dest = (char*)vec->data + index * vec->sizeof_element;
		void *src = (char*)dest + vec->sizeof_element;
		size_t bytes_to_move = (vec->size - index - 1) * vec->sizeof_element;

		memmove(dest, src, bytes_to_move);
	}

	vec->size--;
	void *freed_tail = (char*)vec->data + vec->size * vec->sizeof_element;
	// memset(freed_tail, 0, vec->sizeof_element); // may be excessive, next vector_push_back will write sth over it

	return OK;
}


errno_t vector_resize(
	vector_t *vec, size_t new_size
) {
	assert(vec);

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


// errno_t vector_clear(
// 	vector_t *vec
// ) {
// 	assert(vec);
// 	vector_resize(vec, VECTOR_DEFAULT_INIT_CAPACITY);
// 	vec->size = 0;
// 	vec->capacity = VECTOR_DEFAULT_INIT_CAPACITY;
// 	return OK;
// }


errno_t vector_destroy(
	vector_t *vec
) {
	if (vec) {
		free(vec->data);
		free(vec);
		return OK;
	} else {
		ddloge(TAG, "already no vec");
		return EINVAL;
	}
}
