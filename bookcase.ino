#include <Adafruit_NeoPixel.h>

#define PIN_LED     5 // D1
#define PIN_SWITCH     D2


//#define LINE_LEN 	2
#define LINE_LEN 	39
#define LINES		5
#define LED_COUNT	LINES*LINE_LEN

#define BRIGHT_DELAY	500 // ms
#define BRIGHT_SPEED    3 // higher==slower

// fade tweakables
#define FADE_ON_SPEED	8 // higher==slower
#define FADE_OFF_SPEED	8 // higher==slower

// rollup tweakables
#define SMOOTH_WIDTH 2.5 // width of transition region, in lines

// rainbow tweakables
#define RAINBOW_SPREAD	12
#define RAINBOW_SPEED	7
#define RAINBOW_OFF_SPEED  10 // higher==slower
#define RAINBOW_START 250
#define RAINBOW_END   350


Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, PIN_LED, NEO_GRBW + NEO_KHZ800);

enum state_t {
    STATE_OFF,
    STATE_ON,
} state = STATE_OFF, requested = STATE_OFF;

enum anim_t {
    ANIM_NONE=0,
    ANIM_TURN_ON,
    ANIM_TURN_OFF,
    ANIM_SET_BRIGHT,
} anim = ANIM_NONE;

uint32_t now=0, animstart=0, brightframe;

void setup() {
    Serial.begin(115200);
    Serial.println("bookcase " __DATE__ " " __TIME__);
    pinMode(PIN_SWITCH, INPUT);
    strip.begin();
    strip.show(); // all off
    set_bright(223);
}

#define NTRIES 5 // ~15ms

bool sw = 0;
uint32_t sw_last_changed = 0;

// returns true if state has changed
bool read_switch() {
    static uint8_t debounce = 0;

    bool r = digitalRead(PIN_SWITCH);

    if (sw != r) {
	if (++debounce > NTRIES) {
	    debounce = 0;
	    sw = r;
	    int delta = now-sw_last_changed;
	    sw_last_changed = now;
	    Serial.printf("switch %d\n", sw);
	    return true;
	    //webSocket.broadcastTXT(message_string());
	}
    } else {
	debounce = 0;
    }

    return false;
}

void set_anim (anim_t a) {
	anim = a;
	animstart = now;
}
    
void set_state (state_t what, anim_t how) {
    if (what != requested) {
	requested = what;
	set_anim (how);
    }
}

void anim_done() {
    if (!sw) {
	state = requested;
	anim = ANIM_NONE;
    }
}

void set_bright(int b) {
    //bright = b;
    strip.setBrightness(strip.gamma8(b+32));
    brightframe = 224 << BRIGHT_SPEED;
}


#define max(a,b) ((a>b)?(b):(a))
#define min(a,b) ((a<b)?(b):(a))
#define clamp(x,minval,maxval) max(min(x,minval),maxval)

static inline int boustrophedon(int x, int stride) {
    int y = x / stride,
	yy=(y+1) & ~1,
	xx = x % stride,
	n = yy * stride + ((y&1) ? -xx-1 : xx);
    return n;
}

/* smooth transition from 0 when x<edge0 to 255 when x>edge0+width */
static inline int smoothstep(int x, int edge0, int width) {
    // scale, bias, saturate
    x = clamp((x - edge0) * 256 / width, 0, 256);

    return (x * x * (767 - 2*x))>>16;
}

#define gammaw(whiteval) strip.Color(0,0,0,strip.gamma8(whiteval))

#define fill_all(whiteval) do { 		\
	strip.fill(gammaw(whiteval));		\
    } while (0)

#define fill_line(whiteval,line) do {				\
	strip.fill(gammaw(whiteval), line*LINE_LEN,LINE_LEN);	\
    } while (0)

static inline uint32_t mix(uint32_t a, uint32_t b, int frac) {
    frac = clamp(frac, 0, 256);
    union {
	uint32_t c;
	struct {
	    uint8_t c0, c1, c2, c3;
	};
    } aa = { .c = a },
      bb = { .c = b },
      out;

    out.c0 = ((aa.c0 * frac) + (bb.c0 * (256-frac))) >> 8;
    out.c1 = ((aa.c1 * frac) + (bb.c1 * (256-frac))) >> 8;
    out.c2 = ((aa.c2 * frac) + (bb.c2 * (256-frac))) >> 8;
    out.c3 = ((aa.c3 * frac) + (bb.c3 * (256-frac))) >> 8;
    return out.c;
}

typedef bool (*animfunc(int));
/* const animfunc funcs[] = { */
/*     animate_fade_in, */
/*     animate_fade_up, */
/* }; */

/* on animations. these should leave all the LEDs at full white (0,0,0,255) */
bool animate_on_fade(int f) {
    f = (f<<8) >> FADE_ON_SPEED;
    fill_all(max(f,255));
    return f>255;
}

bool animate_on_rollup(int f) {
    f = (f<<8) >> FADE_ON_SPEED;
    for (int i=0; i<LINES; i++) {
	fill_line(smoothstep(f, i<<8, SMOOTH_WIDTH*256), i);
    }
    return f>(((LINES-1)<<8)+SMOOTH_WIDTH*256);
}

/* off animations. these should leave all the LEDs dark (0,0,0,0) */
bool animate_off_fade(int f) {
    f = (f<<8) >> FADE_OFF_SPEED;
    fill_all(255-max(f,255));
    return f>255;
}

bool animate_off_rainbow(int f) {
f = (f<<8) >> RAINBOW_OFF_SPEED;
    for (int i=0; i<LED_COUNT; i++) {
        uint32_t col = strip.gamma32(strip.ColorHSV((f<<RAINBOW_SPEED) + (i<<RAINBOW_SPREAD))),
	    white = strip.Color(0,0,0,255),
	    black = strip.Color(0,0,0,0),
	    mixed = mix (white,
		  mix(col, black,
		      smoothstep(i, f-RAINBOW_END, 256)),
			     smoothstep(i, f-RAINBOW_START, 256));

	strip.setPixelColor(boustrophedon(LED_COUNT-i-1, LINE_LEN), mixed);
     }
return f>1000; // FIXME
}

void set_leds(void) {
    uint32_t f = now-animstart;
    switch (anim) {
    case ANIM_TURN_ON:
    if (animate_on_rollup(f))
	    anim_done();
	break;
    case ANIM_TURN_OFF:
    if (animate_off_rainbow(f))
	    anim_done();
	break;
    case ANIM_SET_BRIGHT:
	{
	    anim_done();
	    int bright = boustrophedon(f >> BRIGHT_SPEED, 224) %224;
	    set_bright(bright);
	    //Serial.println("jkl");
	    strip.fill(strip.Color(0, 0, 0, 255));
	    brightframe = f;
	    break;
	}
    case ANIM_NONE:
	anim_done();
	delay(3);
	return;
    }
    strip.show();
}

void loop() {
    uint32_t last = now;
    now = millis();
    if (read_switch()) {
	switch (state) {
	case STATE_OFF:
	    // when off, switch on as soon as button is touched
	    set_bright(223); // reset to max brightness
	    set_state(STATE_ON, ANIM_TURN_ON);
	    break;
	case STATE_ON:
	    if (!sw && !anim) {
		// when on, switch off when button released
		set_state(STATE_OFF, ANIM_TURN_OFF);
	    }
	    break;
	}
    } else {
	if (sw && state==STATE_ON) {
	    if (now - sw_last_changed > BRIGHT_DELAY && !anim) {
		set_anim(ANIM_SET_BRIGHT);
		animstart -= brightframe;
	    }
	}
    }
    set_leds();

    //Serial.printf("state %d req %d anim %d elapsed %d  \r", state, requested, anim, now-last);
}
