#include <Adafruit_NeoPixel.h>

#define PIN_LED     5 // D1
#define PIN_SWITCH     D2


//#define LINE_LEN 	2
#define LINE_LEN 	39
#define LINES		5
#define LED_COUNT	LINES*LINE_LEN

#define BRIGHT_DELAY	500 // ms
#define BRIGHT_SPEED    3 // higher==slower

#define FADE_ON_SPEED	8 // higher==slower
#define FADE_OFF_SPEED	8 // higher==slower

//#define RAINBOW_SPREAD	6
//#define RAINBOW_SPEED	8


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

uint32_t now=0, animstart=0, brightframe=224<<BRIGHT_SPEED;

void setup() {
    Serial.begin(115200);
    Serial.println("bookcase " __DATE__ " " __TIME__);
    pinMode(PIN_SWITCH, INPUT);
    strip.begin();
    strip.show(); // all off
    strip.setBrightness(255);
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
#define max(a,b) ((a>b)?(b):(a))
#define min(a,b) ((a<b)?(b):(a))

static inline int boustrophedon(int x, int stride) {
    int y = x / stride,
	yy=(y+1) & ~1,
	xx = x % stride,
	n = yy * stride + ((y&1) ? -xx-1 : xx);
    return n;
}

void set_leds(void) {
    uint32_t f = now-animstart;
    switch (anim) {
    case ANIM_TURN_ON:
	f = (f<<8) >> FADE_ON_SPEED;
	strip.fill(strip.Color(0,0,0,strip.gamma8(max(f,255))));
	if (f>255)
	    anim_done();
	break;
    case ANIM_TURN_OFF:
	{
	    //for (int x=0; x<LINE_LEN; x++) {
	    //	for (int y=0; y<LINES; y++) {
	    //for (int i=0; i<10; i++) {
	    //strip.setPixelColor(i, strip.gamma32(strip.ColorHSV((f<<RAINBOW_SPEED)+(i<<RAINBOW_SPREAD))));
	    //}
	    f = (f<<8) >> FADE_OFF_SPEED;
	    strip.fill(strip.Color(0,0,0,strip.gamma8(255-max(f,255))));
	    if (f>255)
		anim_done();
	}
	break;
    case ANIM_SET_BRIGHT:
	{
	    anim_done();
	    int bright = boustrophedon(f >> BRIGHT_SPEED, 224) %224;
	    strip.setBrightness(strip.gamma8(bright+32));
	    //Serial.println("jkl");
	    strip.fill(strip.Color(0, 0, 0, 255), 0, LINES*LINE_LEN);
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
