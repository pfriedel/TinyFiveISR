// Tiny85 versioned - ATmega requires these defines to be redone as well as the
// DDRB/PORTB calls later on.

/* commentary: 

Future modes:

1. random color random position - existing mode

2. hue walk at constant brightness - existing mode

3. brightness walk at constant hue - existing mode

4. saturation walk is kind of obnoxious - existing mode

5. "Rain" - start at LED1 with a random color.  Advance color to LED2 and pick a
   new LED1.  Rotate around the ring.

6. non-equidistant hue walks - instead of 0 72 144 216 288 incrementing,
   something like...  0 30 130 230 330 rotating around the ring.  The hue += and
   -= parts might help here?

7. Color morphing: Pick 5 random colors.  Transition from color on LED1 to LED2
   and around the ring.  Could be seen as a random variant of #2.

8. Larson scanner in primaries - Doable with 5 LEDs? (probably, I've seen
   larsons with 2 actively lit LEDs in 4 LEDs before.)

9. Pick a color and sweep brightness around the circle (red! 0-100
   larson-style), then green 0-100, the blue 0-100, getting new color on while
   old color is fading out.

*/
#include <avr/sleep.h>
#include <avr/power.h>
#include <EEPROM.h>


// The base unit for the time comparison. 1000=1s, 10000=10s, 60000=1m, etc.
//#define time_multiplier 1000 
#define time_multiplier 60000
// How many time_multiplier intervals should run before going to sleep?
#define run_time 240

#define MAX_HUE 360 // normalized

// How bit-crushed do you want the bit depth to be?  1 << DEPTH is how
// quickly it goes through the LED softpwm scale.

// 1 means 128 shades per color.  3 is 32 colors while 6 is 4 shades per color.
// it sort of scales the time drawing routine, but not well.

// It mostly affects how much POV flicker there is - 
// 1 is moderately flickery, but the color depth is the best.
// 3 is about the best balance of color depth and flicker

// The flicker is visible in all modes, the depth crushing starts to be evident
// in the SBWalk modes.  Mode 7 is the touchstone for not having enough bits.

// The build uses 1 for the PTH versions and 3 for the SMD displays.

#define DEPTH 3

byte __attribute__ ((section (".noinit"))) last_mode;

uint8_t led_grid[15] = {
  000 , 000 , 000 , 000 , 000 , // R
  000 , 000 , 000 , 000 , 000 , // G
  000 , 000 , 000 , 000 , 000  // B
};

// Anode pin | cathode pin
const uint8_t led_dir[15] = {
  ( 1<<1 | 1<<0 ), // 1 r
  ( 1<<0 | 1<<4 ), // 2 r
  ( 1<<4 | 1<<3 ), // 3 r
  ( 1<<3 | 1<<2 ), // 4 r
  ( 1<<2 | 1<<1 ), // 5 r

  ( 1<<1 | 1<<2 ), // 1 g
  ( 1<<0 | 1<<1 ), // 2 g
  ( 1<<4 | 1<<0 ), // 3 g
  ( 1<<3 | 1<<4 ), // 4 g
  ( 1<<2 | 1<<3 ), // 5 g

  ( 1<<1 | 1<<3 ), // 1 b
  ( 1<<0 | 1<<2 ), // 2 b
  ( 1<<4 | 1<<1 ), // 3 b
  ( 1<<3 | 1<<0 ), // 4 b
  ( 1<<2 | 1<<4 ), // 5 b
};

//PORTB output config for each LED (1 = High, 0 = Low)
// (anode pins, the lot of them)
const uint8_t led_out[15] = {
  ( 1<<1 ), // 1
  ( 1<<0 ), // 2
  ( 1<<4 ), // 3
  ( 1<<3 ), // 4
  ( 1<<2 ), // 5
  
  ( 1<<1 ), // 1
  ( 1<<0 ), // 2
  ( 1<<4 ), // 3
  ( 1<<3 ), // 4
  ( 1<<2 ), // 5
  
  ( 1<<1 ), // 1
  ( 1<<0 ), // 2
  ( 1<<4 ), // 3
  ( 1<<3 ), // 4
  ( 1<<2 ), // 5
};

uint16_t b;
uint8_t led;
uint8_t max_brite = 255>>DEPTH;
volatile uint16_t timer_overflow_count = 0;

// invert the logic - update the LEDs during the interrupt, constantly draw them otherwise.
ISR(TIMER0_OVF_vect) {
  // turn off the LEDs while you ponder their new numbers.
  DDRB=0;
  PORTB=0;

  for(uint8_t led = 0; led<5; led++) {
    uint16_t hue = ((led) * MAX_HUE/20+timer_overflow_count)%MAX_HUE;
    setLedColorHSV(led, hue, 255, 255);
  }
  timer_overflow_count++;
  if(timer_overflow_count>360) { timer_overflow_count = 0; }
}


void setup() {

  // Try and set the random seed more randomly.  Alternate solutions involve
  // using the eeprom and writing the last seed there.
  uint16_t seed=0;
  uint8_t count=32;
  while (--count) {
    seed = (seed<<1) | (analogRead(1)&1);
  }
  randomSeed(seed);

  /*
CS bits:
02 01 00
0  0  0 = Clock stopped
0  0  1 = No prescaling
0  1  0 = /8
0  1  1 = /64
1  0  0 = /256
1  0  1 = /1024
  */

//  TCCR0B |= (1<<CS00);             // no prescaling
//  TCCR0B |= (1<<CS01);             // /8
//  TCCR0B |= (1<<CS01) | (1<<CS00); // /64
//  TCCR0B |= (1<<CS02);             // /256
  TCCR0B |= (1<<CS02) | (1<<CS00); // /1024

  TIMSK |= 1<<TOIE0;
  sei();
}

void loop() {
  for ( led=0; led<15; led++ ) { 
    // software PWM 
    // input range is 0 (off) to 255>>DEPTH (127/63/31/etc) (fully on)
    
    // Light the LED in proportion to the value in the led_grid array
    for( b=0; b < led_grid[led]; b++ ) {
      DDRB = led_dir[led];
      PORTB = led_out[led];
    }
    
    // and turn the LEDs off for the amount of time in the led_grid array
    // between LED brightness and 255>>DEPTH.
    for( b=led_grid[led]; b < max_brite; b++ ) {
      DDRB = 0;
      PORTB = 0;
    }
  }
  
  // Force the LEDs off - otherwise if the last LED on (led 14) was at full
  // brightness at the end of the softPWM, it would never actually get turned
  // off and would flicker. (mostly visible in the SB_Walk modes, if you want to
  // test it)
  DDRB = 0;
  PORTB = 0;
}

/*
  Inputs:
  p : LED to set
  hue : 0-359 - color
  sat : 0-255 - how saturated should it be? 0=white, 255=full color
  val : 0-255 - how bright should it be? 0=off, 255=full bright
*/
void setLedColorHSV(uint8_t p, int16_t hue, int16_t sat, int16_t val) {
  while (hue > 359) hue -= 360;
  while (hue < 0) hue += 361;

  int r = 0;
  int g = 0;
  int b = 0;
  int base = 0;

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    r = g = b = val;
  }
  else {
    base = ((255 - sat) * val)>>8;

    switch(hue/60) {
    case 0:
      r = val;
      g = (((val-base)*hue)/60)+base;
      b = base;
      break;

    case 1:
      r = (((val-base)*(60-(hue%60)))/60)+base;
      g = val;
      b = base;
      break;

    case 2:
      r = base;
      g = val;
      b = (((val-base)*(hue%60))/60)+base;
      break;

    case 3:
      r = base;
      g = (((val-base)*(60-(hue%60)))/60)+base;
      b = val;
      break;

    case 4:
      r = (((val-base)*(hue%60))/60)+base;
      g = base;
      b = val;
      break;

    case 5:
      r = val;
      g = base;
      b = (((val-base)*(60-(hue%60)))/60)+base;
      break;
    }
  }

  // output range is 0-255
  // Shifting it to the DEPTH bits right crushes it to 127, 64, 32, 16, 8 shades per color.

  led_grid[p] = r>>DEPTH;
  led_grid[p+5] = g>>DEPTH;
  led_grid[p+10] = b>>DEPTH;
}

