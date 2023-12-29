
/* local includes*/
#include "blinky.h"
#include "main.h"
#include "bsp.h"

#include "dbc_assert.h" /* Design By Contract (DBC) assertions */
DBC_MODULE_NAME("blinky")

static void Blinky_initHandler(BlinkyTask_T *const me, SST_Evt const *const ie);
static void Blinky_taskHandler(BlinkyTask_T *const me, SST_Evt const *const e);

void Blinky_ctor(BlinkyTask_T *me) {

	SST_Task_ctor(&(me->super), (SST_Handler) &Blinky_initHandler,
			(SST_Handler) &Blinky_taskHandler);

	SST_TimeEvt_ctor(&(me->blinkyTimer), BLINKYTIMER, &(me->super));

}

/*Init handler called by SST kernel on start of the task. Arms the blinky timer with reload value of 50ms*/
void Blinky_initHandler(BlinkyTask_T *const me, SST_Evt const *const ie) {
	(void) ie;
	SST_TimeEvt_arm(&(me->blinkyTimer), 1u, 50);
}

/*Gets blinky task is called periodically at 50ms and sets the brighness of the four LEDS on the STM32407G-DISC1
 * board in proportion to the LIS3DSH sensors X and Y axis accelerations (psuedo level sensor)*/
void Blinky_taskHandler(BlinkyTask_T *const me, SST_Evt const *const e) {

	(void) me;

	switch (e->sig) {
	case BLINKYTIMER: {

		/*linear scale isn't great as duty doesn't scale with brightness linearly but ok for a first go*/
		uint_fast16_t  brightnessScale = 6; /*power of 2 for efficiency will shift by 6 places*/

		LIS3DSH_Results_t xyz_accels = LIS3DSH_read();

		uint_fast16_t  xbrightnessPos = (uint_fast16_t) ((xyz_accels.x_g > 0) ? xyz_accels.x_g : 0);
		uint_fast16_t  xbrightnessNeg = (uint_fast16_t) ((xyz_accels.x_g < 0) ? -xyz_accels.x_g : 0);
		uint_fast16_t  ybrightnessPos = (uint_fast16_t) ((xyz_accels.y_g > 0) ? xyz_accels.y_g : 0);
		uint_fast16_t  ybrightnessNeg = (uint_fast16_t) ((xyz_accels.y_g < 0) ? -xyz_accels.y_g : 0);

		/*example of how to round the fixed point division operation,
		 * if the 5th bit is true then it will be truncated, so round up by adding 1 or down by adding 0*/
		xbrightnessPos = (uint_fast16_t)((xbrightnessPos >> brightnessScale) + ((xbrightnessPos & 0x20) ? 1u : 0u));

		xbrightnessNeg = (uint_fast16_t)((xbrightnessNeg >> brightnessScale) + ((xbrightnessNeg & 0x20) ? 1u : 0u));

		ybrightnessPos = (uint_fast16_t)((ybrightnessPos >> brightnessScale) + ((ybrightnessPos & 0x20) ? 1u : 0u));

		ybrightnessNeg = (uint_fast16_t)((ybrightnessNeg >> brightnessScale) + ((ybrightnessNeg & 0x20) ? 1u : 0u));

		set_blue_LED_duty((uint16_t)ybrightnessNeg);
		set_orange_LED_duty((uint16_t)ybrightnessPos);
		set_red_LED_duty((uint16_t)xbrightnessPos);
		set_green_LED_duty((uint16_t)xbrightnessNeg);

		break;
	}
	default: {
		DBC_ERROR(200);
		break;
	}
	}
}

