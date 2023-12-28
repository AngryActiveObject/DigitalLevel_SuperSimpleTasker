/*
 * blinky.h
 *
 *  Created on: Dec 3, 2023
 *      Author: Duncan
 */

#ifndef INC_BLINKY_H_
#define INC_BLINKY_H_

#include "sst.h"


typedef struct {
	SST_Task super;
	/** add additional task data here*/
	SST_TimeEvt blinkyTimer;
} BlinkyTask_T;


void Blinky_ctor(BlinkyTask_T * me) ;
#endif /* INC_BLINKY_H_ */
