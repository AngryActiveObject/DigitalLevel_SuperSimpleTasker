


/* local includes*/
#include "blinky.h"
#include "main.h"
#include "bsp.h"

#include "dbc_assert.h" /* Design By Contract (DBC) assertions */
DBC_MODULE_NAME("blinky")

#define MIN(x_,y_) ((x_ > y_)? y_ : x_)

/*blinky task*/

static void Blinky_initHandler(BlinkyTask_T *const me, SST_Evt const *const ie);
static void Blinky_taskHandler(BlinkyTask_T *const me, SST_Evt const *const e);

void Blinky_ctor(BlinkyTask_T * me) {

	SST_Task_ctor(&(me->super), (SST_Handler) &Blinky_initHandler,
			(SST_Handler) &Blinky_taskHandler);

	SST_TimeEvt_ctor(&(me->blinkyTimer), BLINKYTIMER, &(me->super));

}

void Blinky_initHandler(BlinkyTask_T *const me, SST_Evt const *const ie) {
	(void) ie;
	SST_TimeEvt_arm(&(me->blinkyTimer), 1u, 50);
}


/*Blinky task uses a timer event to toggle the brightness of the red led every 1s*/
#define DUTYHIGH (750)
#define DUTYLOW (250)
void Blinky_taskHandler(BlinkyTask_T *const me, SST_Evt const *const e) {

	(void)me;

	switch (e->sig) {
	case BLINKYTIMER: {

		/*linear scale isn't great as duty doesn't scale with brightness linearly but ok for a first go*/
		uint16_t brightnessScale = 64; /*power of 2 for efficiency*/


		LIS3DSH_Results_t xyz_accels= LIS3DSH_read();

		uint16_t xbrightnessPos = (uint16_t)((xyz_accels.x_g > 0) ? xyz_accels.x_g : 0);
		uint16_t xbrightnessNeg = (uint16_t)((xyz_accels.x_g < 0) ? -xyz_accels.x_g : 0);

		uint16_t ybrightnessPos = (uint16_t)((xyz_accels.y_g > 0) ? xyz_accels.y_g : 0);
		uint16_t ybrightnessNeg = (uint16_t)((xyz_accels.y_g < 0) ? -xyz_accels.y_g : 0);

		set_blue_LED_duty( ybrightnessNeg / brightnessScale);
		set_orange_LED_duty(ybrightnessPos / brightnessScale);

		set_red_LED_duty(xbrightnessPos / brightnessScale);
		set_green_LED_duty(xbrightnessNeg / brightnessScale);

		break;
	}
	default: {
		DBC_ERROR(200);
		break;
	}
	}
}


