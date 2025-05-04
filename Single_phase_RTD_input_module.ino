/**
 * NEPHAT GAKUYA GITHUA 
 *RTD Input Module Firmware – ESP-WROOM-32 + ADS124S06
 * 
 * This firmware interfaces an ESP32 (ESP-WROOM-32 module) with the ADS124S06 24-bit ADC to measure
 * resistance from either a Pt100 or Pt1000 RTD sensor, selected via GPIO-based switches. It configures
 * SPI communication, reads raw ADC values, converts these to resistance, and then calculates
 * temperature using the inverse Callendar–Van Dusen equation. Scaling is automatically adjusted
 * based on the selected RTD type (Pt100 or Pt1000).
 */

#include <SPI.h>

// === SPI Pin Definitions ===
#define PIN_CS     5    // ADS124S06 Chip Select
#define PIN_MOSI   23   // Master Out Slave In
#define PIN_MISO   19   // Master In Slave Out (also DRDY)
#define PIN_SCLK   18   // SPI Clock
#define PIN_START  21   // START/SYNC pin

// === RTD Type Selection GPIOs ===
#define GPIO_RTD_PT100   32  // GPIO input for Pt100 select
#define GPIO_RTD_PT1000  33  // GPIO input for Pt1000 select

// === Callendar–Van Dusen Coefficients for Platinum RTDs ===
const float A = 3.9083e-3;
const float B = -5.775e-7;

void setup() {
  Serial.begin(115200);

  // === Configure RTD Selection GPIOs ===
  pinMode(GPIO_RTD_PT100, INPUT);
  pinMode(GPIO_RTD_PT1000, INPUT);

  // === Configure SPI Pins ===
  pinMode(PIN_CS, OUTPUT);
  pinMode(PIN_START, OUTPUT);
  digitalWrite(PIN_CS, HIGH);      // Deselect ADC
  digitalWrite(PIN_START, HIGH);   // Keep START/SYNC high

  // === Initialize SPI Interface ===
  SPI.begin(PIN_SCLK, PIN_MISO, PIN_MOSI, PIN_CS);
  delay(100);

  // === Wake ADC ===
  digitalWrite(PIN_CS, LOW);
  delayMicroseconds(10);
  SPI.transfer(0x06);  // Send WAKE command
  delayMicroseconds(10);
  digitalWrite(PIN_CS, HIGH);

  Serial.println("RTD Module Initialized.");
}

void loop() {
  float R0 = 0.0;

  // === Read RTD Type Selection from GPIOs ===
  bool pt100_selected = digitalRead(GPIO_RTD_PT100);
  bool pt1000_selected = digitalRead(GPIO_RTD_PT1000);

  if (pt100_selected && !pt1000_selected) {
    R0 = 100.0;
    Serial.println("RTD Type: Pt100");
  } else if (!pt100_selected && pt1000_selected) {
    R0 = 1000.0;
    Serial.println("RTD Type: Pt1000");
  } else {
    Serial.println("Invalid RTD selection. Please press only one RTD select button.");
    delay(1000);
    return;
  }

  // === Begin SPI ADC Conversion ===
  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0x12);  // RDATA command (Read Data)
  delayMicroseconds(5);

  // === Read 3 bytes of ADC data (24-bit) ===
  uint32_t adc_raw = 0;
  for (int i = 0; i < 3; i++) {
    adc_raw = (adc_raw << 8) | SPI.transfer(0x00);
  }
  digitalWrite(PIN_CS, HIGH);

  // === Convert Raw Data to Signed Integer ===
  if (adc_raw & 0x800000) { // Check sign bit for 24-bit data
    adc_raw |= 0xFF000000;  // Sign-extend if negative
  }
  int32_t adc_signed = (int32_t)adc_raw;

  // === Convert ADC Code to Voltage ===
  float Vref = 5.0;              // ADR4550 precision reference
  float gain = 16.0;             // Assumed gain set by RG on INA333
  float adc_voltage = ((float)adc_signed / 8388607.0) * Vref;

  // === Calculate RTD Resistance ===
  float excitation_current = 0.0005;  // 500 µA from REF200
  float rtd_resistance = adc_voltage / (gain * excitation_current);

  // === Inverse Callendar–Van Dusen Equation to Compute Temperature ===
  float temp_c = (-A + sqrt(A * A - 4 * B * (1 - (rtd_resistance / R0)))) / (2 * B);

  // === Output Result ===
  Serial.print("Raw ADC: "); Serial.println(adc_signed);
  Serial.print("RTD Resistance: "); Serial.print(rtd_resistance); Serial.println(" ohms");
  Serial.print("Temperature: "); Serial.print(temp_c); Serial.println(" °C");
  Serial.println("--------------------------------------------------");

  delay(1000); // Wait 1 second before next reading
}
