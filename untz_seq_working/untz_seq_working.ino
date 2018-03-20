//Mini UNTZtrument step seqencer with working pots for Arduino Leonardo
//
//Everything is set to channel 1, port 0 and the pots are assigned to control 13, 12, 11, 10 if you were looking at it face on (Left to right)
//
//The code was mashed together from multiple sources and is not my own, I just put it together.
//
//It can probably be improved a lot and may even have leftover code in it.
//
//Will need to install the libraries

#include <MIDI.h>
#include <Wire.h>
#include <Adafruit_Trellis.h>
#include <Adafruit_Trellis_XY.h>
#include <Adafruit_UNTZtrument.h>
#include "MIDIUSB.h"

#define NUMTRELLIS 1 // Number of Trellis boards
#define NKEYS (NUMTRELLIS * 16) 
#define WIDTH   (NUMTRELLIS * 4)
#define INTPIN A2
#define LED 13 // Pin for heartbeat LED (shows code is working)
#define CHANNEL 1
#define N_POTS 4

MIDI_CREATE_DEFAULT_INSTANCE();

Adafruit_Trellis matrix0 = Adafruit_Trellis();
Adafruit_TrellisSet trellis =  Adafruit_TrellisSet(&matrix0);
Adafruit_UNTZtrument untztrument(&matrix0);

Adafruit_Trellis_XY trellisXY = Adafruit_Trellis_XY();

enc e(4, 5); // Encoder on pin 5 sets tempo.

uint8_t       grid[NKEYS];                 // Sequencer state
uint8_t       heart        = 0,            // Heartbeat LED counter
              col          = NKEYS - 1;    // Current column
unsigned int  bpm          = 128;          // Tempo
unsigned long beatInterval = 60000L / bpm, // ms/beat
              prevBeatTime = 0L,           // Column step timer
              prevReadTime = 0L;           // Keypad polling timer

// Pots
const int potPin[] = {0, 1, 2, 3 };  // Pot pins
const uint8_t potCN[] = {0x0A, 0x0B, 0x0C, 0x0D };  // MIDI control values  
uint8_t potValues[N_POTS];  // Initial values
uint8_t potValuePrev[] = {0, 0, 0, 0, 0}; // previous values for comparison
  
static const uint8_t PROGMEM

note[8]    = {  72, 71, 69, 67, 65, 64, 62,  60 }, // Midi notes C, B, A, G, F, E, D, C (One octive lower)
channel[8] = {   0, 0, 0, 0, 0, 0, 0, 0, }, // Midi channels for pad
bitmask[8] = {   1,  2,  4,  8, 16, 32, 64, 128 };


//-----------------------------------------------------------

void setup() {
  
  Serial.begin(31250);
  
  trellisXY.begin(NKEYS); // define number of keys
  trellisXY.setOffsets(0, 0, 0); // set up x/y offsets
  
  pinMode(INTPIN, INPUT); // INT pin requires a pullup
  digitalWrite(INTPIN, HIGH);

  trellis.begin(0x70); // Tile address

  // Startup light animation //
  
  // light up all the LEDs in order
  for (byte y = 0; y < 4; y++) {
    for (byte x = 0; x < 4; x++) {
      trellis.setLED(trellisXY.getTrellisId(x, y));
      trellis.writeDisplay();
      delay(50);
    }
  }
  
  // then turn them off
  for (byte y = 0; y < 4; y++) {
    for (byte x = 0; x < 4; x++) {
      trellis.clrLED(trellisXY.getTrellisId(x, y));
      trellis.writeDisplay();
      delay(50);
    }
  }

  // End of animation //

  #ifdef __AVR__
    TWBR = 12; // 400 KHz I2C on 16 MHz AVR
  #endif

  memset(grid, 0, sizeof(grid));
  e.setBounds(60 * 4, 480 * 4 + 3); // Set tempo limits
  e.setValue(bpm * 4);              // *4's for encoder detents
  
  MIDI.begin();
  
}

// Turn on (or off) one column of the display
void line(uint8_t x, boolean set) {
  for (uint8_t mask = 1, y = 0; y < 8; y++, mask <<= 1) {
    uint8_t i = untztrument.xy2i(x, y);
    if (set || (grid[x] & mask)) trellis.setLED(i);
    else                        trellis.clrLED(i);
  }
}

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush();
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
  MidiUSB.flush();
}

void readPots() {
  for (int i=0; i < N_POTS; i++) {
    int val = analogRead(potPin[i]);
    potValues[i] = (uint8_t) (map(val, 0, 1023, 0, 127));
  }
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

void sendMIDI()
{
  for (int i=0; i < N_POTS; i++) {
    if (abs(potValuePrev[i] - potValues[i]) > 1)
    {
      potValuePrev[i] = potValues[i];
      controlChange(0, potCN[i], potValues[i]);
    }
  }
}
//-----------------------------------------------------------
void loop() {
  
  uint8_t       mask;
  boolean       refresh = false;
  unsigned long t       = millis();

  readPots();
  sendMIDI();
  MidiUSB.flush();
  delay(30);

  // If a button was just pressed or released...
  if (trellis.readSwitches()) {
    
    // go through every button
    for (uint8_t i = 0; i < trellisXY.numKeys; i++) {
      byte xValue;
      byte yValue;
      byte xyTrellisID;
      uint8_t x, y;
      
      untztrument.i2xy(i, &x, &y);
      mask = pgm_read_byte(&bitmask[y]);
      
      // if it was pressed...
      if (trellis.justPressed(i)) {
        
        // get x/y values
        xValue = trellisXY.getTrellisX(i);
        yValue = trellisXY.getTrellisY(i);
        
        // get Trellis ID from x/y values
        xyTrellisID = trellisXY.getTrellisId(xValue, yValue);
        
        if (grid[x] & mask) { // Already set?  Turn off...
          grid[x] &= ~mask;
          trellis.clrLED(i);
          noteOff(pgm_read_byte(&channel[y]), pgm_read_byte(&note[y]), 127);
        } else { // Turn on
          grid[x] |= mask;
          trellis.setLED(i);
        }
        
        refresh = true;
        
      }
    }
    
    prevReadTime = t;
    digitalWrite(LED, ++heart & 32); // Blink = alive
    
  }
  if ((t - prevBeatTime) >= beatInterval) { // Next beat?

    // Turn off old column
    line(col, false);
    
    for (uint8_t row = 0, mask = 1; row < 8; row++, mask <<= 1) {
      if (grid[col] & mask) {
        noteOff(pgm_read_byte(&channel[row]), pgm_read_byte(&note[row]), 127);
      }
    }
    
    // Advance column counter, wrap around
    if (++col >= WIDTH) col = 0;
    
    // Turn on new column
    line(col, true);
    
    for (uint8_t row = 0, mask = 1; row < 8; row++, mask <<= 1) {
      if (grid[col] & mask) {
        noteOn(pgm_read_byte(&channel[row]), pgm_read_byte(&note[row]), 127);
      }
    }
    
    prevBeatTime = t;
    refresh      = true;
    bpm          = e.getValue() / 4; // Div for encoder detents
    beatInterval = 60000L / bpm;
    
  }
  
  if (refresh) trellis.writeDisplay();
  
}

