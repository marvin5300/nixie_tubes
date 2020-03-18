/*
  Shift Register Example
  Turning on the outputs of a 74HC595 using an array

 Hardware:
 * 74HC595 shift register
 * LEDs attached to each of the outputs of the shift register

 */
#include <Arduino.h>
#include "./event_counter.h"
//#include "/home/user/Desktop/arduino-1.8.12/hardware/arduino/avr/cores/arduino/Arduino.h"
const double frequency = 3.6864e6;
const double time_factor = frequency/16.0;
const double prescaler0 = 1024.0;
 //Pin connected to ST_CP of 74HC595
const uint8_t latchPin = 6;
//Pin connected to SH_CP of 74HC595
const uint8_t clockPin = 5;
////Pin connected to DS of 74HC595
const uint8_t dataPin = 4;
//Pin connected to power Relais
const uint8_t powerPin = 3;
bool powerON = false;
//Pin pullup button check
const uint8_t buttonPin = 15;
uint8_t interruptPin0 = 2;
uint8_t counter = 0;
uint8_t ledPin = 13;
bool incrementCounter = false;
bool ledON = false;
bool toggle1 = false;
bool buttonPressed = false;
//holders for infromation you're going to pass to shifting function
uint8_t num_nixies = 1;

void setup() {
	//set pins to output because they are addressed in the main loop
	pinMode(latchPin, OUTPUT);
	pinMode(clockPin, OUTPUT);
	pinMode(dataPin, OUTPUT);
	pinMode(ledPin, OUTPUT);
	pinMode(powerPin, OUTPUT);
	pinMode(buttonPin, INPUT_PULLUP);
	pinMode(interruptPin0, INPUT);
	digitalWrite(powerPin, HIGH);
	digitalWrite(ledPin, LOW);
	ledON = false;
	digitalWrite(latchPin, LOW);
	digitalWrite(clockPin, LOW);
	digitalWrite(dataPin, LOW);
	Serial.begin(9600);
	//startMillis = millis();
	// setup timer interrupts
	cli();
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;
	OCR1A = static_cast<uint16_t>(frequency/prescaler0-1); // (must be < 65536)
	TCCR1B |= (1 << WGM12);
	TCCR1B |= (1 << CS12);
	TCCR1B |= (1 << CS10);
	TIMSK1 |= (1 << OCIE1A);
	// setup external trigger
	EICRA |= (1 << ISC00);	// sets interrupt 0 trigger on any logic change
	EIMSK |= (1 << INT0);	// turns on interrupt 0
	sei();
}

void loop() {
	if (incrementCounter){
		if (counter < 9){
			counter = counter +1;
		}else{
			counter = 0;
		}
		//shiftOut(dataPin, clockPin, latchPin, static_cast<uint16_t>(1<<2)); // 
		//shiftOut(dataPin, clockPin, latchPin, static_cast<uint16_t>(1<<3)); // == 1
		//shiftOut(dataPin, clockPin, latchPin, static_cast<uint16_t>(1<<4)); // == 2
		// 7 does nothing
		//shiftOut(dataPin, clockPin, latchPin, static_cast<uint16_t>(1<<8)); // == 5
		//shiftOut(dataPin, clockPin, latchPin, static_cast<uint16_t>(1<<12)); // == 9
		// 14 and 15 do nothing
		//shiftOut(dataPin, clockPin, latchPin, static_cast<uint16_t>(1<<13)); // == 0
		//shiftOut(dataPin, clockPin, latchPin, static_cast<uint16_t>(1<<14)); //
		uint8_t shift[] = {counter};
		shiftout(shift);
		incrementCounter = false;
	}
	
	if (digitalRead(buttonPin)==HIGH && !buttonPressed){
		buttonPressed = true;
		if (powerON){
			digitalWrite(powerPin, HIGH);
			powerON = false;
		}else{
			digitalWrite(powerPin, LOW);
			powerON = true;
		}
	}
	if (digitalRead(buttonPin)==LOW && buttonPressed){
		buttonPressed = false;
	}
}

ISR(TIMER1_COMPA_vect){
	incrementCounter = true;
	if (toggle1){
		digitalWrite(ledPin, HIGH);
		toggle1 = false;
	}else{
		digitalWrite(ledPin, LOW);
		toggle1 = true;
	}
}

ISR(INT0_vect){
	/*if (digitalRead(interruptPin0)==HIGH){
		digitalWrite(ledPin,HIGH);
	}else{
		digitalWrite(ledPin,LOW);
	}*/
}

// the heart of the program
void shiftout(uint8_t *myDataOut) {
	// This shifts 16 bits out MSB first, 
	//on the rising edge of the clock,
	//clock idles low

	//clear everything out just in case to
	//prepare shift register for bit shifting
	digitalWrite(dataPin, 0);
	digitalWrite(clockPin, 0);

	uint16_t shift[num_nixies];
	for (uint8_t i = 0; i < num_nixies; i++){
		if (myDataOut[i] > 0){
			shift[i] = static_cast<uint16_t>(1<<(myDataOut[i]+2));
		}else{
			shift[i] = static_cast<uint16_t>(1<<13);
		}
		if (myDataOut[i] > 4){
			shift[i] = shift[i] << 1;
		}
	}
	//for each bit in the byte myDataOut
	//NOTICE THAT WE ARE COUNTING DOWN in our for loop
	//This means that %00000001 or "1" will go through such
	//that it will be pin Q0 that lights. 
	for (int i = 0; i < num_nixies; i++){
		for (int k = 0; k < 16; k++) {
			digitalWrite(clockPin, 0);
			//if the value passed to myDataOut and a bitmask result 
			// true then... so if we are at i=6 and our value is
			// %11010100 it would the code compares it to %01000000 
			// and proceeds to set pinState to 1.
			uint16_t mask = (1<<k);
			if (shift[i] == mask) {
				digitalWrite(dataPin, HIGH);
			}
			else {
				digitalWrite(dataPin, LOW);
			}

			//Sets the pin to HIGH or LOW depending on pinState
			//register shifts bits on upstroke of clock pin  
			digitalWrite(clockPin, 1);
			//zero the data pin after shift to prevent bleed through
			digitalWrite(dataPin, 0);
		}
	}

	//stop shifting
	digitalWrite(clockPin, 0);
	digitalWrite(latchPin, 1);
	digitalWrite(latchPin, 0);
}
