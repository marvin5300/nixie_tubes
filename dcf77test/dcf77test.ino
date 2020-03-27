#include <Arduino.h>
#include "TimeLib.h"
//#include "/home/user/Desktop/arduino-1.8.12/hardware/arduino/avr/cores/arduino/Arduino.h"
// Setup specific things
const double frequency = 3.6864e6;
const double time_factor = frequency/16.0;
const double prescaler0 = 1024.0;
const uint8_t num_nixies = 2;
const uint32_t max_number = pow_ten(num_nixies)-1;

//Pin Setup
//Pin connected to ST_CP of 74HC595
const uint8_t latchPin = 6;
//Pin connected to SH_CP of 74HC595
const uint8_t clockPin = 5;
////Pin connected to DS of 74HC595
const uint8_t dataPin = 4;
//Pin connected to power Relais
const uint8_t powerPin = 3;
//--------------------------------------

bool powerON = false;
const uint8_t buttonPin = 15;
uint8_t interruptPin0 = 2;
uint8_t counter = 0;
uint8_t ledPin = 13;
bool toggle1 = false;
bool buttonPressed = false;

// debug variable for time output over serial
int lastTimeOutput = 0;

// DCF77 stuff:
uint8_t dcf_counter = 1;
uint8_t dcf_data[8];
bool interval_ready = false;
bool ignore_next_falling = false;
int falling = 0;
int rising = 0;
int last_rising = 0;
const int dcf_timing_threshold = 180;
const int max_interval = 300;
const int min_interval = 95;
const int max_high_high = 1500;

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
	digitalWrite(latchPin, LOW);
	digitalWrite(clockPin, LOW);
	digitalWrite(dataPin, LOW);
  	Serial.begin(9600);
	// setup timer interrupts
	cli(); // disable interrupts
	/*TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;
	OCR1A = static_cast<uint16_t>(frequency/prescaler0-1); // (must be < 65536)
	TCCR1B |= (1 << WGM12);
	TCCR1B |= (1 << CS12);
	TCCR1B |= (1 << CS10);
	TIMSK1 |= (1 << OCIE1A);
	*/
  // setup external trigger
	EICRA |= (1 << ISC00);	// sets interrupt 0 trigger on any logic change
	EIMSK |= (1 << INT0);	// turns on interrupt 0
	sei(); // enable interrupts
}

void loop() {
	if (interval_ready){
		// called if the interrupt pin captured an interval
		cli();
		int interval = rising-falling;
		sei();
		if (interval > max_interval || rising-last_rising > max_high_high || dcf_counter > 59){
			if (dcf_counter == 59){
				// very likely that the transmission was successful
				dcf_evaluate();
			}
			// something went wrong with the transmission, start again
			Serial.println((int)dcf_counter);
			dcf_counter = 1;
			for (int i = 7; i >= 0; i--){
				Serial.print((int)dcf_data[i],BIN);
				dcf_data[i]=0;
			}
			Serial.println("");
		}else if(interval > min_interval){
			if (interval > dcf_timing_threshold){
				dcf_data[(dcf_counter>>3)] |= (1 << (dcf_counter & 0b111));
			}
			dcf_counter++;
		}
		if (interval > min_interval){
			last_rising = rising;
		}
		interval_ready = false;
		int now = millis();
		if ((now-lastTimeOutput)>1200){
			lastTimeOutput = now;
			Serial.print(hour());
			Serial.print(":");
			Serial.print(minute());
			Serial.print(":");
			Serial.print(second());
			Serial.print(" ");
			Serial.print(day());
			Serial.print(" ");
			Serial.print(month());
			Serial.print(" ");
			Serial.print(year());
			Serial.println(); 
		}
		// DEBUG -------------------
		/*if (interval > 100){
			Serial.print(interval); // debug output over serial
			Serial.print("   ");
			Serial.println(rising - last_rising);
		}*/
		// --------------------------
	}
}

ISR(TIMER1_COMPA_vect){
	/*if (toggle1){
		digitalWrite(ledPin, HIGH);
		toggle1 = false;
	}else{
		digitalWrite(ledPin, LOW);
		toggle1 = true;
	}*/
}

ISR(INT0_vect){
	// interrupt pin 0 function
	cli();
	int now = millis();
  	bool i0 = digitalRead(interruptPin0)==HIGH;
	if (i0){
		digitalWrite(ledPin,HIGH);
		rising = now;
		interval_ready = true;
	}else{
		digitalWrite(ledPin,LOW);
		falling = now;
	}
	sei();
}

// evaluate dcf message
void dcf_evaluate(){
	cli();
	bool parity_all = true;
	bool CEST = (dcf_data[2]>>1)&1 == 1;
	if ((dcf_data[2]>>4)&1!=1){
		Serial.println("WARNING, S != 1");
		parity_all = false;
	}
	uint8_t parity1 = (dcf_data[2]>>5);
	parity1 |= (dcf_data[3]&0b11111)<<3;
	if (parity_check(parity1)){
		Serial.println("WARNING, P1 wrong");
		parity_all = false;
	}
	uint8_t parity2 = (dcf_data[3]&0b11100000)>>5;
	parity2 |= ((dcf_data[4]&0b1111)<<3);
	if (parity_check(parity2)){
		Serial.println("WARNING, P2 wrong");
		parity_all = false;
	}
	uint32_t parity3 = ((dcf_data[4]&0xf0)>>4);
	parity3 <<= 8;
	parity3 |= dcf_data[5];
	parity3 <<= 8;
	parity3 |= dcf_data[6];
	parity3 <<= 3;
	parity3 |= (dcf_data[7]&0b111);
	if (parity_check(parity3)){
		Serial.println("WARNING, P3 wrong");
		parity_all = false;
	}
	if (!parity_all){
		sei();
		return;
	}
	int minutes = 0;
	minutes += ((dcf_data[2]&(1<<5))>>5);
	minutes += 2*((dcf_data[2]&(1<<6))>>6);
	minutes += 4*((dcf_data[2]&(1<<7))>>7);
	minutes += 8*((dcf_data[3]&1));
	minutes += 10*((dcf_data[3]&(1<<1))>>1);
	minutes += 20*((dcf_data[3]&(1<<2))>>2);
	minutes += 40*((dcf_data[3]&(1<<3))>>3);
	int hours = 0;
	hours += ((dcf_data[3]&(1<<5))>>5);
	hours += 2*((dcf_data[3]&(1<<6))>>6);
	hours += 4*((dcf_data[3]&(1<<7))>>7);
	hours += 8*((dcf_data[4]&1));
	hours += 10*((dcf_data[4]&(1<<1))>>1);
	hours += 20*((dcf_data[4]&(1<<2))>>2);
	int day_month = 0;
	day_month += ((dcf_data[4]&(1<<4))>>4);
	day_month += 2*((dcf_data[4]&(1<<5))>>5);
	day_month += 4*((dcf_data[4]&(1<<6))>>6);
	day_month += 8*((dcf_data[4]&(1<<7))>>7);
	day_month += 10*(dcf_data[5]&1);
	day_month += 20*((dcf_data[5]&(1<<1))>>1);
	int day_week = ((dcf_data[5]&0b11100)>>2);
	int month = 0;
	month += ((dcf_data[5]&(1<<5))>>5);
	month += 2*((dcf_data[5]&(1<<6))>>6);
	month += 4*((dcf_data[5]&(1<<7))>>7);
	month += 8*(dcf_data[6]&1);
	month += 10*((dcf_data[6]&(1<<1))>>1);
	int year = 2000;
	year += ((dcf_data[6]&(1<<2))>>2);
	year += 2*((dcf_data[6]&(1<<3))>>3);
	year += 4*((dcf_data[6]&(1<<4))>>4);
	year += 8*((dcf_data[6]&(1<<5))>>5);
	year += 10*((dcf_data[6]&(1<<6))>>6);
	year += 20*((dcf_data[6]&(1<<7))>>7);
	year += 40*(dcf_data[7]&1);
	year += 80*((dcf_data[7]&(1<<1))>>1);
	setTime(hours,minutes,0,day_month,month,year);
	sei();
}

// the heart of the program
void shiftout(uint8_t number) {
	// This shifts 16 bits out MSB first, 
	//on the rising edge of the clock,
	//clock idles low

	//clear everything out just in case to
	//prepare shift register for bit shifting
	digitalWrite(dataPin, 0);
	digitalWrite(clockPin, 0);

	//for each bit in the byte myDataOut
	//NOTICE THAT WE ARE COUNTING DOWN in our for loop
	//This means that %00000001 or "1" will go through such
	//that it will be pin Q0 that lights.
	for (int i = num_nixies-1; i >= 0; i--){
		uint8_t tmp = (number/(pow_ten(i)))%10;
		uint16_t shift;
		if (tmp>0){
			shift = static_cast<uint16_t>(1<<(tmp+2));
		}else{
			shift = static_cast<uint16_t>(1<<13);
		}
		if (tmp>4){
			shift <<=1;
		}
		for (int k = 0; k < 16; k++) {
			digitalWrite(clockPin, 0);
			//if the value passed to myDataOut and a bitmask result 
			// true then... so if we are at i=6 and our value is
			// %11010100 it would the code compares it to %01000000 
			// and proceeds to set pinState to 1.
			uint16_t mask = (1<<k);
			if (shift == mask) {
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

uint32_t pow_ten(uint8_t n){
	uint32_t val = 1;
	for (uint8_t i = 0; i < n; i++){
		val *= 10;
	}
	return val;
}

bool parity_check(uint8_t x) {
    uint8_t parity = 0;
    while(x != 0) {
        parity ^= x;
        x >>= 1;
    }
    return (parity & 0x1);
}

int parity_check(uint32_t x) {
    uint32_t parity = 0;
    while(x != 0) {
        parity ^= x;
        x >>= 1;
    }
    return (parity & 0x1);
}