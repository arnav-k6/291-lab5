#include <stdio.h>                                                        // Needed for sprintf
#include <EFM8LB1.h>                                                      // EFM8LB1 register definitions

#define SYSCLK 72000000L                                                  // System clock frequency

#define SERVO_PIN P2_5                                                    // Servo signal pin P2.5

char _c51_external_startup (void)                                         // Startup function
{
SFRPAGE = 0x00;                                                        // Use default SFR page
WDTCN = 0xDE;                                                          // Disable watchdog key 1
WDTCN = 0xAD;                                                          // Disable watchdog key 2

```
VDM0CN  = 0x80;                                                        // Enable VDD monitor
RSTSRC  = 0x02 | 0x04;                                                 // Reset on missing clock + low VDD

SFRPAGE = 0x10;                                                        // Flash timing page
PFE0CN  = 0x20;                                                        // Flash timing for 72 MHz (<75 MHz)
SFRPAGE = 0x00;                                                        // Back to default

CLKSEL = 0x00;                                                         // Transition via 24.5 MHz first
CLKSEL = 0x00;                                                         // Write twice
while ((CLKSEL & 0x80) == 0);                                          // Wait stable
CLKSEL = 0x03;                                                         // Select 72 MHz
CLKSEL = 0x03;                                                         // Write twice
while ((CLKSEL & 0x80) == 0);                                          // Wait stable

P2MDOUT |= 0x20;                                                       // P2.5 push-pull output for servo
XBR1     = 0x00;                                                       // No extra crossbar peripherals
XBR2     = 0x40;                                                       // Enable crossbar + weak pull-ups

return 0;                                                              // Done
```

}

void Timer3us(unsigned char us)                                            // Delay in microseconds
{
unsigned char i;                                                       // Loop counter

```
CKCON0 |= 0b_0100_0000;                                                // Timer3 uses SYSCLK
TMR3RL   = (-(SYSCLK) / 1000000L);                                     // Reload for 1us overflow
TMR3     = TMR3RL;                                                     // Initialize Timer3
TMR3CN0  = 0x04;                                                       // Start Timer3

for (i = 0; i < us; i++)                                               // Repeat us times
{
	while (!(TMR3CN0 & 0x80));                                         // Wait overflow
	TMR3CN0 &= ~(0x80);                                                // Clear overflow flag
}

TMR3CN0 = 0;                                                           // Stop Timer3
```

}

void waitms(unsigned int ms)                                               // Delay in milliseconds
{
unsigned int j;                                                        // Outer loop
unsigned char k;                                                       // Inner loop
for (j = 0; j < ms; j++)                                               // Loop ms times
for (k = 0; k < 4; k++) Timer3us(250);                             // 4*250us = 1ms
}

// Delay any number of microseconds using 250us chunks
// (Timer3us is capped at 255 us max)
void delay_us_long(unsigned int us)                                        // Delay arbitrary microseconds
{
while (us >= 250)                                                      // Consume in 250us chunks
{
Timer3us(250);                                                     // 250us chunk
us -= 250;                                                         // Subtract chunk
}
if (us > 0) Timer3us((unsigned char)us);                               // Delay remainder
}

void servo_pulse(int angle)                                                // Send ONE 20ms servo frame
{
unsigned int pulse_us;                                                 // HIGH pulse width in microseconds
unsigned int low_us;                                                   // LOW time to complete 20ms frame

```
// Clamp angle to valid range
if (angle < -180) angle = -180;                                        // Floor at -180
if (angle >  180) angle =  180;                                        // Ceiling at +180

// Map signed phase -180..+180 to pulse 1000..2000 us:
//   phase -180 -> 1000 us  (servo 0 deg,   needle far left)
//   phase    0 -> 1500 us  (servo 90 deg,  needle centre)
//   phase +180 -> 2000 us  (servo 180 deg, needle far right)
pulse_us = (unsigned int)(1500L + ((long)angle * 500L) / 180L);

low_us = 20000 - pulse_us;                                             // LOW = 20ms - pulse (always positive)

SERVO_PIN = 1;                                                         // Start HIGH pulse
delay_us_long(pulse_us);                                               // Hold HIGH for pulse_us
SERVO_PIN = 0;                                                         // End pulse, go LOW

delay_us_long(low_us);                                                 // Hold LOW for remainder of 20ms frame
```

}

void servo_hold(int angle, unsigned int hold_ms)                           // Hold servo at angle for hold_ms milliseconds
{
// Send repeated 20ms pulses for the full hold duration.
// Each servo_pulse() takes exactly 20ms, so hold_ms/20 pulses = correct hold time.
// Minimum hold_ms should be >= 20 (one frame).
unsigned int frames;                                                   // Number of 20ms frames to send
unsigned int i;                                                        // Frame counter

```
frames = hold_ms / 20;                                                 // Number of frames in hold period
if (frames == 0) frames = 1;                                           // Always send at least one pulse

for (i = 0; i < frames; i++)                                           // Repeat for hold duration
	servo_pulse(angle);                                                // Send one 20ms frame
```

}

void main(void)                                                            // Main program
{
int phase;                                                             // Signed phase: -180 to +180 degrees

```
SERVO_PIN = 0;                                                         // Start pin LOW
waitms(500);                                                           // Servo power-up settle

while (1)                                                              // Forever loop
{
	// Clockwise sweep: phase 0 -> +45 -> +90 -> +135 -> +180
	for (phase = 0; phase <= 180; phase += 45)                         // Step positive phase
	{
		servo_hold(phase, 1000);                                       // Hold position for 1 second (50 pulses)
	}

	// Counter-clockwise sweep: phase 0 -> -45 -> -90 -> -135 -> -180
	for (phase = 0; phase >= -180; phase -= 45)                        // Step negative phase
	{
		servo_hold(phase, 1000);                                       // Hold position for 1 second (50 pulses)
	}
}
```

}