#include <stdio.h>                                                        // Needed for sprintf
#include <EFM8LB1.h>                                                      // EFM8LB1 register definitions

#define SYSCLK  72000000L                                                 // System clock frequency
#define SARCLK  18000000L                                                 // ADC SAR clock (max 18 MHz)
#define VDD     3.3035                                                    // Measured VDD in volts
#define VMID   (VDD / 2.0)                                                // Midpoint ~1.65V = “zero”

// 74HC595 pin assignments
#define SR_DATA  P0_5                                                     // DS:   serial data in   (P0.5)
#define SR_SCLK  P0_7                                                     // SHCP: shift clock      (P0.7)
#define SR_RCLK  P0_6                                                     // STCP: latch / rclk     (P0.6)
// NOTE: P0.8 does not exist on EFM8LB1 (port 0 is 8-bit, P0.0-P0.7).
// If your RCLK wire is physically on the pin labelled “P0.8” on your board
// header, it maps to P1.0. Change SR_RCLK to P1_0 in that case.

// ADC signal pin
#define ADC_PIN  QFP32_MUX_P2_3                                          // P2.3 = ADC input channel

// LED bar pattern bytes for 74HC595
// Q0 = rightmost LED (LED1, small positive)
// Q7 = leftmost  LED (LED8, most negative)
//
// Positive side: Q0..Q3  (right 4, LEDs 1-4)
// Negative side: Q4..Q7  (left  4, LEDs 5-8)
//
// Positive levels (right LEDs, Q0=LSB side):
//   +1 level  -> Q0        = 0b_0000_0001
//   +2 levels -> Q0|Q1     = 0b_0000_0011
//   +3 levels -> Q0|Q1|Q2  = 0b_0000_0111
//   +4 levels -> Q0..Q3    = 0b_0000_1111
//
// Negative levels (left LEDs, Q4=first negative):
//   -1 level  -> Q4        = 0b_0001_0000
//   -2 levels -> Q4|Q5     = 0b_0011_0000
//   -3 levels -> Q4|Q5|Q6  = 0b_0111_0000
//   -4 levels -> Q4..Q7    = 0b_1111_0000
//
// 0V -> all off = 0b_0000_0000

// Lookup table indexed by bar level 0-4
// pos_bar[n] = pattern for n positive LEDs lit
code unsigned char pos_bar[5] =
{
0x00,                                                                 // 0 LEDs  (0V)
0x01,                                                                 // 1 LED   (Q0)
0x03,                                                                 // 2 LEDs  (Q0-Q1)
0x07,                                                                 // 3 LEDs  (Q0-Q2)
0x0F                                                                  // 4 LEDs  (Q0-Q3)
};

// neg_bar[n] = pattern for n negative LEDs lit
code unsigned char neg_bar[5] =
{
0x00,                                                                 // 0 LEDs  (0V)
0x10,                                                                 // 1 LED   (Q4)
0x30,                                                                 // 2 LEDs  (Q4-Q5)
0x70,                                                                 // 3 LEDs  (Q4-Q6)
0xF0                                                                  // 4 LEDs  (Q4-Q7)
};

char _c51_external_startup (void)                                         // Startup function
{
SFRPAGE = 0x00;                                                       // Default SFR page
WDTCN = 0xDE;                                                         // Disable watchdog key 1
WDTCN = 0xAD;                                                         // Disable watchdog key 2

```
VDM0CN  = 0x80;                                                       // Enable VDD monitor
RSTSRC  = 0x02 | 0x04;                                                // Reset: missing clock + low VDD

SFRPAGE = 0x10;                                                       // Flash timing page
PFE0CN  = 0x20;                                                       // Flash timing for 72 MHz
SFRPAGE = 0x00;                                                       // Back to default

CLKSEL = 0x00;                                                        // Transition via 24.5 MHz
CLKSEL = 0x00;                                                        // Write twice
while ((CLKSEL & 0x80) == 0);                                         // Wait stable
CLKSEL = 0x03;                                                        // Select 72 MHz
CLKSEL = 0x03;                                                        // Write twice
while ((CLKSEL & 0x80) == 0);                                         // Wait stable

// 74HC595 pins as push-pull outputs
P0MDOUT |= 0b_1110_0000;                                              // P0.5, P0.6, P0.7 push-pull
SR_DATA  = 0;                                                         // Data  idle low
SR_SCLK  = 0;                                                         // Clock idle low
SR_RCLK  = 0;                                                         // Latch idle low

XBR1     = 0x00;                                                      // No extra crossbar peripherals
XBR2     = 0x40;                                                      // Enable crossbar + weak pull-ups

return 0;                                                             // Done
```

}

void Timer3us(unsigned char us)                                           // Delay in microseconds
{
unsigned char i;                                                      // Loop counter

```
CKCON0 |= 0b_0100_0000;                                               // Timer3 uses SYSCLK
TMR3RL   = (-(SYSCLK) / 1000000L);                                    // Reload for 1us overflow
TMR3     = TMR3RL;                                                    // Initialize Timer3
TMR3CN0  = 0x04;                                                      // Start Timer3

for (i = 0; i < us; i++)                                              // Repeat us times
{
    while (!(TMR3CN0 & 0x80));                                        // Wait overflow
    TMR3CN0 &= ~(0x80);                                               // Clear overflow flag
}

TMR3CN0 = 0;                                                          // Stop Timer3
```

}

void waitms(unsigned int ms)                                              // Delay in milliseconds
{
unsigned int j;                                                       // Outer loop
unsigned char k;                                                      // Inner loop
for (j = 0; j < ms; j++)                                              // Loop ms times
for (k = 0; k < 4; k++) Timer3us(250);                           // 4*250us = 1ms
}

void InitADC(void)                                                        // Initialize ADC
{
SFRPAGE = 0x00;                                                       // Default page
ADEN = 0;                                                             // Disable ADC while configuring

```
ADC0CN1 =
    (0x2 << 6) |                                                      // 14-bit mode
    (0x0 << 3) |                                                      // No shift
    (0x0 << 0) ;                                                      // No accumulation

ADC0CF0 =
    ((SYSCLK / SARCLK) << 3) |                                        // SAR clock divider
    (0x0 << 2);                                                       // ADCCLK = SYSCLK

ADC0CF1 =
    (0   << 7) |                                                      // Low power off
    (0x1E << 0);                                                      // Tracking time

ADC0CN0 =
    (0x0 << 7) |                                                      // ADEN = 0 here
    (0x0 << 6) |                                                      // No idle powerdown
    (0x0 << 5) |                                                      // ADINT cleared
    (0x0 << 4) |                                                      // ADBUSY cleared
    (0x0 << 3) |                                                      // Window compare off
    (0x0 << 2) |                                                      // Gain = 1
    (0x0 << 0) ;                                                      // Temp sensor off

ADC0CF2 =
    (0x0 << 7) |                                                      // Ref GND pin
    (0x1 << 5) |                                                      // Ref = VDD
    (0x1F << 0);                                                      // Power-up delay

ADC0CN2 =
    (0x0 << 7) |                                                      // Accumulator overwrite
    (0x0 << 0) ;                                                      // Software trigger

ADEN = 1;                                                             // Enable ADC
```

}

void InitPinADC(unsigned char portno, unsigned char pinno)                // Configure pin as analog input
{
unsigned char mask;                                                   // Bit mask
mask = 1 << pinno;                                                    // Compute mask

```
SFRPAGE = 0x20;                                                       // Port config page
switch (portno)
{
    case 0: P0MDIN &= (~mask); P0SKIP |= mask; break;                // Port 0 analog
    case 1: P1MDIN &= (~mask); P1SKIP |= mask; break;                // Port 1 analog
    case 2: P2MDIN &= (~mask); P2SKIP |= mask; break;                // Port 2 analog
    default: break;
}
SFRPAGE = 0x00;                                                       // Back to default
```

}

unsigned int ADC_at_Pin(unsigned char pin)                                // Return raw 14-bit ADC counts
{
ADC0MX  = pin;                                                        // Select mux pin
ADINT   = 0;                                                          // Clear flag
ADBUSY  = 1;                                                          // Start conversion
while (!ADINT);                                                       // Wait complete
return (ADC0);                                                        // Return result
}

float Volts_at_Pin(unsigned char pin)                                     // Return voltage at pin
{
return ((ADC_at_Pin(pin) * VDD) / 0x3FFF);                           // 14-bit: max = 16383 = 0x3FFF
}

//—————————————————–
// Sample pin 3 times and return max voltage seen
// Used separately for positive peak and negative peak
//—————————————————–
float Sample_Max(unsigned char pin)                                       // Return max of 3 samples
{
float a, b, c;                                                        // Three samples

```
a = Volts_at_Pin(pin);                                                // Sample 1
waitms(1);                                                            // Small gap between samples
b = Volts_at_Pin(pin);                                                // Sample 2
waitms(1);                                                            // Small gap
c = Volts_at_Pin(pin);                                                // Sample 3

if (b > a) a = b;                                                     // Keep larger of a, b
if (c > a) a = c;                                                     // Keep larger of that, c
return a;                                                             // Return maximum
```

}

//—————————————————–
// Shift one byte out to 74HC595 MSB first
// Q7 = first bit shifted = MSB = leftmost LED (LED8)
// Q0 = last bit shifted  = LSB = rightmost LED (LED1)
//—————————————————–
void SR_ShiftByte(unsigned char data)                                     // Shift byte into 74HC595
{
unsigned char i;                                                      // Bit counter

```
for (i = 0; i < 8; i++)                                              // 8 bits MSB first
{
    SR_DATA = (data & 0x80) ? 1 : 0;                                 // Put MSB on data line
    SR_SCLK = 1;                                                      // Rising edge clocks bit in
    Timer3us(1);                                                      // Setup time
    SR_SCLK = 0;                                                      // Clock low
    Timer3us(1);                                                      // Hold time
    data <<= 1;                                                       // Next bit
}
```

}

void SR_Latch(void)                                                       // Latch shift register to outputs
{
SR_RCLK = 1;                                                          // Rising edge latches to output
Timer3us(1);                                                          // Hold time
SR_RCLK = 0;                                                          // Latch low
}

//—————————————————–
// Display voltage on 8-LED bar graph
//
// Input:  v_pos = positive peak above VMID (volts, >= 0)
//         v_neg = negative peak below VMID (volts, >= 0, magnitude)
//         amplitude = full-scale reference (max of v_pos, v_neg)
//
// Right 4 LEDs (Q0-Q3) = positive bar, fill right to left
// Left  4 LEDs (Q4-Q7) = negative bar, fill left to right
//
// Level mapping (each level = amplitude/4):
//   ratio 0.00-0.25 -> 1 LED
//   ratio 0.25-0.50 -> 2 LEDs
//   ratio 0.50-0.75 -> 3 LEDs
//   ratio 0.75-1.00 -> 4 LEDs
//   ratio 0.00      -> 0 LEDs (0V, all off)
//—————————————————–
void LED_Bar_Update(float v_pos, float v_neg, float amplitude)            // Update LED bar graph
{
unsigned char pos_level;                                              // Number of positive LEDs to light
unsigned char neg_level;                                              // Number of negative LEDs to light
unsigned char pattern;                                                // Final byte for 74HC595
float step;                                                           // Voltage per LED step

```
if (amplitude < 0.001f)                                               // Guard: near-zero signal
{
    SR_ShiftByte(0x00);                                               // All LEDs off
    SR_Latch();                                                        // Latch output
    return;                                                            // Done
}

step = amplitude / 4.0f;                                              // Each LED represents amplitude/4

// Positive bar: how many right-side LEDs to light
if      (v_pos < 0.001f)       pos_level = 0;                        // No signal -> none
else if (v_pos <= step)        pos_level = 1;                        // 0-25%  -> 1 LED
else if (v_pos <= 2.0f * step) pos_level = 2;                        // 25-50% -> 2 LEDs
else if (v_pos <= 3.0f * step) pos_level = 3;                        // 50-75% -> 3 LEDs
else                           pos_level = 4;                        // 75-100%-> 4 LEDs

// Negative bar: how many left-side LEDs to light
if      (v_neg < 0.001f)       neg_level = 0;                        // No signal -> none
else if (v_neg <= step)        neg_level = 1;                        // 0-25%  -> 1 LED
else if (v_neg <= 2.0f * step) neg_level = 2;                        // 25-50% -> 2 LEDs
else if (v_neg <= 3.0f * step) neg_level = 3;                        // 50-75% -> 3 LEDs
else                           neg_level = 4;                        // 75-100%-> 4 LEDs

pattern = pos_bar[pos_level] | neg_bar[neg_level];                   // Combine both halves

SR_ShiftByte(pattern);                                                // Shift pattern out
SR_Latch();                                                           // Latch to LED outputs
```

}

void main(void)                                                           // Main program
{
float v_raw;                                                          // Raw sampled voltage
float v_pos_peak;                                                     // Positive peak above VMID
float v_neg_peak;                                                     // Negative peak below VMID
float v_max_pos;                                                      // Running max positive
float v_max_neg;                                                      // Running max negative
float amplitude;                                                      // Full-scale = max(pos, neg)
unsigned char sample;                                                 // Sample loop counter

```
InitPinADC(2, 3);                                                     // Configure P2.3 as analog input
InitADC();                                                            // Initialize ADC

SR_ShiftByte(0x00);                                                   // Clear shift register
SR_Latch();                                                           // All LEDs off on startup
waitms(200);                                                          // Startup settle

while (1)                                                             // Forever loop
{
    v_max_pos = 0.0f;                                                 // Reset max positive
    v_max_neg = 0.0f;                                                 // Reset max negative

    // Take 3 samples, track max deviation above and below VMID
    for (sample = 0; sample < 3; sample++)                           // 3 sample passes
    {
        v_raw = Volts_at_Pin(ADC_PIN);                               // Read voltage

        v_pos_peak = v_raw - VMID;                                   // Above midpoint = positive
        if (v_pos_peak < 0.0f) v_pos_peak = 0.0f;                   // Clamp: ignore negative half
        if (v_pos_peak > v_max_pos) v_max_pos = v_pos_peak;         // Track positive peak

        v_neg_peak = VMID - v_raw;                                   // Below midpoint = negative
        if (v_neg_peak < 0.0f) v_neg_peak = 0.0f;                   // Clamp: ignore positive half
        if (v_neg_peak > v_max_neg) v_max_neg = v_neg_peak;         // Track negative peak

        waitms(1);                                                    // 1ms between samples
    }

    // Amplitude = larger of the two peaks (full-scale reference)
    amplitude = v_max_pos;                                            // Start with positive peak
    if (v_max_neg > amplitude) amplitude = v_max_neg;                // Use negative if larger

    // Update LED bar relative to amplitude
    LED_Bar_Update(v_max_pos, v_max_neg, amplitude);                 // Drive 74HC595

    waitms(50);                                                       // ~20 Hz display refresh
}
```

}