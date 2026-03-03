// Lab5_ADC_MinimalChange_LCD.c                                           // File name comment
// Minimal-change version of prof's ADC.c + your LCD_4bit.c                // What this file is
// Measures Frequency, Vrms of CH1/CH2, and Phase using ADC midpoint       // What it measures
// Displays results on LCD                                                 // Output device

#include <stdio.h>                                                        // Needed for sprintf
#include <stdlib.h>                                                       // Standard library
#include <EFM8LB1.h>                                                      // EFM8LB1 register definitions



#define SYSCLK 72000000L                                                  // System clock frequency
#define BAUDRATE 115200L                                                  // UART baud rate (kept from prof code)
#define SARCLK 18000000L                                                  // ADC SAR clock (max 18 MHz)

#define VDD 3.3035                                                        // Measured VDD in volts (use your DMM value)
#define VMID (VDD/2.0)                                                    // Midpoint voltage (~1.65 V) used as "zero-cross"

// ---------------- LCD pin defines (copied from your LCD_4bit.c) --------
#define LCD_RS P1_7                                                       // LCD RS pin
// #define LCD_RW Px_x                                                     // LCD RW not used (tied to GND)
#define LCD_E  P2_0                                                       // LCD Enable pin (IMPORTANT: don't use P2.0 for ADC)
#define LCD_D4 P1_3                                                       // LCD D4 pin
#define LCD_D5 P1_2                                                       // LCD D5 pin
#define LCD_D6 P1_1                                                       // LCD D6 pin
#define LCD_D7 P1_0                                                       // LCD D7 pin
#define CHARS_PER_LINE 16                                                 // LCD line length

// ---------------------- PROF STARTUP (unchanged style) ------------------
char _c51_external_startup (void)                                         // Startup function
{
	SFRPAGE = 0x00;                                                        // Use default SFR page
	WDTCN = 0xDE;                                                          // Disable watchdog key 1
	WDTCN = 0xAD;                                                          // Disable watchdog key 2

	VDM0CN=0x80;                                                           // Enable VDD monitor
	RSTSRC=0x02|0x04;                                                      // Reset on missing clock + low VDD

	#if (SYSCLK == 48000000L)	                                           // If 48 MHz
		SFRPAGE = 0x10;                                                    // Flash timing page
		PFE0CN  = 0x10;                                                    // Flash timing for <50 MHz
		SFRPAGE = 0x00;                                                    // Back to default
	#elif (SYSCLK == 72000000L)                                            // If 72 MHz
		SFRPAGE = 0x10;                                                    // Flash timing page
		PFE0CN  = 0x20;                                                    // Flash timing for <75 MHz
		SFRPAGE = 0x00;                                                    // Back to default
	#endif                                                                 // End flash timing selection

	#if (SYSCLK == 12250000L)                                              // If 12.25 MHz
		CLKSEL = 0x10;                                                     // Select that clock
		CLKSEL = 0x10;                                                     // Write twice
		while ((CLKSEL & 0x80) == 0);                                       // Wait stable
	#elif (SYSCLK == 24500000L)                                            // If 24.5 MHz
		CLKSEL = 0x00;                                                     // Select 24.5 MHz
		CLKSEL = 0x00;                                                     // Write twice
		while ((CLKSEL & 0x80) == 0);                                       // Wait stable
	#elif (SYSCLK == 48000000L)	                                           // If 48 MHz
		CLKSEL = 0x00;                                                     // Transition via 24.5 first
		CLKSEL = 0x00;                                                     // Write twice
		while ((CLKSEL & 0x80) == 0);                                       // Wait stable
		CLKSEL = 0x07;                                                     // Select 48 MHz
		CLKSEL = 0x07;                                                     // Write twice
		while ((CLKSEL & 0x80) == 0);                                       // Wait stable
	#elif (SYSCLK == 72000000L)                                            // If 72 MHz
		CLKSEL = 0x00;                                                     // Transition via 24.5 first
		CLKSEL = 0x00;                                                     // Write twice
		while ((CLKSEL & 0x80) == 0);                                       // Wait stable
		CLKSEL = 0x03;                                                     // Select 72 MHz
		CLKSEL = 0x03;                                                     // Write twice
		while ((CLKSEL & 0x80) == 0);                                       // Wait stable
	#else                                                                  // Otherwise
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L // Compile-time guard
	#endif                                                                 // End SYSCLK selection

	P0MDOUT |= 0x10;                                                       // UART0 TX push-pull (P0.4)
	XBR0     = 0x01;                                                       // Enable UART0 on P0.4/P0.5
	XBR1     = 0X00;                                                       // No extra peripherals
	XBR2     = 0x40;                                                       // Enable crossbar + weak pull-ups

	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)                               // Check timer reload validity
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF // Guard
	#endif                                                                 // End guard

	SCON0 = 0x10;                                                          // UART0 mode 1, RX enabled
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));                              // Timer1 reload for baud
	TL1 = TH1;                                                             // Init Timer1 low
	TMOD &= ~0xf0;                                                         // Clear Timer1 bits
	TMOD |=  0x20;                                                         // Timer1 8-bit auto reload
	TR1 = 1;                                                               // Start Timer1
	TI = 1;                                                                // TX ready

	return 0;                                                              // Done
}

// ---------------------- PROF ADC INIT (same as your pasted code) --------
void InitADC (void)                                                       // Initialize ADC
{
	SFRPAGE = 0x00;                                                        // Default page
	ADEN=0;                                                                // Disable ADC while configuring

	ADC0CN1=                                                                // ADC0CN1 config
		(0x2 << 6) |                                                        // 14-bit mode
        (0x0 << 3) |                                                       // No shift
		(0x0 << 0) ;                                                        // No accumulation

	ADC0CF0=                                                                // ADC0CF0 config
	    ((SYSCLK/SARCLK) << 3) |                                           // SAR clock divider
		(0x0 << 2);                                                         // ADCCLK source = SYSCLK

	ADC0CF1=                                                                // ADC0CF1 config
		(0 << 7)   |                                                        // Low power disabled
		(0x1E << 0);                                                        // Tracking time

	ADC0CN0 =                                                               // ADC0CN0 config
		(0x0 << 7) |                                                        // ADEN bit in this register = 0
		(0x0 << 6) |                                                        // No idle powerdown
		(0x0 << 5) |                                                        // ADINT cleared
		(0x0 << 4) |                                                        // ADBUSY cleared
		(0x0 << 3) |                                                        // Window compare off
		(0x0 << 2) |                                                        // Gain=1
		(0x0 << 0) ;                                                        // Temp sensor off

	ADC0CF2=                                                                // ADC0CF2 config
		(0x0 << 7) |                                                        // Reference is GND pin
		(0x1 << 5) |                                                        // Reference is VDD
		(0x1F << 0);                                                        // Power-up delay

	ADC0CN2 =                                                               // ADC0CN2 config
		(0x0 << 7) |                                                        // Accumulator overwrite
		(0x0 << 0) ;                                                        // ADC trigger = software

	ADEN=1;                                                                // Enable ADC
}

// ---------------------- PROF Timer3 delays (unchanged) ------------------
void Timer3us(unsigned char us)                                            // Delay in microseconds
{
	unsigned char i;                                                       // Loop counter

	CKCON0|=0b_0100_0000;                                                   // Timer3 uses SYSCLK
	TMR3RL = (-(SYSCLK)/1000000L);                                          // Reload for 1us overflow
	TMR3 = TMR3RL;                                                         // Initialize Timer3
	TMR3CN0 = 0x04;                                                        // Start Timer3

	for (i = 0; i < us; i++)                                                // Repeat us times
	{
		while (!(TMR3CN0 & 0x80));                                          // Wait overflow
		TMR3CN0 &= ~(0x80);                                                 // Clear overflow flag
	}

	TMR3CN0 = 0 ;                                                          // Stop Timer3
}

void waitms (unsigned int ms)                                              // Delay in milliseconds
{
	unsigned int j;                                                        // Outer loop
	unsigned char k;                                                       // Inner loop
	for(j=0; j<ms; j++)                                                    // Loop ms times
		for (k=0; k<4; k++) Timer3us(250);                                  // 4*250us = 1ms
}

// ---------------------- PROF InitPinADC (unchanged) ---------------------
void InitPinADC (unsigned char portno, unsigned char pinno)                // Configure a pin as analog
{
	unsigned char mask;                                                    // Bit mask
	mask=1<<pinno;                                                         // Compute mask

	SFRPAGE = 0x20;                                                        // Port config page
	switch (portno)                                                        // Choose port
	{
		case 0:                                                             // Port 0
			P0MDIN &= (~mask);                                              // Set as analog
			P0SKIP |= mask;                                                 // Skip crossbar
		break;                                                              // End case
		case 1:                                                             // Port 1
			P1MDIN &= (~mask);                                              // Set as analog
			P1SKIP |= mask;                                                 // Skip crossbar
		break;                                                              // End case
		case 2:                                                             // Port 2
			P2MDIN &= (~mask);                                              // Set as analog
			P2SKIP |= mask;                                                 // Skip crossbar
		break;                                                              // End case
		default:                                                            // Unknown port
		break;                                                              // Do nothing
	}
	SFRPAGE = 0x00;                                                        // Back to default
}

// ---------------------- PROF ADC read (unchanged) -----------------------
unsigned int ADC_at_Pin(unsigned char pin)                                 // Return raw ADC counts
{
	ADC0MX = pin;                                                          // Select mux pin
	ADINT = 0;                                                             // Clear flag
	ADBUSY = 1;                                                            // Start conversion
	while (!ADINT);                                                        // Wait complete
	return (ADC0);                                                         // Return result
}

float Volts_at_Pin(unsigned char pin)                                      // Return voltage at pin
{
	return ((ADC_at_Pin(pin)*VDD)/0b_0011_1111_1111_1111);                  // Convert 14-bit code to volts
}

// =======================================================================
// ======================= ADDED: LCD FUNCTIONS (exact from your file) ====
// =======================================================================

void LCD_pulse (void)                                                      // Pulse enable pin
{
	LCD_E=1;                                                               // Enable high
	Timer3us(40);                                                          // Wait
	LCD_E=0;                                                               // Enable low
}

void LCD_byte (unsigned char x)                                            // Send one byte (4-bit)
{
	ACC=x;                                                                 // Put byte in accumulator
	LCD_D7=ACC_7;                                                          // Send high nibble bit 7
	LCD_D6=ACC_6;                                                          // Send high nibble bit 6
	LCD_D5=ACC_5;                                                          // Send high nibble bit 5
	LCD_D4=ACC_4;                                                          // Send high nibble bit 4
	LCD_pulse();                                                           // Pulse enable
	Timer3us(40);                                                          // Small delay

	ACC=x;                                                                 // Reload accumulator
	LCD_D7=ACC_3;                                                          // Send low nibble bit 3
	LCD_D6=ACC_2;                                                          // Send low nibble bit 2
	LCD_D5=ACC_1;                                                          // Send low nibble bit 1
	LCD_D4=ACC_0;                                                          // Send low nibble bit 0
	LCD_pulse();                                                           // Pulse enable
}

void WriteData (unsigned char x)                                           // Write data to LCD
{
	LCD_RS=1;                                                              // RS=1 means data
	LCD_byte(x);                                                           // Send byte
	waitms(2);                                                             // Wait
}

void WriteCommand (unsigned char x)                                        // Write command to LCD
{
	LCD_RS=0;                                                              // RS=0 means command
	LCD_byte(x);                                                           // Send byte
	waitms(5);                                                             // Wait longer
}

void LCD_4BIT (void)                                                      // Initialize LCD
{
	LCD_E=0;                                                               // E starts low
	waitms(20);                                                            // Startup delay
	WriteCommand(0x33);                                                    // Force 8-bit mode
	WriteCommand(0x33);                                                    // Force 8-bit mode again
	WriteCommand(0x32);                                                    // Switch to 4-bit mode
	WriteCommand(0x28);                                                    // 2 lines, 5x7
	WriteCommand(0x0c);                                                    // Display ON, cursor OFF
	WriteCommand(0x01);                                                    // Clear display
	waitms(20);                                                            // Wait clear
}

void LCDprint(char * string, unsigned char line, bit clear)                // Print string on LCD
{
	int j;                                                                 // Index
	WriteCommand(line==2?0xc0:0x80);                                        // Set cursor line
	waitms(5);                                                             // Wait
	for(j=0; string[j]!=0; j++)	WriteData(string[j]);                      // Write chars
	if(clear) for(; j<CHARS_PER_LINE; j++) WriteData(' ');                  // Clear rest of line
}

// =======================================================================
// ======================= ADDED: MEASUREMENT HELPERS =====================
// =======================================================================

// -------- Timer0 setup (new) --------
void Timer0_Init16bit(void)                                                // Init Timer0 for timing
{
	TMOD &= ~0x03;                                                         // Clear Timer0 mode bits
	TMOD |=  0x01;                                                         // Mode 1 = 16-bit
	CKCON0 &= ~0x03;                                                       // Timer0 clock = SYSCLK/12
	TR0 = 0;                                                               // Stop Timer0
}

void Timer0_ResetStart(void)                                               // Reset and start Timer0
{
	TR0 = 0;                                                               // Stop timer
	TH0 = 0;                                                               // Clear high byte
	TL0 = 0;                                                               // Clear low byte
	TF0 = 0;                                                               // Clear overflow flag
	TR0 = 1;                                                               // Start timer
}

unsigned int Timer0_ReadStop(void)                                         // Stop and read Timer0
{
	unsigned int t;                                                        // Variable to store count
	TR0 = 0;                                                               // Stop timer
	t = ((unsigned int)TH0<<8) | TL0;                                      // Combine bytes
	return t;                                                              // Return counts
}

float CountsToSeconds(unsigned int counts)                                 // Convert timer counts to seconds
{
	return ((float)counts * 12.0f) / (float)SYSCLK;                        // seconds = counts / (SYSCLK/12)
}

// -------- ADC midpoint crossing (new) --------
void WaitForMidCross(unsigned char pin, bit rising)                        // Wait for VMID crossing
{
	float v_prev;                                                          // Previous voltage sample
	float v_now;                                                           // Current voltage sample

	v_prev = Volts_at_Pin(pin);                                            // Take initial sample

	while(1)                                                               // Loop until crossing found
	{
		v_now = Volts_at_Pin(pin);                                         // Take next sample

		if(rising)                                                         // If rising crossing wanted
		{
			if((v_prev < VMID) && (v_now >= VMID)) return;                 // Below->above means crossing
		}
		else                                                               // Otherwise falling crossing wanted
		{
			if((v_prev > VMID) && (v_now <= VMID)) return;                 // Above->below means crossing
		}

		v_prev = v_now;                                                    // Update previous
	}
}

// -------- Half period measurement (new) --------
float Measure_HalfPeriod(unsigned char pin)                                 // Measure T/2 using rising->falling
{
	unsigned int counts;                                                    // Timer counts
	float t_half;                                                           // Half period in seconds

	WaitForMidCross(pin, 1);                                                // Wait rising midpoint
	Timer0_ResetStart();                                                    // Start timing
	WaitForMidCross(pin, 0);                                                // Wait falling midpoint (half cycle later)
	counts = Timer0_ReadStop();                                             // Stop+read timer
	t_half = CountsToSeconds(counts);                                       // Convert to seconds
	return t_half;                                                          // Return half period
}

// -------- One-shot peak measurement (new) --------
float OneShotPeak(unsigned char pin, float t_half)                          // Sample near peak using T/4 delay
{
	unsigned long delay_us;                                                 // Delay in microseconds
	float v_sample;                                                         // Sampled voltage
	float v_peak;                                                           // Peak above midpoint

	delay_us = (unsigned long)((t_half * 0.5f) * 1000000.0f);               // T/4 = (T/2)/2 = t_half/2

	WaitForMidCross(pin, 1);                                                // Sync at rising midpoint

	while(delay_us >= 250)                                                  // Delay in chunks (Timer3us max ~255)
	{
		Timer3us(250);                                                      // Delay 250us
		delay_us -= 250;                                                    // Subtract
	}

	if(delay_us > 0) Timer3us((unsigned char)delay_us);                     // Delay remainder

	v_sample = Volts_at_Pin(pin);                                           // Sample at ~peak time
	v_peak = v_sample - VMID;                                               // Convert to peak above midpoint
	if(v_peak < 0) v_peak = -v_peak;                                        // Make magnitude positive
	return v_peak;                                                          // Return peak magnitude
}

// -------- Phase measurement (new) --------
float Measure_Phase(unsigned char ref, unsigned char test, float t_half)    // Phase using time between mid crossings
{
	float T;                                                                // Full period seconds
	unsigned int counts;                                                    // Delta counts
	float dt;                                                               // Delta time seconds
	float phase;                                                            // Phase degrees

	T = 2.0f * t_half;                                                      // Full period = 2*(T/2)

	WaitForMidCross(ref, 1);                                                // Wait reference rising midpoint
	Timer0_ResetStart();                                                    // Start timer at reference crossing
	WaitForMidCross(test, 1);                                               // Wait test rising midpoint
	counts = Timer0_ReadStop();                                             // Stop and read delta time

	dt = CountsToSeconds(counts);                                           // Convert dt to seconds
	phase = (dt / T) * 360.0f;                                              // Convert to degrees

	if(phase > 180.0f) phase -= 360.0f;                                     // Wrap to (-180..180)

	return phase;                                                           // Return phase
}

// =======================================================================
// ======================= MINIMAL-CHANGE MAIN() ==========================
// =======================================================================

void main (void)                                                           // Main program
{
	char line1[17];                                                        // LCD line 1 buffer
	char line2[17];                                                        // LCD line 2 buffer

	float t_half;                                                          // Half period
	float freq;                                                            // Frequency
	float v1_peak;                                                         // CH1 peak above midpoint
	float v2_peak;                                                         // CH2 peak above midpoint
	float v1_rms;                                                          // CH1 RMS
	float v2_rms;                                                          // CH2 RMS
	float phase;                                                           // Phase degrees

	// --- Minimal change from prof ADC.c: keep pin setup style -------------
	InitPinADC(2, 2);                                                       // Configure P2.2 analog input (CH1)
	InitPinADC(2, 3);                                                       // Configure P2.3 analog input (CH2)
	InitADC();                                                              // Initialize ADC (prof function)

	Timer0_Init16bit();                                                     // Initialize Timer0 (added)
	LCD_4BIT();                                                             // Initialize LCD (added)

	waitms(200);                                                            // Small startup delay

	while(1)                                                                // Forever loop (like prof code)
	{
		// 1) Measure half period from reference (CH1)                        // Step explanation
		t_half = Measure_HalfPeriod(QFP32_MUX_P2_2);                         // Measure T/2 using midpoint crossings

		// 2) Convert to frequency                                             // Step explanation
		freq = 1.0f / (2.0f * t_half);                                       // f = 1/T = 1/(2*t_half)

		// 3) One-shot peak sample for both channels                           // Step explanation
		v1_peak = OneShotPeak(QFP32_MUX_P2_2, t_half);                        // Peak magnitude of CH1
		v2_peak = OneShotPeak(QFP32_MUX_P2_3, t_half);                        // Peak magnitude of CH2

		// 4) Convert peak to RMS                                              // Step explanation
		v1_rms = v1_peak * 0.70710678f;                                      // Vrms = Vpeak / sqrt(2)
		v2_rms = v2_peak * 0.70710678f;                                      // Same for CH2

		// 5) Phase difference using time shift                                // Step explanation
		phase = Measure_Phase(QFP32_MUX_P2_2, QFP32_MUX_P2_3, t_half);         // CH2 relative to CH1

		// 6) Display on LCD                                                   // Step explanation
		sprintf(line1, "F=%4.1fHz P=%+4.0f", freq, phase);                     // Format top line
		LCDprint(line1, 1, 1);                                                // Print top line

		sprintf(line2, "V1=%1.2f V2=%1.2f", v1_rms, v2_rms);                   // Format bottom line
		LCDprint(line2, 2, 1);                                                // Print bottom line

		waitms(200);                                                          // Update rate (~5 Hz)
	}
}
