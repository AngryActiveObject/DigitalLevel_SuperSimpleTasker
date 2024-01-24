/*
 * devnt.h
 *
 *  Created on: Jan 13, 2024
 *      Author: Duncan
 */

#ifndef INC_DEVNT_H_
#define INC_DEVNT_H_

#include <stdint.h>

typedef struct devnt_pool_s {
	uint32_t freeList_bf; /*bitfield of free pool locations*/
	uint32_t blockSize;
	uint8_t *pmemPool; /*pointer to the start of the mempool*/
} devnt_pool_t;

void devnt_pool_init(devnt_pool_t *me, void *pMem, uint32_t memSize,
		uint32_t blockSize);

void* devnt_pool_get(devnt_pool_t *me, uint32_t Size);

void devnt_pool_put(devnt_pool_t *me, uint8_t *block);

#endif /* INC_DEVNT_H_ */
