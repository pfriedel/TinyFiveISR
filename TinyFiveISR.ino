// Tiny85 versioned - ATmega requires these defines to be redone as well as the
// DDRB/PORTB calls later on.

#include <avr/sleep.h>
#include <avr/power.h>
#include <EEPROM.h>


// The base unit for the time comparison. 1000=1s, 10000=10s, 60000=1m, etc.
//#define time_multiplier 1000 
#define time_multiplier 60000
// How many time_multiplier intervals should run before going to sleep?
#define run_time 240
#define MAX_MODE 14 // should be 14
#define MAX_HUE 360

// Location of the brown-out disable switch in the MCU Control Register (MCUCR)
#define BODS 7
// Location of the Brown-out disable switch enable bit in the MCU Control Register (MCUCR)
#define BODSE 2

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
uint8_t led, drawcount;
uint8_t max_brite = 255>>DEPTH;
uint32_t end_time = 0;
volatile uint8_t cur_led = 0;

// invert the logic - update the LEDs during the interrupt, constantly draw them otherwise.
ISR(TIMER0_OVF_vect) { 

  // How many times should the routine loop through the array before returning?
  // It's a scaling factor - one pass makes the display dimmer, 8 passes makes
  // the ISR time out.  Arguably this could also be handled by setting the
  // overflow count to 255>>5 so the ISR gets called 7 times more often than
  // 62500 times/second. (16,000,000 mhz / 256)

  for(drawcount=0; drawcount<7; drawcount++)  {  
    for(led = 0; led<15; led++) {
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
    DDRB=0;
    PORTB=0;
  }
}


void EEReadSettings (void) {  // TODO: Detect ANY bad values, not just 255.
  byte detectBad = 0;
  byte value = 255;

  value = EEPROM.read(0);
  if (value > MAX_MODE)
    detectBad = 1;
  else
    last_mode = value;  // MainBright has maximum possible value of 8.

  if (detectBad)
    last_mode = 0; // I prefer the rainbow effect.
}

void EESaveSettings (void){
  //EEPROM.write(Addr, Value);

  // Careful if you use  this function: EEPROM has a limited number of write
  // cycles in its life.  Good for human-operated buttons, bad for automation.

  byte value = EEPROM.read(0);

  if(value != last_mode)
    EEPROM.write(0, last_mode);
}

void SleepNow(void) {
  // decrement last_mode, so the EXTRF increment sets it back to where it was.
  // note: this actually doesn't work that great in cases where power is removed after the MCU goes to sleep.
  // On the other hand, that's an edge condition that I'm not going to worry about too much.
  last_mode--;

  // mode is type byte, so when it rolls below 0, it will become a Very
  // Large Number compared to MAX_MODE.  Set it to MAX_MODE and the setup
  // routine will jump it up and down by one.
  if(last_mode > MAX_MODE) { last_mode = MAX_MODE; }

  EESaveSettings();

  // Important power management stuff follows
  ADCSRA &= ~(1<<ADEN); // turn off the ADC
  ACSR |= _BV(ACD);     // disable the analog comparator
  MCUCR |= _BV(BODS) | _BV(BODSE); // turn off the brown-out detector

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // do a complete power down
  sleep_enable(); // enable sleep mode
  sei();  // allow interrupts to end sleep mode
  sleep_cpu(); // go to sleep
  delay(500);
  sleep_disable(); // disable sleep mode for safety
}

/* ------------------------------------------------------------------------ */

void setup() {

  if(bit_is_set(MCUSR, PORF)) { // Power was just established!
    MCUSR = 0; // clear MCUSR
    EEReadSettings(); // read the last mode out of eeprom
  }
  else if(bit_is_set(MCUSR, EXTRF)) { // we're coming out of a reset condition.
    MCUSR = 0; // clear MCUSR
    last_mode++; // advance mode

    if(last_mode > MAX_MODE) {
      last_mode = 0; // reset mode
    }
  }


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

  TCCR0B |= (1<<CS00);             // no prescaling
  TIMSK |= 1<<TOIE0;
  sei();
}


void AllRand(void) {
  uint8_t allrand_time = 5;
  // Clear the display so it doesn't have any cruft left over from the prior mode.
  while(1) {
    for(int x = 0; x<=15; x++) {
      led_grid[x] = 0;
    }

    int randmode = random(14); // the 15th mode would be itself.

    switch(randmode) {
      // red modes
    case 0:
      HueWalk(allrand_time,millis(),20,1); // wide virtual space, slow progression
      break;
    case 1:
      HueWalk(allrand_time,millis(),20,5); // wide virtual space, fast progression
      break;
    case 2:
      HueWalk(allrand_time,millis(),5,1); // 1:1 space to LED, slow progression
      break;
    case 3:
      HueWalk(allrand_time,millis(),5,2); // 1:1 space to LED, fast progression
      break;
    case 4:
      SBWalk(allrand_time,millis(),1,1); // Slow progression through hues modifying brightness
      break;
    case 5:
      SBWalk(allrand_time,millis(),4,1); // fast progression through hues modifying brightness
      break;
    case 6:
      RandHueWalk(allrand_time,millis());
      break;
    case 7:
      BiColorWalk(allrand_time, millis(), 0, 120); // red to green, works great
      break;
    case 8:
      BiColorWalk(allrand_time, millis(), 120, 240); // green to blue, works great
      break;
    case 9:
      BiColorWalk(allrand_time, millis(), 240, 359); // blue to red, works great - setting endhue to 0 makes it a very wiiiide walk.
      break;
    case 10:
      {
        uint16_t starthue = random(360);
        uint16_t endhue = starthue + 120;
        BiColorWalk(allrand_time, millis(), starthue, endhue);
        break;
      }
    }
  }
}

void loop() {
  // indicate which mode we're entering
  for(int x = 0; x <= 255>>DEPTH; x++) {
    led_grid[last_mode] = x;
    delay(250/int(255>>DEPTH));
  }
  delay(500);
  for(int x = 255>>DEPTH; x>=0; x--) {
    led_grid[last_mode] = x;
    delay(250/int(255>>DEPTH));
  }

  delay(100);
  
  EESaveSettings();

  switch(last_mode) {
    // red modes
  case 0:
    HueWalk(run_time,millis(),20,1); // wide virtual space, slow progression
    break;
  case 1:
    HueWalk(run_time,millis(),20,2); // wide virtual space, medium progression
    break;
  case 2:
    HueWalk(run_time,millis(),20,5); // wide virtual space, fast progression
    break;
  case 3:
    HueWalk(run_time,millis(),5,1); // 1:1 space to LED, slow progression
    break;
  case 4:
    HueWalk(run_time,millis(),5,2); // 1:1 space to LED, fast progression
    break;
  case 5:
    RandomColorRandomPosition(run_time,millis());
    break;
  case 6:
    RandHueWalk(run_time,millis()); // It's a lot like the prior mode, but with color shifting
    break;
  case 7:
    SBWalk(run_time,millis(),1,1); // Slow progression through hues modifying brightness
    break;
  case 8:
    SBWalk(run_time,millis(),4,1); // fast progression through hues modifying brightness
    break;
  case 9:
    PrimaryColors(run_time,millis()); // just a bulb check, to be honest.
    break;
  case 10:
    BiColorWalk(run_time, millis(), 0, 120); // red to green, works great
    break;
  case 11:
    BiColorWalk(run_time, millis(), 120, 240); // green to blue, works great
    break;
  case 12:
    BiColorWalk(run_time, millis(), 240, 359); // blue to red, works great - setting endhue to 0 makes it a very wiiiide walk.
  break;
  case 13:
    {
      uint16_t starthue = random(360);
      uint16_t endhue = starthue + 120;
      BiColorWalk(run_time, millis(), starthue, endhue);
      break;
    }
  case 14:
    AllRand();
    break;
  }
  SleepNow();
}

/*
time: How long it should run
jump: How much should hue increment every time an LED flips direction?
mode:
  1 = walk through brightnesses at full saturation,
  2 = walk through saturations at full brightness.
1 tends towards colors & black, 2 tends towards colors & white.
note: mode 2 is kind of funny looking with the current HSV->RGB code.
*/
void SBWalk(uint16_t time, uint32_t start_time, uint8_t jump, uint8_t mode) {
  uint8_t scale_max, delta;
  uint16_t hue = random(MAX_HUE); // initial color
  uint8_t led_val[5] = {37,29,21,13,5}; // some initial distances
  bool led_dir[5] = {1,1,1,1,1}; // everything is initially going towards higher brightnesses

  scale_max = 254;
  delta = 2;
  end_time = (start_time + (time * time_multiplier));

  while(1) {
    if(millis() >= end_time) { break; }
    for(uint8_t led = 0; led<5; led++) {
      if(mode == 1)       { setLedColorHSV(led, hue, 254,          led_val[led]); }
      else if (mode == 2) { setLedColorHSV(led, hue, led_val[led], 254); }
      
      // if the current value for the current LED is about to exceed the top or the bottom, invert that LED's direction
      if((led_val[led] >= (scale_max-delta)) or (led_val[led] <= (0+delta))) {
	led_dir[led] = !led_dir[led];
	if(led_val[led] <= (0+delta)) {
	  hue += jump;
	}
	if(hue >= MAX_HUE)
	  hue = 0;
      }
      if(led_dir[led] == 1)
	led_val[led] += delta;
      else
	led_val[led] -= delta;
    }
    delay(10);
  }
}

void PrimaryColors(uint16_t time, uint32_t start_time) {
  uint8_t led_bright = 1;
  bool led_dir = 1;
  uint8_t led = 0;
  end_time = (start_time + (time * time_multiplier));

  while(1) {
    if(millis() >= end_time) { break; }
    
    // flip the direction when the LED is at full brightness or no brightness.
    if((led_bright >= (255>>DEPTH)) or (led_bright <= 0))
      led_dir = !led_dir;
    
    // increment or decrement the brightness
    if(led_dir == 1)
      led_bright++;
    else
      led_bright--;
    
    // if the decrement will turn off the current LED, switch to the next LED
    if( led_bright <= 0 ) {
      led_grid[led] = 0;
      led++;
    }
    
    // And if that change pushes the current LED off the end of the spire, reset to the first LED.
    if( led >=15)
      led = 0;
    
    // push the change out to the array.
    led_grid[led] = led_bright;
    delay(10);
  }
}

// rotate 5 random color distances around the ring. Works quite nicely.  Leans heavily on randomseed being random.
void RandHueWalk(uint16_t time, uint32_t start_time) {
  end_time = (start_time + (time * time_multiplier));

  uint16_t ledhue[] = {random(360), random(360), random(360), random(360), random(360)};
  while(1) {

    if(millis() >= end_time) { break; }

    for(uint8_t led = 0; led<5; led++) {
      uint16_t hue = ledhue[led]--;
      if(hue == 0) { ledhue[led] = 359; }
      setLedColorHSV(led, hue, 255, 255);
    }
    delay(10); // this gets called 5 times, once per LED.
  }
}

/* Additional ideas:
1) Consider hitting endhue, then desaturating to white and ending up at the starthue.
2) Maybe put all 5 LEDs on slightly different hues within the same start/endhue progression, then larson the\
 colors back and forth?
*/

void BiColorWalk(uint16_t time, uint32_t start_time, uint16_t starthue, uint16_t endhue) {
  uint16_t curhue = starthue;
  int8_t curdir = 1;
  end_time = (start_time + (time * time_multiplier));
  while(1) {
    if(millis() >= end_time) { break; }

    curhue += curdir;
    if(curhue == endhue) { curdir = -1; }
    if(curhue == starthue) { curdir = 1; }

    for(uint8_t led = 0; led < 5; led++) {
      setLedColorHSV(led, curhue, 255, 255);
    }
    delay(10);
  }
}

void HueWalk(uint16_t time, uint32_t start_time, uint8_t width, uint8_t speed) {
  end_time = (start_time + (time * time_multiplier));
  while(1) {

    if(millis() >= end_time) { break; }
    for(int16_t colorshift=MAX_HUE; colorshift>0; colorshift = colorshift - speed) {
      if(millis() >= end_time) { break; }
      for(uint8_t led = 0; led<5; led++) {
        uint16_t hue = ((led) * MAX_HUE/(width)+colorshift)%MAX_HUE;
        setLedColorHSV(led, hue, 255, 255);
      }
      delay(10);
    }
  }
}

void RandomColorRandomPosition(uint16_t time, uint32_t start_time) {
  end_time = (start_time + (time * time_multiplier));

  // preload all the LEDs with a color
  for(int x = 0; x<5; x++) {
    setLedColorHSV(x, random(MAX_HUE), 255, 255);
  }
  // and start blinking new ones in once a second.
  while(1) {
    setLedColorHSV(random(5), random(MAX_HUE), 255, 255);
    delay(1000);
    if(millis() >= end_time) { break; }
  }
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

