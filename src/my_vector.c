#include "my_vector.h"
#include "defs.h"

#define TAG "my_vector "
#define INIT_CAPACITY 8



vector_t* vector_create(
	size_t sizeof_element
) {
	vector_t* vec = (vector_t *)malloc(sizeof(vector_t));
	if (!vec) {
		ddloge(TAG, "couldn't malloc vector");
		return NULL;
	}

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
		ddloge(TAG, "!vec");
		return EINVAL;
	}

	if (new_capacity <= vec->capacity)
		return OK;

	void *realloced_data = realloc(vec->data, new_capacity * vec->sizeof_element);
	if (!realloced_data) {
		ddloge(TAG, "realloc(...) lost data");
		return ENOSPC;
	}

	vec->data = realloced_data;
	vec->capacity = new_capacity;

	return OK;
}


errno_t vector_push_back(
	vector_t *vec,
	const void *element
) {
	if (!vec || !element) {
		ddloge(TAG, "(!vec || !element)");
		return EINVAL;
	}

	if (vec->size >= vec->capacity) {
		size_t new_capacity = (vec->capacity == 0) ? INIT_CAPACITY : vec->capacity * (vec->capacity * 3) / 2;
		if (vector_reserve(vec, new_capacity))
			return ENOMEM;
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


errno_t vector_set(
	vector_t *vec,
	const size_t index,
	void *val
) {
	if (index >= vec->capacity)
		vector_reserve(vec, index + 2);

	void *dest = (char*)vec->data + index * vec->sizeof_element;
	if (!memcpy(dest, val, vec->sizeof_element)) {
		ddloge(TAG, "failed to set element in vec, sizeof_element %ld", vec->sizeof_element);
		return ENOSPC;
	}
	if (index >= vec->size)
		vec->size = index + 1;

	return OK;
}


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
		ddloge(TAG, "no vec");
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
		ddloge(TAG, "no vec");
		return EINVAL;
	}
}


errno_t vector_resize(
	vector_t *vec, size_t new_size
) {
	if (!vec) {
		ddloge(TAG, "no vec!");
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
