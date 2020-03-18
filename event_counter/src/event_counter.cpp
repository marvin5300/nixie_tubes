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
const double frequency = 16e6;//3.6864e6;
const double time_factor = frequency/16.0;
const double prescaler0 = 1024.0;
 //Pin connected to ST_CP of 74HC595
uint8_t latchPin = 11;
//Pin connected to SH_CP of 74HC595
uint8_t clockPin = 10;
////Pin connected to DS of 74HC595
uint8_t dataPin = 12;
//Pin connected to power Relais
uint8_t powerPin = 3;
bool powerON = false;
//Pin pullup button check
uint8_t buttonPin = 15;
uint8_t interruptPin0 = 2;
uint8_t counter = 0;
uint8_t ledPin = 13;
bool incrementCounter = false;
bool ledON = false;
bool toggle1 = false;
bool buttonPressed = false;
//holders for infromation you're going to pass to shifting function

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
		shiftOut(dataPin, clockPin, latchPin, (1<<counter));
		//int16_t output = (1<<(counter+1));
		//shiftOut(dataPin, clockPin, latchPin, ((output&0xff00)>>8));
		//shiftOut(dataPin, clockPin, latchPin, (output&0xff));
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
	/*if (toggle1){
		digitalWrite(ledPin, HIGH);
		toggle1 = false;
	}else{
		digitalWrite(ledPin, LOW);
		toggle1 = true;
	}*/
}

ISR(INT0_vect){
	if (digitalRead(interruptPin0)==HIGH){
		digitalWrite(ledPin,HIGH);
	}else{
		digitalWrite(ledPin,LOW);
	}
}

// the heart of the program
void shiftOut(uint8_t myDataPin, uint8_t myClockPin, uint8_t myLatchPin, uint8_t myDataOut) {
	// This shifts 8 bits out MSB first, 
	//on the rising edge of the clock,
	//clock idles low

	//clear everything out just in case to
	//prepare shift register for bit shifting
	digitalWrite(myDataPin, 0);
	digitalWrite(myClockPin, 0);

	//for each bit in the byte myDataOut�
	//NOTICE THAT WE ARE COUNTING DOWN in our for loop
	//This means that %00000001 or "1" will go through such
	//that it will be pin Q0 that lights. 
	for (int i = 7; i >= 0; i--) {
		digitalWrite(myClockPin, 0);
		//if the value passed to myDataOut and a bitmask result 
		// true then... so if we are at i=6 and our value is
		// %11010100 it would the code compares it to %01000000 
		// and proceeds to set pinState to 1.
		uint8_t mask = (1<<i);
		if ((myDataOut & mask)!=0) {
			digitalWrite(myDataPin, HIGH);
		}
		else {
			digitalWrite(myDataPin, LOW);
		}

		//Sets the pin to HIGH or LOW depending on pinState
		//register shifts bits on upstroke of clock pin  
		digitalWrite(myClockPin, 1);
		//zero the data pin after shift to prevent bleed through
		digitalWrite(myDataPin, 0);
	}

	//stop shifting
	digitalWrite(myClockPin, 0);
	digitalWrite(myLatchPin, 1);
	digitalWrite(myLatchPin, 0);
}
