#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>


#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define SARCLK 18000000L

#define VDD 3.3035
#define VMID (VDD/2.0)


#define LCD_RS P1_7
#define LCD_E  P2_0
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0
#define CHARS_PER_LINE 16


#define SERVO_PIN P2_5   // Servo signal on P2.5

char _c51_external_startup (void)
{
	SFRPAGE = 0x00;
	WDTCN = 0xDE;
	WDTCN = 0xAD;

	VDM0CN=0x80;
	RSTSRC=0x02|0x04;

	#if (SYSCLK == 48000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x10;
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20;
		SFRPAGE = 0x00;
	#endif

	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif

	P0MDOUT |= 0x10;
	XBR0     = 0x01;
	XBR1     = 0x00;
	XBR2     = 0x40;

	
	SFRPAGE = 0x20;        
	P2MDOUT |= 0x20;         
	SFRPAGE = 0x00;

	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif

	SCON0 = 0x10;
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;
	TMOD &= ~0xf0;
	TMOD |=  0x20;
	TR1 = 1;
	TI = 1;

	return 0;
}

void InitADC (void)
{
	SFRPAGE = 0x00;
	ADEN=0;

	ADC0CN1=
		(0x2 << 6) |
        (0x0 << 3) |
		(0x0 << 0) ;

	ADC0CF0=
	    ((SYSCLK/SARCLK) << 3) |
		(0x0 << 2);

	ADC0CF1=
		(0 << 7)   |
		(0x1E << 0);

	ADC0CN0 =
		(0x0 << 7) |
		(0x0 << 6) |
		(0x0 << 5) |
		(0x0 << 4) |
		(0x0 << 3) |
		(0x0 << 2) |
		(0x0 << 0) ;

	ADC0CF2=
		(0x0 << 7) |
		(0x1 << 5) |
		(0x1F << 0);

	ADC0CN2 =
		(0x0 << 7) |
		(0x0 << 0) ;

	ADEN=1;
}

void Timer3us(unsigned char us)
{
	unsigned char i;

	CKCON0|=0b_0100_0000;
	TMR3RL = (-(SYSCLK)/1000000L);
	TMR3 = TMR3RL;
	TMR3CN0 = 0x04;

	for (i = 0; i < us; i++)
	{
		while (!(TMR3CN0 & 0x80));
		TMR3CN0 &= ~(0x80);
	}

	TMR3CN0 = 0 ;
}

void waitms (unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0; j<ms; j++)
		for (k=0; k<4; k++) Timer3us(250);
}

void InitPinADC (unsigned char portno, unsigned char pinno)
{
	unsigned char mask;
	mask=1<<pinno;

	SFRPAGE = 0x20;
	switch (portno)
	{
		case 0:
			P0MDIN &= (~mask);
			P0SKIP |= mask;
		break;
		case 1:
			P1MDIN &= (~mask);
			P1SKIP |= mask;
		break;
		case 2:
			P2MDIN &= (~mask);
			P2SKIP |= mask;
		break;
		default:
		break;
	}
	SFRPAGE = 0x00;
}

unsigned int ADC_at_Pin(unsigned char pin)
{
	ADC0MX = pin;
	ADINT = 0;
	ADBUSY = 1;
	while (!ADINT);
	return (ADC0);
}

float Volts_at_Pin(unsigned char pin)
{
	return ((ADC_at_Pin(pin)*VDD)/0b_0011_1111_1111_1111);
}


void LCD_pulse (void)
{
	LCD_E=1;
	Timer3us(40);
	LCD_E=0;
}

void LCD_byte (unsigned char x)
{
	ACC=x;
	LCD_D7=ACC_7;
	LCD_D6=ACC_6;
	LCD_D5=ACC_5;
	LCD_D4=ACC_4;
	LCD_pulse();
	Timer3us(40);

	ACC=x;
	LCD_D7=ACC_3;
	LCD_D6=ACC_2;
	LCD_D5=ACC_1;
	LCD_D4=ACC_0;
	LCD_pulse();
}

void WriteData (unsigned char x)
{
	LCD_RS=1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand (unsigned char x)
{
	LCD_RS=0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT (void)
{
	LCD_E=0;
	waitms(20);
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32);
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01);
	waitms(20);
}

void LCDprint(char * string, unsigned char line, bit clear)
{
	int j;
	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' ');
}


void Timer0_Init16bit(void)
{
	TMOD &= ~0x03;
	TMOD |=  0x01;
	CKCON0 &= ~0x03;     
	TR0 = 0;
}

void Timer0_ResetStart(void)
{
	TR0 = 0;
	TH0 = 0;
	TL0 = 0;
	TF0 = 0;
	TR0 = 1;
}

unsigned int Timer0_ReadStop(void)
{
	unsigned int t;
	TR0 = 0;
	t = ((unsigned int)TH0<<8) | TL0;
	return t;
}

float CountsToSeconds(unsigned int counts)
{
	return ((float)counts * 12.0f) / (float)SYSCLK;
}

void WaitForMidCross(unsigned char pin, bit rising)
{
	float v_prev;
	float v_now;

	v_prev = Volts_at_Pin(pin);

	while(1)
	{
		v_now = Volts_at_Pin(pin);

		if(rising)
		{
			if((v_prev < VMID) && (v_now >= VMID)) return;
		}
		else
		{
			if((v_prev > VMID) && (v_now <= VMID)) return;
		}

		v_prev = v_now;
	}
}

float Measure_HalfPeriod(unsigned char pin)
{
	unsigned int counts;
	float t_half;

	WaitForMidCross(pin, 1);
	Timer0_ResetStart();
	WaitForMidCross(pin, 0);
	counts = Timer0_ReadStop();
	t_half = CountsToSeconds(counts);
	return t_half;
}

float OneShotPeak(unsigned char pin, float t_half)
{
	unsigned long delay_us;
	float v_sample;
	float v_peak;

	delay_us = (unsigned long)((t_half * 0.5f) * 1000000.0f);

	WaitForMidCross(pin, 1);

	while(delay_us >= 250)
	{
		Timer3us(250);
		delay_us -= 250;
	}
	if(delay_us > 0) Timer3us((unsigned char)delay_us);

	v_sample = Volts_at_Pin(pin);
	v_peak = v_sample - VMID;
	if(v_peak < 0) v_peak = -v_peak;
	return v_peak;
}


volatile unsigned long t0_ovf = 0;

void Timer0_ResetStart32(void)
{
	TR0 = 0;
	TH0 = 0;
	TL0 = 0;
	TF0 = 0;
	t0_ovf = 0;
	TR0 = 1;
}

unsigned long Timer0_ReadStop32(void)
{
	unsigned long t;
	TR0 = 0;
	t = ((unsigned long)t0_ovf << 16) | ((unsigned long)TH0 << 8) | (unsigned long)TL0;
	return t;
}

float Counts32ToSeconds(unsigned long counts)
{
	return ((float)counts * 12.0f) / (float)SYSCLK;
}

void WaitForMidCross_T0(unsigned char pin, bit rising)
{
	float v_prev;
	float v_now;

	v_prev = Volts_at_Pin(pin);

	while(1)
	{
		if(TF0)
		{
			TF0 = 0;
			t0_ovf++;
		}

		v_now = Volts_at_Pin(pin);

		if(rising)
		{
			if((v_prev < VMID) && (v_now >= VMID)) return;
		}
		else
		{
			if((v_prev > VMID) && (v_now <= VMID)) return;
		}

		v_prev = v_now;
	}
}

float Measure_Period(unsigned char ref)
{
	unsigned long counts;
	float T;

	WaitForMidCross_T0(ref, 1);
	Timer0_ResetStart32();
	WaitForMidCross_T0(ref, 1);
	counts = Timer0_ReadStop32();
	T = Counts32ToSeconds(counts);
	return T;
}

float Measure_Phase_Signed(unsigned char ref, unsigned char test)
{
	float T;
	float dt_rt;
	float dt_tr;
	unsigned long counts;
	float phase;

	T = Measure_Period(ref);
	if(T <= 0.0f) return 0.0f;

	
	WaitForMidCross_T0(ref, 1);
	Timer0_ResetStart32();
	WaitForMidCross_T0(test, 1);
	counts = Timer0_ReadStop32();
	dt_rt = Counts32ToSeconds(counts);


	WaitForMidCross_T0(test, 1);
	Timer0_ResetStart32();
	WaitForMidCross_T0(ref, 1);
	counts = Timer0_ReadStop32();
	dt_tr = Counts32ToSeconds(counts);

	if(dt_rt <= dt_tr) phase = (dt_rt / T) * 360.0f;    
	else              phase = - (dt_tr / T) * 360.0f;    

	if(phase > 180.0f) phase -= 360.0f;
	if(phase < -180.0f) phase += 360.0f;

	return phase;
}


void delay_us_long(unsigned int us)
{
	while (us >= 250)
	{
		Timer3us(250);
		us -= 250;
	}
	if (us > 0) Timer3us((unsigned char)us);
}


void servo_pulse_abs(int angle_deg_abs)
{
	unsigned int pulse_us;
	unsigned int low_us;

	if(angle_deg_abs < 0) angle_deg_abs = -angle_deg_abs;
	if(angle_deg_abs > 180) angle_deg_abs = 180;

	pulse_us = 1000u + (unsigned int)(((long)angle_deg_abs * 1000L) / 180L); // 1000..2000
	low_us   = 20000u - pulse_us;

	SERVO_PIN = 1;
	delay_us_long(pulse_us);
	SERVO_PIN = 0;
	delay_us_long(low_us);
}


void servo_hold_abs(int angle_deg_abs, unsigned int hold_ms)
{
	unsigned int frames, i;
	frames = hold_ms / 20;
	if(frames == 0) frames = 1;

	for(i=0; i<frames; i++)
		servo_pulse_abs(angle_deg_abs);
}


void main (void)
{
	char xdata line1[17];
	char xdata line2[17];

	float t_half;
	float freq;
	float v1_peak, v2_peak;
	float v1_rms, v2_rms;
	float phase;

	InitPinADC(2, 2);       
	InitPinADC(2, 3);       
	InitADC();

	Timer0_Init16bit();
	LCD_4BIT();

	
	SERVO_PIN = 0;
	servo_hold_abs(0, 400); 

	waitms(200);

	while(1)
	{
		int freq_x10;
		int phase_i;
		int v1_cV, v2_cV;
		int servo_angle_abs;

		t_half = Measure_HalfPeriod(QFP32_MUX_P2_2);
		freq   = 1.0f / (2.0f * t_half);

		v1_peak = OneShotPeak(QFP32_MUX_P2_2, t_half);
		v2_peak = OneShotPeak(QFP32_MUX_P2_3, t_half);

		v1_rms = v1_peak * 0.70710678f;
		v2_rms = v2_peak * 0.70710678f;

		phase = Measure_Phase_Signed(QFP32_MUX_P2_2, QFP32_MUX_P2_3);

		
		freq_x10 = (int)(freq * 10.0f + 0.5f);
		phase_i  = (int)(phase + (phase >= 0.0f ? 0.5f : -0.5f));
		v1_cV    = (int)(v1_rms * 100.0f + 0.5f);
		v2_cV    = (int)(v2_rms * 100.0f + 0.5f);

		sprintf(line1, "F=%3d.%1dHz P=%+4d", freq_x10/10, freq_x10%10, phase_i);
		LCDprint(line1, 1, 1);

		sprintf(line2, "V1=%1d.%02d V2=%1d.%02d",
		        v1_cV/100, v1_cV%100,
		        v2_cV/100, v2_cV%100);
		LCDprint(line2, 2, 1);

		
		servo_angle_abs = phase_i;
		if(servo_angle_abs < 0) servo_angle_abs = -servo_angle_abs;
		if(servo_angle_abs > 180) servo_angle_abs = 180;

		
		servo_hold_abs(servo_angle_abs, 200);
	}
}
