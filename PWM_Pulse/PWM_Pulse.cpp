/* version 02
* create in 20141119
* Modified in 20150623
* This labariry can generate 4 parallel serial triggers on ATmega2560 or 1 parallel serial triggers on ATmega328p(UNO or NANO).
*/
#include "PWM_Pulse.h"

#define PRE_SCALE_64DIV (_BV(CS10) | _BV(CS11)) //4us
#define CTC_ISI 250 //equals to 1ms(250*4us)
#define CTC_FACTOR (1000/4/CTC_ISI)
#define PRE_SCALE_MASK (_BV(CS10) | _BV(CS11) | _BV(CS12))

#define TRG_ON 1
#define TRG_OFF 0
#define RAMPDOWN_STEPNUM 50
#define _SET(var,bit) var |= _BV(bit)
#define _CLEAR(var,bit) var &= ~_BV(bit)
uint8_t t_TCCRnA;
uint8_t t_TCCRnB;
uint8_t t_TCCRnC;
/* PWM pin defination on ATmega2560 :
*	pin		PWM source		physical pin	operate interruptor
*	44		OC5C			PL5				p1
*	45		OC5B			PL4				p2
*	46		OC5A			PL3				p3
*
*/

/***********t5_pwm***************/
/***********t5_pwm***************/
inline static void t5_PWM_init() {
	_SET(DDRL, DDL3); //Set PL3 to output mode.
	_CLEAR(PORTL, PORTL3); //Set PL3 to low;
	_SET(DDRL, DDL4); //Set PL4 to output mode.
	_CLEAR(PORTL, PORTL4); //Set PL4 to low;
	_SET(DDRL, DDL5); //Set PL5 to output mode.
	_CLEAR(PORTL, PORTL5); //Set PL5 to low;

	t_TCCRnA = TCCR5A;
	_CLEAR(t_TCCRnA, WGM50); _SET(t_TCCRnA, WGM51); //Fast PWM, TOP: ICR5;
	_SET(t_TCCRnA, COM5A1); _CLEAR(t_TCCRnA, COM5A0); // non-inverting mode;
	_SET(t_TCCRnA, COM5B1); _CLEAR(t_TCCRnA, COM5B0); // non-inverting mode;
	_SET(t_TCCRnA, COM5C1); _CLEAR(t_TCCRnA, COM5C0); // non-inverting mode;

	t_TCCRnB = TCCR5B;
	_SET(t_TCCRnB, CS50); _CLEAR(t_TCCRnB, CS51); _CLEAR(t_TCCRnB, CS52); // No prescaling;
																		  //_SET(t_TCCRnB, WGM52); _CLEAR(t_TCCRnB, WGM53); //Fast PWM, 10bit;
	_SET(t_TCCRnB, WGM52); _SET(t_TCCRnB, WGM53); //Fast PWM, TOP: ICR5;

	ICR5H = 800 >> 8;
	ICR5L = 800; //period of PWM is seted to 50us;

	OCR5AH = 0;
	OCR5AL = 0;

	OCR5BH = 0;
	OCR5BL = 0;

	OCR5CH = 0;
	OCR5CL = 0;

	TCNT5H = 0; //set TCNT5 to 0
	TCNT5L = 0; //set TCNT5 to 0

	TCCR5A = t_TCCRnA;
	TCCR5B = t_TCCRnB;
	TCCR5C = 0;
}

void PWM_write5A(uint16_t data)
{
	OCR5AH = data >> 8;
	OCR5AL = data;
}


void PWM_write5B(uint16_t data)
{
	OCR5BH = data >> 8;
	OCR5BL = data;
}


void PWM_write5C(uint16_t data)
{
	OCR5CH = data >> 8;
	OCR5CL = data;
}


unsigned long ramp50ms_cal(unsigned long data) //data can't be greater than 800.
{
	if (data < 50) return 1;
	return data / RAMPDOWN_STEPNUM;
}

/***********p1***************/
/***********p1***************/
unsigned long t1_ON_DUR;
unsigned long t1_OFF_DUR;
unsigned long t1_PHASE_COUT_NUM;
unsigned long t1_DUR_COUT_NUM;
unsigned long t1_CUR_STATUS;
unsigned long t1_PRETRG_DELAY_COUT_NUM;
unsigned long t1_REAL_PW;
unsigned long t1_POWER_RAMPSTEP;
unsigned long t1_RAMPCOUNTER;
char t1_RAMPDOWN_FLAG;

inline static void t1_init(){
	t_TCCRnB = TCCR1B;
	_CLEAR(t_TCCRnB, CS10); _CLEAR(t_TCCRnB, CS11); _CLEAR(t_TCCRnB, CS12); //Close clock source.
	_SET(t_TCCRnB, WGM12); _CLEAR(t_TCCRnB, WGM13); //CTC mode, TOP: OCR1A;
	TCCR1B = t_TCCRnB;
	TCCR1A = 0;
	TCCR1C = 0;
	TCNT1H = 0; //set TCNT1 to 0
	TCNT1L = 0; //set TCNT1 to 0
	TIMSK1 |= _BV(OCIE1A); // Output Compare A Match Interrupt Enable
}


inline static void _t1_stop(){
	TCCR1B &= ~PRE_SCALE_MASK;
	OCR1AH = 0;
	OCR1AL = 0;
	TCNT1H = 0; //set TCNT1 to 0
	TCNT1L = 0; //set TCNT1 to 0
}

inline void _t1_start(byte pre_scale){
	t1_init();
	TCCR1B |= pre_scale; //set prescale
}

inline static void _set_t1_isi(uint16_t isi){
	OCR1AH = isi >> 8;
	OCR1AL = isi;
}

void t1_init_global()
{
	t1_ON_DUR = 0;
	t1_OFF_DUR = 0;
	t1_PHASE_COUT_NUM = 0;
	t1_DUR_COUT_NUM = 0;
	t1_PRETRG_DELAY_COUT_NUM = 0;
	t1_REAL_PW = 0;
	t1_POWER_RAMPSTEP = 0;
	t1_RAMPDOWN_FLAG = 0;
	t1_RAMPCOUNTER = 0;
}

void t1_operator()
{
	if (t1_PRETRG_DELAY_COUT_NUM > 0){
		t1_PRETRG_DELAY_COUT_NUM--;
		if (t1_PRETRG_DELAY_COUT_NUM == 0) PWM_write5C(t1_REAL_PW);
	}
	else{
			if (t1_DUR_COUT_NUM > 0)
			{
				t1_DUR_COUT_NUM--;
				if (t1_PHASE_COUT_NUM > 0)
				{
					t1_PHASE_COUT_NUM--;
				}
				else
				{
					if ((t1_CUR_STATUS == TRG_ON) && (t1_OFF_DUR > 0))
					{
						t1_PHASE_COUT_NUM = t1_OFF_DUR;
						t1_CUR_STATUS = TRG_OFF;
						//digitalWrite(t1_PIN_, t1_TRG_OFF_VAL);
						PWM_write5C(0);
					}
					else if (t1_ON_DUR > 0)
					{
						t1_PHASE_COUT_NUM = t1_ON_DUR;
						t1_CUR_STATUS = TRG_ON;
						//digitalWrite(t1_PIN_, t1_TRG_ON_VAL);
						PWM_write5C(t1_REAL_PW);
					}
				}
			}
			else if (1 == t1_RAMPDOWN_FLAG) 
			{
				if ((t1_REAL_PW > t1_POWER_RAMPSTEP)&&(t1_RAMPCOUNTER < 50)) 
				{
					t1_REAL_PW = t1_REAL_PW - t1_POWER_RAMPSTEP;
					PWM_write5C(t1_REAL_PW);
					t1_RAMPCOUNTER++;
				}
				else 
				{
					t1_RAMPDOWN_FLAG = 0;
					PWM_write5C(0);
				}
			}
			else
			{
				_t1_stop();
				PWM_write5C(0);
				//digitalWrite(t1_PIN_, t1_TRG_OFF_VAL);
				t1_init_global();
			}
        }
}

ISR(TIMER1_COMPA_vect)
{
	t1_operator();
}

void PWM_PULSE_Class::p1_multipulses(unsigned long duration, float fq, unsigned long p_width, unsigned long pre_trg_delay,int power)
{
	float CYCLE = 1000 / fq;
	if (power > 100)
	{
		Serial.println("Error in Pulse, power can't be greater than 100.");
		return;
	}
	t1_REAL_PW = power * 8;
	if (duration == 0) return;

	//t1_PIN_ = pin;
	t1_DUR_COUT_NUM = duration * CTC_FACTOR - 1;
	t1_PRETRG_DELAY_COUT_NUM = pre_trg_delay * CTC_FACTOR;

	t1_ON_DUR = (p_width > 0) ? (p_width * CTC_FACTOR - 1) : 0;
	t1_OFF_DUR = (CYCLE > p_width) ? ((CYCLE - p_width) * CTC_FACTOR - 1) : 0;

	t1_PHASE_COUT_NUM = t1_ON_DUR;
	t1_CUR_STATUS = TRG_ON;
	_set_t1_isi(CTC_ISI);
	_t1_start(PRE_SCALE_64DIV);
}



void PWM_PULSE_Class::p1_multipulses_ramp50ms(unsigned long duration, float fq, unsigned long p_width, unsigned long pre_trg_delay, int power)
{
	t1_POWER_RAMPSTEP = ramp50ms_cal(power * 8);
	t1_RAMPDOWN_FLAG = 1;
	p1_multipulses(duration, fq, p_width, pre_trg_delay, power);
}

void PWM_PULSE_Class::p1_constant( uint32_t duration, unsigned long pre_trg_delay, int power)
{
	float fq = (float)1000 / 2 / (float)duration;
	p1_multipulses(duration, fq, duration, pre_trg_delay, power);
}

void PWM_PULSE_Class::p1_constant_ramp50ms( uint32_t duration, unsigned long pre_trg_delay, int power)
{
	t1_POWER_RAMPSTEP = ramp50ms_cal(power * 8);
	t1_RAMPDOWN_FLAG = 1;
	p1_constant(duration, pre_trg_delay, power);
}

void PWM_PULSE_Class::p1_cancel()
{
	t1_RAMPDOWN_FLAG = 0;
	t1_DUR_COUT_NUM = 0;
}

void PWM_PULSE_Class::p1_cancel_ramp50ms()
{
	t1_POWER_RAMPSTEP = ramp50ms_cal(t1_REAL_PW);
	t1_RAMPDOWN_FLAG = 1;
	t1_DUR_COUT_NUM = 0;
}

/***********p2***************/
/***********p2***************/

unsigned long t3_ON_DUR;
unsigned long t3_OFF_DUR;
unsigned long t3_PHASE_COUT_NUM;
unsigned long t3_DUR_COUT_NUM;
unsigned long t3_CUR_STATUS;
unsigned long t3_PRETRG_DELAY_COUT_NUM;
unsigned long t3_REAL_PW;
unsigned long t3_POWER_RAMPSTEP;
unsigned long t3_RAMPCOUNTER;
char t3_RAMPDOWN_FLAG;

inline static void t3_init(){
	t_TCCRnB = TCCR3B;
	_CLEAR(t_TCCRnB, CS30); _CLEAR(t_TCCRnB, CS31); _CLEAR(t_TCCRnB, CS32); //Close clock source.
	_SET(t_TCCRnB, WGM32); _CLEAR(t_TCCRnB, WGM33); //CTC mode, TOP: OCR1A;
	TCCR3B = t_TCCRnB;
	TCCR3A = 0;
	TCCR3C = 0;
	TCNT3H = 0; //set TCNT3 to 0
	TCNT3L = 0; //set TCNT3 to 0
	TIMSK3 |= _BV(OCIE3A); // Output Compare A Match Interrupt Enable
}


inline static void _t3_stop(){
	TCCR3B &= ~PRE_SCALE_MASK;
	OCR3AH = 0;
	OCR3AL = 0;
	TCNT3H = 0; //set TCNT3 to 0
	TCNT3L = 0; //set TCNT3 to 0
}

inline void _t3_start(byte pre_scale){
	t3_init();
	TCCR3B |= pre_scale; //set prescale
}

inline static void _set_t3_isi(uint16_t isi){
	OCR3AH = isi >> 8;
	OCR3AL = isi;
}

void t3_init_global()
{
	t3_ON_DUR = 0;
	t3_OFF_DUR = 0;
	t3_PHASE_COUT_NUM = 0;
	t3_DUR_COUT_NUM = 0;
	t3_PRETRG_DELAY_COUT_NUM = 0;
	t3_REAL_PW = 0;
	t3_POWER_RAMPSTEP = 0;
	t3_RAMPDOWN_FLAG = 0;
	t3_RAMPCOUNTER = 0;
}

void t3_operator()
{
	if (t3_PRETRG_DELAY_COUT_NUM > 0){
		t3_PRETRG_DELAY_COUT_NUM--;
		if (t3_PRETRG_DELAY_COUT_NUM == 0) PWM_write5B(t3_REAL_PW);
	}
	else{
			if (t3_DUR_COUT_NUM > 0)
			{
				t3_DUR_COUT_NUM--;
				if (t3_PHASE_COUT_NUM > 0)
				{
					t3_PHASE_COUT_NUM--;
				}
				else
				{
					if ((t3_CUR_STATUS == TRG_ON) && (t3_OFF_DUR > 0))
					{
						t3_PHASE_COUT_NUM = t3_OFF_DUR;
						t3_CUR_STATUS = TRG_OFF;
						//digitalWrite(t3_PIN_, t3_TRG_OFF_VAL);
						PWM_write5B(0);
					}
					else if (t3_ON_DUR > 0)
					{
						t3_PHASE_COUT_NUM = t3_ON_DUR;
						t3_CUR_STATUS = TRG_ON;
						//digitalWrite(t3_PIN_, t3_TRG_ON_VAL);
						PWM_write5B(t3_REAL_PW);
					}
				}
			}
			else if (1 == t3_RAMPDOWN_FLAG) 
			{
				if ((t3_REAL_PW > t3_POWER_RAMPSTEP)&&(t3_RAMPCOUNTER < 50)) 
				{
					t3_REAL_PW = t3_REAL_PW - t3_POWER_RAMPSTEP;
					PWM_write5B(t3_REAL_PW);
					t3_RAMPCOUNTER++;
				}
				else 
				{
					t3_RAMPDOWN_FLAG = 0;
					PWM_write5B(0);
				}
			}
			else
			{
				_t3_stop();
				PWM_write5B(0);
				//digitalWrite(t3_PIN_, t3_TRG_OFF_VAL);
				t3_init_global();
			}
        }
}

ISR(TIMER3_COMPA_vect)
{
	t3_operator();
}

void PWM_PULSE_Class::p2_multipulses(unsigned long duration, float fq, unsigned long p_width, unsigned long pre_trg_delay,int power)
{
	float CYCLE = 1000 / fq;
	if (power > 100)
	{
		Serial.println("Error in Pulse, power can't be greater than 100.");
		return;
	}
	t3_REAL_PW = power * 8;
	if (duration == 0) return;

	//t3_PIN_ = pin;
	t3_DUR_COUT_NUM = duration * CTC_FACTOR - 1;
	t3_PRETRG_DELAY_COUT_NUM = pre_trg_delay * CTC_FACTOR;

	t3_ON_DUR = (p_width > 0) ? (p_width * CTC_FACTOR - 1) : 0;
	t3_OFF_DUR = (CYCLE > p_width) ? ((CYCLE - p_width) * CTC_FACTOR - 1) : 0;

	t3_PHASE_COUT_NUM = t3_ON_DUR;
	t3_CUR_STATUS = TRG_ON;
	_set_t3_isi(CTC_ISI);
	_t3_start(PRE_SCALE_64DIV);
}

void PWM_PULSE_Class::p2_multipulses_ramp50ms(unsigned long duration, float fq, unsigned long p_width, unsigned long pre_trg_delay, int power)
{
	t3_POWER_RAMPSTEP = ramp50ms_cal(power * 8);
	t3_RAMPDOWN_FLAG = 1;
	p2_multipulses(duration, fq, p_width, pre_trg_delay, power);
}

void PWM_PULSE_Class::p2_constant( uint32_t duration, unsigned long pre_trg_delay, int power)
{
	float fq = (float)1000 / 2 / (float)duration;
	p2_multipulses(duration, fq, duration, pre_trg_delay, power);
}

void PWM_PULSE_Class::p2_constant_ramp50ms( uint32_t duration, unsigned long pre_trg_delay, int power)
{
	t3_POWER_RAMPSTEP = ramp50ms_cal(power * 8);
	t3_RAMPDOWN_FLAG = 1;
	p2_constant(duration, pre_trg_delay, power);
}

void PWM_PULSE_Class::p2_cancel()
{
	t3_RAMPDOWN_FLAG = 0;
	t3_DUR_COUT_NUM = 0;
}

void PWM_PULSE_Class::p2_cancel_ramp50ms()
{
	t3_POWER_RAMPSTEP = ramp50ms_cal(t3_REAL_PW);
	t3_RAMPDOWN_FLAG = 1;
	t3_DUR_COUT_NUM = 0;
}

/***********p3***************/
/***********p3***************/
unsigned long t4_ON_DUR;
unsigned long t4_OFF_DUR;
unsigned long t4_PHASE_COUT_NUM;
unsigned long t4_DUR_COUT_NUM;
unsigned long t4_CUR_STATUS;
unsigned long t4_PRETRG_DELAY_COUT_NUM;
unsigned long t4_REAL_PW;
unsigned long t4_POWER_RAMPSTEP;
unsigned long t4_RAMPCOUNTER;
char t4_RAMPDOWN_FLAG;

inline static void t4_init(){
	t_TCCRnB = TCCR4B;
	_CLEAR(t_TCCRnB, CS40); _CLEAR(t_TCCRnB, CS41); _CLEAR(t_TCCRnB, CS42); //Close clock source.
	_SET(t_TCCRnB, WGM42); _CLEAR(t_TCCRnB, WGM43); //CTC mode, TOP: OCR1A;
	TCCR4B = t_TCCRnB;
	TCCR4A = 0;
	TCCR4C = 0;
	TCNT4H = 0; //set TCNT4 to 0
	TCNT4L = 0; //set TCNT4 to 0
	TIMSK4 |= _BV(OCIE4A); // Output Compare A Match Interrupt Enable
}


inline static void _t4_stop(){
	TCCR4B &= ~PRE_SCALE_MASK;
	OCR4AH = 0;
	OCR4AL = 0;
	TCNT4H = 0; //set TCNT4 to 0
	TCNT4L = 0; //set TCNT4 to 0
}

inline void _t4_start(byte pre_scale){
	t4_init();
	TCCR4B |= pre_scale; //set prescale
}

inline static void _set_t4_isi(uint16_t isi){
	OCR4AH = isi >> 8;
	OCR4AL = isi;
}

void t4_init_global()
{
	t4_ON_DUR = 0;
	t4_OFF_DUR = 0;
	t4_PHASE_COUT_NUM = 0;
	t4_DUR_COUT_NUM = 0;
	t4_PRETRG_DELAY_COUT_NUM = 0;
	t4_REAL_PW = 0;
	t4_POWER_RAMPSTEP = 0;
	t4_RAMPDOWN_FLAG = 0;
	t4_RAMPCOUNTER = 0;
}

void t4_operator()
{
	if (t4_PRETRG_DELAY_COUT_NUM > 0){
		t4_PRETRG_DELAY_COUT_NUM--;
		if (t4_PRETRG_DELAY_COUT_NUM == 0) PWM_write5A(t4_REAL_PW);
	}
	else{
			if (t4_DUR_COUT_NUM > 0)
			{
				t4_DUR_COUT_NUM--;
				if (t4_PHASE_COUT_NUM > 0)
				{
					t4_PHASE_COUT_NUM--;
				}
				else
				{
					if ((t4_CUR_STATUS == TRG_ON) && (t4_OFF_DUR > 0))
					{
						t4_PHASE_COUT_NUM = t4_OFF_DUR;
						t4_CUR_STATUS = TRG_OFF;
						//digitalWrite(t4_PIN_, t4_TRG_OFF_VAL);
						PWM_write5A(0);
					}
					else if (t4_ON_DUR > 0)
					{
						t4_PHASE_COUT_NUM = t4_ON_DUR;
						t4_CUR_STATUS = TRG_ON;
						//digitalWrite(t4_PIN_, t4_TRG_ON_VAL);
						PWM_write5A(t4_REAL_PW);
					}
				}
			}
			else if (1 == t4_RAMPDOWN_FLAG) 
			{
				if ((t4_REAL_PW > t4_POWER_RAMPSTEP)&&(t4_RAMPCOUNTER < 50)) 
				{
					t4_REAL_PW = t4_REAL_PW - t4_POWER_RAMPSTEP;
					PWM_write5A(t4_REAL_PW);
					t4_RAMPCOUNTER++;
				}
				else 
				{
					t4_RAMPDOWN_FLAG = 0;
					PWM_write5A(0);
				}
			}
			else
			{
				_t4_stop();
				PWM_write5A(0);
				//digitalWrite(t4_PIN_, t4_TRG_OFF_VAL);
				t4_init_global();
			}
        }
}

ISR(TIMER4_COMPA_vect)
{
	t4_operator();
}

void PWM_PULSE_Class::p3_multipulses(unsigned long duration, float fq, unsigned long p_width, unsigned long pre_trg_delay,int power)
{
	float CYCLE = 1000 / fq;
	if (power > 100)
	{
		Serial.println("Error in Pulse, power can't be greater than 100.");
		return;
	}
	t4_REAL_PW = power * 8;
	if (duration == 0) return;

	//t4_PIN_ = pin;
	t4_DUR_COUT_NUM = duration * CTC_FACTOR - 1;
	t4_PRETRG_DELAY_COUT_NUM = pre_trg_delay * CTC_FACTOR;

	t4_ON_DUR = (p_width > 0) ? (p_width * CTC_FACTOR - 1) : 0;
	t4_OFF_DUR = (CYCLE > p_width) ? ((CYCLE - p_width) * CTC_FACTOR - 1) : 0;

	t4_PHASE_COUT_NUM = t4_ON_DUR;
	t4_CUR_STATUS = TRG_ON;
	_set_t4_isi(CTC_ISI);
	_t4_start(PRE_SCALE_64DIV);
}

void PWM_PULSE_Class::p3_multipulses_ramp50ms(unsigned long duration, float fq, unsigned long p_width, unsigned long pre_trg_delay, int power)
{
	t4_POWER_RAMPSTEP = ramp50ms_cal(power * 8);
	t4_RAMPDOWN_FLAG = 1;
	p3_multipulses(duration, fq, p_width, pre_trg_delay, power);
}

void PWM_PULSE_Class::p3_constant( uint32_t duration, unsigned long pre_trg_delay, int power)
{
	float fq = (float)1000 / 2 / (float)duration;
	p3_multipulses(duration, fq, duration, pre_trg_delay, power);
}

void PWM_PULSE_Class::p3_constant_ramp50ms( uint32_t duration, unsigned long pre_trg_delay, int power)
{
	t4_POWER_RAMPSTEP = ramp50ms_cal(power * 8);
	t4_RAMPDOWN_FLAG = 1;
	p3_constant(duration, pre_trg_delay, power);
}

void PWM_PULSE_Class::p3_cancel()
{
	t4_RAMPDOWN_FLAG = 0;
	t4_DUR_COUT_NUM = 0;
}

void PWM_PULSE_Class::p3_cancel_ramp50ms()
{
	t4_POWER_RAMPSTEP = ramp50ms_cal(t4_REAL_PW);
	t4_RAMPDOWN_FLAG = 1;
	t4_DUR_COUT_NUM = 0;
}

/***************init********************/
/***************init********************/
void PWM_PULSE_Class::init()
{
	t5_PWM_init();
	sei();

}


PWM_PULSE_Class PWM_PULSE;