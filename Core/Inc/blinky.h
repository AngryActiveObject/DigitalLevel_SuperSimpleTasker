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
	SST_Task super; /*Inherit SST task */
	/** add additional task data here*/
	SST_TimeEvt blinkyTimer; /*Timer provide cyclic call to the blinky task handler*/
} BlinkyTask_T;

/*Constructor for the blinky task*/
void Blinky_ctor(BlinkyTask_T * me) ;
#endif /* INC_BLINKY_H_ */
