#include <MIDI.h>
#include <SPI.h>
#include "MCP4728.h"
#include <Wire.h>
//#include <PT2258.h>

MCP4728 dac;
//PT2258 mixerA(0x80);    // CODE1 = 0, CODE2 = 0

#define DAC1  6
#define GATE1 14
#define GATE2 15
#define GATE3 16
#define nVoices 3

MIDI_CREATE_DEFAULT_INSTANCE();

void setup() {
  Wire.begin();
  Serial.begin(9600);
  pinMode(GATE1, OUTPUT);
  pinMode(GATE2, OUTPUT);
  pinMode(GATE3, OUTPUT);

  pinMode(DAC1, OUTPUT);
  digitalWrite(DAC1,HIGH);

  Serial.begin(9600);

  MIDI.begin(MIDI_CHANNEL_OMNI);
    //============== Mixer init ===================
/*    if (!mixerA.begin()) { // initialise the PT2258 mixerA chip
        Serial.println("mixerA: connection error!");
    }

    mixerA.volumeAll(0);  // at the beginning the volume is by default at 100%. Set the desired volume at startup before unmuting next
    mixerA.mute(false); // the mute is active when the device powers up. Unmute it to ear the sound
*/
    //============== DAC init =====================
    dac.attach(Wire, DAC1);
    dac.readRegisters();

    dac.selectVref(MCP4728::VREF::INTERNAL_2_8V, MCP4728::VREF::INTERNAL_2_8V, MCP4728::VREF::INTERNAL_2_8V, MCP4728::VREF::INTERNAL_2_8V);
    dac.selectPowerDown(MCP4728::PWR_DOWN::GND_100KOHM, MCP4728::PWR_DOWN::GND_100KOHM, MCP4728::PWR_DOWN::GND_500KOHM, MCP4728::PWR_DOWN::GND_500KOHM);
    dac.selectGain(MCP4728::GAIN::X2, MCP4728::GAIN::X2, MCP4728::GAIN::X2, MCP4728::GAIN::X2);
    dac.analogWrite(MCP4728::DAC_CH::A, 0);
    dac.analogWrite(MCP4728::DAC_CH::B, 0);
    dac.analogWrite(MCP4728::DAC_CH::C, 0);
    dac.analogWrite(MCP4728::DAC_CH::D, 0);
}

int VCO[3] = {-1};
bool notes[88] = {0}; 
int8_t noteOrder[20] = {0}, orderIndx = {0};
unsigned long trigTimer = 0;

void loop() {
  int type, noteMsg, velocity, channel, d1, d2;
  
  if (MIDI.read()) {                    
    byte type = MIDI.getType();
    switch (type) {
      case midi::NoteOn:
        noteMsg = MIDI.getData1() - 21; // A0 = 21, Top Note = 108
        channel = MIDI.getChannel();
        
        if ((noteMsg < 0) || (noteMsg > 87)) break; // Only 88 notes of keyboard are supported

        if (type == midi::NoteOn) velocity = MIDI.getData2();
        else velocity  = 0;  

        if (velocity == 0)  {
          notes[noteMsg] = false;
          //disableNote(noteMsg);
        }
        else {
          notes[noteMsg] = true;
        }

        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
            orderIndx = (orderIndx+1) % 20;
            noteOrder[orderIndx] = noteMsg;                 
          }
          commandLastNote();            
        break;
        
/*      case midi::NoteOff:
        noteMsg = MIDI.getData1() - 21; // A0 = 21, Top Note = 108
        channel = MIDI.getChannel();
        disableNote(noteMsg);
        break;*/

      case midi::ActiveSensing: 
        break;
        
      default:
        d1 = MIDI.getData1();
        d2 = MIDI.getData2();
    }
  }

}


/*void disableNote(int noteMsg) {
  for (int i=0; i<20; i++) {
    if (noteOrder[i] == noteMsg) {
      noteOrder[i] = -1;
      break;
   }
  }
  playNotes();
  //setVoltage(noteVCO[noteMsg], 0);
  //digitalWrite(13 + noteVCO[noteMsg], HIGH);
  //noteVCO[noteMsg] = -1;
}*/

/*
void playNotes() {
  int k = 0;
  for (int i = 0; i < 20; i++) {
    if (k < 3 & noteOrder[i] > -1) {
        k++;
//        mixerA.volume(k, (notes[i]/127)*100);
//        noteVCO[noteOrder[i]] = k;
        commandNote(noteOrder[i], k);
    } else if (k >= 3) {
      return;
    }
  }

  while (k < 3) {
    commandNote(0, k);
    k++;
  }
} */

void commandLastNote()
{

  int8_t noteIndx;
  
  for (int i=0; i<20; i++) {
    noteIndx = noteOrder[ mod(orderIndx-i, 20) ];
    if (notes[noteIndx]) {
      commandNote(noteIndx, 1);
      return;
    }
  }
  digitalWrite(GATE1,LOW);  // All notes are off
}

// Rescale 88 notes to 4096 mV:
//    noteMsg = 0 -> 0 mV 
//    noteMsg = 87 -> 4096 mV
// DAC output will be (4095/87) = 47.069 mV per note, and 564.9655 mV per octive
// Note that DAC output will need to be amplified by 1.77X for the standard 1V/octave 
#define NOTE_SF 44.0f // This value can be tuned if CV output isn't exactly 1V/octave

void commandNote(int noteMsg, int channel) {
  digitalWrite(GATE1,HIGH);
  unsigned int mV = (unsigned int) ((float) noteMsg * NOTE_SF + 0.5); 
  setVoltage(channel, mV);  // DAC1, channel 0, gain = 2X
}

// setVoltage -- Set DAC voltage output
// dacpin: chip select pin for DAC.  Note and velocity on DAC1, pitch bend and CC on DAC2
// channel: 0 (A) or 1 (B).  Note and pitch bend on 0, velocity and CC on 2.
// gain: 0 = 1X, 1 = 2X.  
// mV: integer 0 to 4095.  If gain is 1X, mV is in units of half mV (i.e., 0 to 2048 mV).
// If gain is 2X, mV is in units of mV
void setVoltage(int channel, unsigned int mV)
{
  switch(channel) {
    case 1:
      dac.analogWrite(MCP4728::DAC_CH::A, mV);
      break;
    case 2:
      dac.analogWrite(MCP4728::DAC_CH::B, mV);
      break;
    case 3:
      dac.analogWrite(MCP4728::DAC_CH::C, mV);
      break;
    case 4:
      dac.analogWrite(MCP4728::DAC_CH::D, mV);
      break;
    
    default:
      return;
      
  }

  //dac.analogWrite(mV, 0, 0, 0);
}

int mod(int a, int b)
{
    int r = a % b;
    return r < 0 ? r + b : r;
}