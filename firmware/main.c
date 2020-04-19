#define F_CPU 20000000
#include <stdbool.h>
#include <util/crc16.h>
#include <avr/io.h>
#include <stdio.h>
#include <string.h>

#define NOTIFY_PORT PORTC
#define NOTIFY_PIN_BM PIN3_bm
#define SDA_PULLUP_SRC_PORT PORTB
#define SDA_PULLUP_SRC_PIN_BM PIN3_bm
#define SCL_PULLUP_SRC_PORT PORTC
#define SCL_PULLUP_SRC_PIN_BM PIN0_bm
#define SENSOR_PULLUP_SRC_PORT PORTB
#define SENSOR_PULLUP_SRC_PIN_BM PIN4_bm
#define SENSOR_ADC_MUXPOS ADC_MUXPOS_AIN8_gc

// delay waits approximately ms milliseconds before returning
void delay(uint16_t ms) {
  uint16_t tms = ms * 10;
  for (; tms > 0; tms--) {
    while (!(TCA0.SINGLE.INTFLAGS & TCA_SINGLE_OVF_bm)) {}
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
  }    
}

// i2c_init initializes the TWI interface for I2C
void i2c_init() {
  TWI0.MBAUD = 255; // Use the slowest baud - we're in no rush
  TWI0.MCTRLA = TWI_ENABLE_bm;
  TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;
}

// i2c_tx returns true of transaction is successful, false otherwise.
// An I2C transaction involves one of the following:
//   a. a write (where w_len > 0 and r_len == 0)
//   b. a read  (where w_len == 0 and r_len > 0)
//   c. a write followed by a read (where w_len > 0 and r_len > 0)
// Data to be written is read from w. Data to be read is stored in r.
bool i2c_tx(uint8_t addr, uint8_t *w, uint8_t w_len, uint8_t *r, uint8_t r_len) {
  if (w_len == 0 && r_len == 0) {
    return true;
  }
  if (w_len > 0) {
    TWI0.MADDR = addr << 1;
    while (!(TWI0.MSTATUS & (TWI_WIF_bm | TWI_RIF_bm)));
    if ((TWI0.MSTATUS & TWI_ARBLOST_bm) || (TWI0.MSTATUS & TWI_BUSERR_bm)) {
      TWI0.MSTATUS |= TWI_ARBLOST_bm;
      return false;
    }
    while (1) {
      while (!(TWI0.MSTATUS & TWI_WIF_bm));
      if (TWI0.MSTATUS & TWI_RXACK_bm) {
        TWI0.MCTRLB = TWI_MCMD_STOP_gc;
        return false;
      }
      if (w_len == 0) {
        break;
      }
      TWI0.MDATA = *w;
      w_len--;
      w++;
    }
  }
  if (r_len > 0) {
    TWI0.MADDR = addr << 1 | 1;
    while (!(TWI0.MSTATUS & TWI_RIF_bm));
    if ((TWI0.MSTATUS & TWI_ARBLOST_bm) || (TWI0.MSTATUS & TWI_BUSERR_bm)) {
      TWI0.MSTATUS |= TWI_ARBLOST_bm;
      return false;
    }
    for (; r_len > 0; r_len--, r++) {
      while (!(TWI0.MSTATUS & TWI_RIF_bm));
      *r = TWI0.MDATA;
      TWI0.MCTRLB = (r_len > 1 ? 0 : TWI_ACKACT_bm) | TWI_MCMD_RECVTRANS_gc;
    }
  }
  TWI0.MCTRLB = TWI_MCMD_STOP_gc;
  return true;
}

// i2c_reg_write performs the following write I2C transaction:
//   register address (reg), register value (d)
// This is a common way of writing into I2C device registers.
// Returns true if successful, false otherwise.
bool i2c_reg_write(uint8_t addr, uint8_t reg, uint8_t d) {
  uint8_t buf[] = {reg, d};
  return i2c_tx(addr, buf, 2, NULL, 0);
}

// crc16 returns the xmodem variant of CRC16 of the msg.
uint16_t crc16(uint8_t *msg, uint8_t msglen)
{
  uint16_t crc = 0;
  for (; msglen > 0; msg++, msglen--) {
    crc = _crc_xmodem_update(crc, *msg);
  }
  return crc;
}

// rev_byte returns b with the bit-order reversed.
uint8_t rev_byte(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

// read_adc returns a value between 0-1023 from a single ADC reading.
uint16_t read_adc() {
  ADC0.COMMAND = ADC_STCONV_bm;
  while ((ADC0.INTFLAGS & ADC_RESRDY_bm) == 0);
  ADC0.INTFLAGS = ADC_RESRDY_bm;
  return ADC0.RES;
}

// wait_on_edge blocks until a change in signal level is detected and
// then returns the time since last signal level change.
uint16_t wait_on_edge() {
  const uint16_t block_size = 1000;

  static uint8_t cur_level  = 0;
  uint8_t        next_level = cur_level;

  static uint16_t cur_block_min  = 65535;
  static uint16_t cur_block_max  = 0;
  static uint16_t cur_block_pos  = 0;
  static uint16_t low_thresh     = 0;
  static uint16_t high_thresh    = 65535;
  static uint16_t peak_to_peak   = 0;
  static uint16_t cur_time       = 0;
  static uint16_t cur_level_time = 0;

  while (next_level == cur_level) {
    // Wait for timer to overflow
    while (!(TCA0.SINGLE.INTFLAGS & TCA_SINGLE_OVF_bm)) {}
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
    cur_time++;

    uint16_t adc_value = read_adc();
    if (adc_value < cur_block_min) {
      cur_block_min = adc_value;
    }
    if (adc_value > cur_block_max) {
      cur_block_max = adc_value;
    }
    cur_block_pos++;

    if (peak_to_peak > 100) {
      if (adc_value >= high_thresh) {
        next_level = 1;
      } else if (adc_value <= low_thresh) {
        next_level = 0;
      }
    }

    if (cur_block_pos == block_size) {
      peak_to_peak = cur_block_max - cur_block_min;
      cur_block_pos = 0;
      high_thresh = cur_block_min + (peak_to_peak * 7) / 10;
      low_thresh = cur_block_min + (peak_to_peak * 3) / 10;
      cur_block_max = 0;
      cur_block_min = 65535;
    }
  }
  cur_level = next_level;

  uint16_t pulse_len;
  if (cur_time < cur_level_time) {
    pulse_len = ((uint16_t)(65535) - cur_level_time) + (1 + cur_time);
  } else {
    pulse_len = cur_time - cur_level_time;
  }    
  cur_level_time = cur_time;
  return pulse_len;
}

// wait_on_peak returns the time between two rising edges or two falling
// edges.
uint16_t wait_on_peak() {
  uint16_t t1 = wait_on_edge();
  uint16_t t2 = wait_on_edge();
  return t1 + t2;
}

// read_packet blocks until a packet of the given length is successfully
// read and the trailing crc is verified.
void read_packet(uint8_t packet[], uint16_t ptp_buffer[], uint8_t *ptp_next_bit_pos, uint8_t packet_len) {
  while (1) {
    ptp_buffer[*ptp_next_bit_pos] = wait_on_peak();
    *ptp_next_bit_pos = (*ptp_next_bit_pos + 1) % (packet_len * 8);
    
    // Determine thresholds based on the last 8 bits
    uint16_t min = 65535, max = 0;
    uint8_t window_start = (*ptp_next_bit_pos - 8) % (packet_len * 8);
    for (uint8_t i = 0; i < 8; i++) {
      uint16_t v = ptp_buffer[(i + window_start) % (packet_len * 8)];
      if (v > max) {
        max = v;
      }
      if (v < min) {
        min = v;
      }
    }

    uint16_t max_min_delta = max - min;
    uint16_t low_thresh = (max_min_delta * 35) / 100 + min;
    uint16_t high_thresh = (max_min_delta * 65) / 100 + min;
    if (high_thresh - low_thresh < 20) {
      continue;
    }

    // Determine data bits from peak-to-peak timings
    for (uint8_t i = 0; i < packet_len; i++) {
      packet[i] = 0;
    }
    uint8_t success = 1;
    for (uint8_t i = 0; i < packet_len * 8; i++) {
      uint16_t ptp = ptp_buffer[(*ptp_next_bit_pos + i) % (packet_len * 8)];
      uint8_t bit_value;
      if (ptp >= high_thresh) {
        bit_value = 1;
      } else if (ptp <= low_thresh) {
        bit_value = 0;
      } else {
        success = 0;
        break;
      }
      packet[i / 8] |= bit_value << (7 - (i % 8));
    }
    
    if (!success) {
      continue;
    }
    uint16_t crc = crc16(packet, packet_len - 2);
    if (!(rev_byte(packet[packet_len - 2]) == (crc >> 8) && rev_byte(packet[packet_len - 1]) == (crc & 0xff))) {
      continue;
    }
    return;
  }
}

typedef struct {
  uint16_t v1; // Voltage for first preference - mapping in read_config()
  uint8_t i1; // Current for first preference - mapping in current lookup table of STUSB4500 datasheet
  uint16_t v2; // Voltage for second preference - mapping in read_config()
  uint8_t i2; // Current for second preference - mapping in current lookup table of STUSB4500 datasheet
  bool req_pd; // If true, requires source to be PD capable before enabling output
} stusb4500_config;

// read_config blocks until a successful configuration is read from the
// light sensor and returns it.
stusb4500_config read_config() {
  uint16_t voltage_map[] = {5 * 20, 9 * 20, 12 * 20, 15 * 20, 20 * 20};
  enum { packet_len = 4 };
  uint8_t packet[packet_len];
  static uint16_t ptp_buffer[packet_len * 8];
  static uint8_t ptp_next_bit_pos;
  while (1) {
    read_packet(packet, ptp_buffer, &ptp_next_bit_pos, packet_len);
    if (packet[0] & 0b10000000) {
      continue;
    }
    stusb4500_config c; 
    c.v1 = (packet[0] >> 4) & 0b111;
    c.v2 = (packet[1] >> 4) & 0b111;
    if (c.v1 > 4 || c.v2 > 4) {
      continue;
    }
    c.v1 = voltage_map[c.v1];
    c.v2 = voltage_map[c.v2];
    c.i1 = packet[0] & 0xf;
    c.i2 = packet[1] & 0xf;
    if (c.i1 == 0 || c.i2 == 0) {
      continue;
    }
    c.req_pd = packet[1] >> 7;
    return c;
  }
}

// stusb4500_flash flashes the given configuration onto STUSB4500 NVRAM
// and returns true if successful, false otherwise.
bool stusb4500_flash(stusb4500_config cfg) {
  uint8_t nvm[] = {
    0x00, 0x00, 0xB0, 0xAA, 0x00, 0x45, 0x00, 0x00,
    0x10, 0x40, 0x9C, 0x1C, 0xFF, 0x01, 0x3C, 0xDF,
    0x02, 0x40, 0x0F, 0x00, 0x32, 0x00, 0xFC, 0xF1,
    // default: 0x00, 0x19, 0x56, 0xAF, 0xF5, 0x35, 0x5F, 0x00
    // changed to 15% lower voltage threshold from 20% for PDO2 & PDO3
    0x00, 0x19, 0x56, 0xAF, 0xA5, 0x35, 0x5A, 0x00,
    0x00, 0x4B, 0x90, 0x21, 0x43, 0x00, 0x40, 0xFB
  };
  nvm[0x1D] = (nvm[0x1D] & 0x0F) | cfg.i1 << 4;
  nvm[0x1C] = (nvm[0x1C] & 0xF0) | cfg.i2;
  nvm[0x22] = cfg.v1 & 0xFF;
  nvm[0x23] = (nvm[0x23] & 0b11111100) | (cfg.v1 >> 8);
  nvm[0x20] = (nvm[0x20] & 0b11000000) | ((cfg.v2 & 0b11) << 6);
  nvm[0x21] = cfg.v2 >> 2;
  if (cfg.req_pd) {
    nvm[0x26] |= 0b1000;
  }
  
  // STUSB4500 seems to pull down the SDA on reset and that messes
  // with internal state of the TWI. Therefore explicitly put the bus
  // into idle state.
  TWI0.MSTATUS = TWI_BUSSTATE_IDLE_gc;

  enum { addr = 0x28 };
  // Flash
  if (!i2c_reg_write(addr, 0x95, 0x47)) return false;
  if (!i2c_reg_write(addr, 0x53, 0x00)) return false;
  if (!i2c_reg_write(addr, 0x96, 0x40)) return false;
  if (!i2c_reg_write(addr, 0x96, 0x00)) return false;
  delay(4);
  if (!i2c_reg_write(addr, 0x96, 0x40)) return false;
  if (!i2c_reg_write(addr, 0x97, 0xFA)) return false;
  if (!i2c_reg_write(addr, 0x96, 0x50)) return false;
  delay(4);
  if (!i2c_reg_write(addr, 0x97, 0x07)) return false;
  if (!i2c_reg_write(addr, 0x96, 0x50)) return false;
  delay(20);
  if (!i2c_reg_write(addr, 0x97, 0x05)) return false;
  if (!i2c_reg_write(addr, 0x96, 0x50)) return false;
  delay(20);
  for (uint8_t i = 0; i < 5; i++) {
    uint8_t buf[9] = {0x53};
    memcpy(buf + 1, nvm + (i * 8), 8);
    if (!i2c_tx(addr, buf, 9, NULL, 0)) return false;
    delay(4);
    if (!i2c_reg_write(addr, 0x97, 0x01)) return false;
    if (!i2c_reg_write(addr, 0x96, 0x50)) return false;
    delay(4);
    if (!i2c_reg_write(addr, 0x97, 0x06)) return false;
    if (!i2c_reg_write(addr, 0x96, 0x50 + i)) return false;
    delay(8);
  }
  uint8_t buf[3] = {0x96, 0x40, 0x00};
  if (!i2c_tx(addr, buf, 3, NULL, 0)) return false;
  if (!i2c_reg_write(addr, 0x95, 0x00)) return false;

  return true;
}

// stusb4500_reset resets the STUSB4500. This also results in loss of
// power to the entire board while STUSB4500 boots up again, effectively
// resetting the uC as well.
bool stusb4500_reset() {
  enum { addr = 0x28 };
  return i2c_reg_write(addr, 0x23, 0x01);
}

void setup() {
  // Disable clock pre-scaler
  CPU_CCP = CCP_IOREG_gc;
  CLKCTRL.MCLKCTRLB = 0;

  // Configure a timer to overflow 10K times per second
  TCA0.SINGLE.PER = 2000;
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc | TCA_SINGLE_ENABLE_bm;

  // Configure ADC
  ADC0.CTRLC = ADC_REFSEL_VDDREF_gc | ADC_PRESC_DIV32_gc;
  ADC0.MUXPOS = SENSOR_ADC_MUXPOS;
  ADC0.CTRLA = ADC_ENABLE_bm;
  
  // Configure notification pin
  NOTIFY_PORT.DIRSET = NOTIFY_PIN_BM;
  NOTIFY_PORT.OUTCLR = NOTIFY_PIN_BM;

  // Configure I2C pull-up sources
  SCL_PULLUP_SRC_PORT.DIRSET = SCL_PULLUP_SRC_PIN_BM;
  SCL_PULLUP_SRC_PORT.OUTSET = SCL_PULLUP_SRC_PIN_BM;
  SDA_PULLUP_SRC_PORT.DIRSET = SDA_PULLUP_SRC_PIN_BM;
  SDA_PULLUP_SRC_PORT.OUTSET = SDA_PULLUP_SRC_PIN_BM;

  // Configure light sensor pull-up source
  SENSOR_PULLUP_SRC_PORT.DIRSET = SENSOR_PULLUP_SRC_PIN_BM;
  SENSOR_PULLUP_SRC_PORT.OUTSET = SENSOR_PULLUP_SRC_PIN_BM;
  
  i2c_init();
}

// notify notifies the user that programming is successfully completed
// by flashing the green light on the top side of the board.
void notify() {
  for (uint8_t i = 0; i < 10; i++) {
    NOTIFY_PORT.OUTSET = NOTIFY_PIN_BM;
    delay(100);
    NOTIFY_PORT.OUTCLR = NOTIFY_PIN_BM;
    delay(100);
  }
}

int main(void) {
  setup();
  // Sync to peak
  wait_on_edge();
  while (1) {
    stusb4500_config cfg = read_config();
    bool flash_success = stusb4500_flash(cfg);
    if (flash_success) {
      notify();
      stusb4500_reset();
    }
  }
}
