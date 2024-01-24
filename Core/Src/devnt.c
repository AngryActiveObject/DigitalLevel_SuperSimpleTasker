/*
 * devnt.c
 *
 *  Created on: Jan 13, 2024
 *      Author: Duncan
 *
 *      dynamic event pool
 *      bitfield based event pool manager, optimised for cortex m4, can only support 32 entries maximum, relies on the
 */

#include "devnt.h"
#include "cmsis_gcc.h"
#include "dbc_assert.h"
#include <stddef.h>
#include "sst.h"

DBC_MODULE_NAME("devnt")

void devnt_pool_init(devnt_pool_t *me, void *pMem, uint32_t memSize,
		uint32_t blockSize) {
	uint32_t numBlocks = memSize / blockSize;

	DBC_ASSERT(10u, (numBlocks > 0) && (numBlocks <= 32));
	DBC_ASSERT(11u, pMem != NULL);

	/*set the free list bits with available blocks*/
	me->freeList_bf = (uint32_t) (1 << (numBlocks)) - 1; /* shift a single bit and subtract one to fill lower bits.*/
	me->pmemPool = pMem;
	me->blockSize = blockSize;
}

void* devnt_pool_get(devnt_pool_t *me, uint32_t Size) {
	SST_PORT_CRIT_STAT
	if (Size > me->blockSize) {
		return NULL;
	}

	SST_PORT_CRIT_ENTRY();

	uint_fast8_t nextFreeBit = (31u - __CLZ (me->freeList_bf)); /*find a free entry*/
	me->freeList_bf &= ~(uint32_t) (1 << nextFreeBit); /*clear the bit as used, we can now leave the critical section*/

	SST_PORT_CRIT_EXIT();

	return (void*) (me->pmemPool + (nextFreeBit * me->blockSize));
}

void devnt_pool_put(devnt_pool_t *me, uint8_t *block) {

	SST_PORT_CRIT_ENTRY();
	uint8_t blockBit = (uint8_t) ((uint32_t) (block - me->pmemPool)
			/ me->blockSize);
	me->freeList_bf |= (uint32_t) (1 << blockBit); /*set the bit as unused, we can now leave the critical section*/
	SST_PORT_CRIT_EXIT();
}
