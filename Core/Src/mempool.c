/*
 * mempool.c
 *
 *  Created on: Jan 13, 2024
 *      Author: Duncan
 */

#include "mempool.h"
#include "dbc_assert.h"
#include <stddef.h>
#include "sst.h"

DBC_MODULE_NAME("mempool")

void mpool_init(mpool_t *me, uint8_t *pMem, uint32_t memSize,
		uint32_t blockSize) {

	uint32_t numBlocks = (uint32_t) (memSize / blockSize);

	DBC_ASSERT(10u, (numBlocks > 0));
	DBC_ASSERT(11u, pMem != NULL);

	me->head = (mpool_empty_t*) pMem;

	uint8_t *ptr = pMem;
	while (ptr < (pMem + (memSize - blockSize)))
	{
		((mpool_empty_t *)ptr)->next = (mpool_empty_t *)(ptr + blockSize);
		ptr = ptr + blockSize;
	}

	((mpool_empty_t *)ptr)->next =NULL;
	me->free = numBlocks;
	me->blocksize = blockSize;
}

void* mpool_get(mpool_t *me) {
	SST_PORT_CRIT_STAT
	void *result = NULL;

	SST_PORT_CRIT_ENTRY();

	if (me->free > 0u) {  /*if we have free values in the list*/
		result = (void*) me->head;
		me->free--;
		if (me->free > 0) {
			me->head = me->head->next;
		} else {
			me->head = NULL;
		}
	}
	SST_PORT_CRIT_EXIT();
	return result;
}

void mpool_put(mpool_t *me, uint8_t *block) {
	SST_PORT_CRIT_STAT
	SST_PORT_CRIT_ENTRY();
	((mpool_empty_t*) block)->next = me->head;
	me->head = (mpool_empty_t*) block;
	me->free++;
	SST_PORT_CRIT_EXIT();
}
