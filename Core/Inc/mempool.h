/*
 * mempool.h
 *
 *  Created on: Jan 13, 2024
 *      Author: Duncan
 */

#ifndef INC_MEMPOOL_H_
#define INC_MEMPOOL_H_

#include <stdint.h>

typedef struct mpool_empty_s{
	struct mpool_empty_s * next;
}mpool_empty_t;


typedef struct mpool_s{
	mpool_empty_t *head;
	uint32_t free;
	uint32_t blocksize;
}mpool_t;


void mpool_init(mpool_t *me, uint8_t *pMem, uint32_t memSize,
		uint32_t blockSize);

void* mpool_get(mpool_t *me);

void mpool_put(mpool_t *me, uint8_t *block);

#endif /* INC_MEMPOOL_H_ */
