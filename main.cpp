#include "ch.hpp"
#include "hal.h"
#include "Notifier.hpp"
#include "chprintf.h"

class LEDData : public NotifierMsg<LEDData> {
public:
	uint8_t pin;
	bool_t set;
};


Notifier<LEDData> led_notifier;

/*
 * Receive data from led_notifier
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {
	Listener<LEDData, 5> listener(&led_notifier);
  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
	  LEDData *d = listener.get();
	  if(d->set)
		  palSetPad(GPIOD, d->pin);
	  else
		  palClearPad(GPIOD, d->pin);

	  listener.release(d);
  }
}

/* print the leds to serial */
static WORKING_AREA(waThread2, 256);
static msg_t Thread2(void *arg) {
	Listener<LEDData, 2> listener(&led_notifier);
  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
	  LEDData *d = listener.get();

	  if(d->set)
		  chprintf((BaseChannel *)&SD2, "palSetPad(GPIOD, %d);\r\n", 1);
	  else
		  chprintf((BaseChannel *)&SD2, "palClearPad(GPIOD, %d);\r\n", 2);

	  listener.release(d);
  }
}

static const SerialConfig serial_config = {
	230400,
	0,
	USART_CR2_STOP1_BITS | USART_CR2_LINEN,
	0
};

int main(void) {
  halInit();
  chSysInit();

  /*
   * Activates the serial driver 2 using the driver default configuration.
   * PA2(TX) and PA3(RX) are routed to USART2.
   */
  sdStart(&SD2, &serial_config);
  palSetPadMode(GPIOA, 2, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 3, PAL_MODE_ALTERNATE(7));

  /*
   * Creates the example thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);
  chThdCreateStatic(waThread2, sizeof(waThread2), NORMALPRIO, Thread2, NULL);

  LEDData *d;
  while (TRUE) {
	d = led_notifier.alloc();
	if(d != NULL) {
		d->pin = GPIOD_LED3;
		d->set = true;
		led_notifier.broadcast(d);
	}
	chThdSleepMilliseconds(500);

	d = led_notifier.alloc();
	if(d != NULL) {
		d->pin = GPIOD_LED4;
		d->set = true;
		led_notifier.broadcast(d);
	}
	chThdSleepMilliseconds(500);

	d = led_notifier.alloc();
	if(d != NULL) {
		d->pin = GPIOD_LED3;
		d->set = false;
		led_notifier.broadcast(d);
	}
	chThdSleepMilliseconds(500);

	d = led_notifier.alloc();
	if(d != NULL) {
		d->pin = GPIOD_LED4;
		d->set = false;
		led_notifier.broadcast(d);
	}
	chThdSleepMilliseconds(500);
  }
}
