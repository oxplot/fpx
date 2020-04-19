package main

import (
	"log"
	"math"
	"time"

	"periph.io/x/periph/conn/i2c"
	"periph.io/x/periph/conn/i2c/i2creg"
	"periph.io/x/periph/host"
)

type GPIOFunction uint8

func (g GPIOFunction) String() string {
	switch g {
	case GPIOSoftwareControl:
		return "GPIOSoftwareControl"
	case GPIOErrorRecovery:
		return "GPIOErrorRecovery"
	case GPIODebug:
		return "GPIODebug"
	case GPIOSinkPower:
		return "GPIOSinkPower"
	default:
		panic("invalid value")
	}
}

type VBUSDischargeControl uint8

func (v VBUSDischargeControl) String() string {
	switch v {
	case VBUSDischargeEnabled:
		return "VBUSDischargeEnabled"
	case VBUSDischargeDisabled:
		return "VBUSDischargeDisabled"
	default:
		panic("invalid value")
	}
}

type PDOCurrent uint8

func (c PDOCurrent) String() string {
	switch c {
	case CurrentFlex:
		return "CurrentFlex"
	case Current500mA:
		return "Current500mA"
	case Current750mA:
		return "Current750mA"
	case Current1000mA:
		return "Current1000mA"
	case Current1250mA:
		return "Current1250mA"
	case Current1500mA:
		return "Current1500mA"
	case Current1750mA:
		return "Current1750mA"
	case Current2000mA:
		return "Current2000mA"
	case Current2250mA:
		return "Current2250mA"
	case Current2500mA:
		return "Current2500mA"
	case Current2750mA:
		return "Current2750mA"
	case Current3000mA:
		return "Current3000mA"
	case Current3500mA:
		return "Current3500mA"
	case Current4000mA:
		return "Current4000mA"
	case Current4500mA:
		return "Current4500mA"
	case Current5000mA:
		return "Current5000mA"
	default:
		panic("invalid value")
	}
}

const (
	GPIOSoftwareControl GPIOFunction = 0x00
	GPIOErrorRecovery   GPIOFunction = 0x01
	GPIODebug           GPIOFunction = 0x02
	GPIOSinkPower       GPIOFunction = 0x03

	VBUSDischargeEnabled  VBUSDischargeControl = 0x00
	VBUSDischargeDisabled VBUSDischargeControl = 0x01
)

const (
	CurrentFlex PDOCurrent = iota
	Current500mA
	Current750mA
	Current1000mA
	Current1250mA
	Current1500mA
	Current1750mA
	Current2000mA
	Current2250mA
	Current2500mA
	Current2750mA
	Current3000mA
	Current3500mA
	Current4000mA
	Current4500mA
	Current5000mA
)

type Config struct {
	GPIOFunction  GPIOFunction
	VBUSDischarge VBUSDischargeControl
	// Between 84ms and 1260ms rounded up to the nearest 84ms
	VBUSDischargeTimeTo0V time.Duration
	// Between 24ms and 288ms rounded up to the nearest 24ms
	VBUSDischargeTimeToPDO time.Duration
	// Between 5V and 20V rounded to the nearest 50mV
	PDO2Voltage float64
	// Between 5V and 20V rounded to the nearest 50mV
	PDO3Voltage float64
	PDO1Current PDOCurrent
	PDO2Current PDOCurrent
	PDO3Current PDOCurrent
	// Whether to enable the power if PD profiles are not negotiated
	EnableWithoutPD bool
}

type NVM [40]byte

var (
	DefaultNVM = NVM([40]byte{
		0x00, 0x00, 0xB0, 0xAA, 0x00, 0x45, 0x00, 0x00,
		0x10, 0x40, 0x9C, 0x1C, 0xFF, 0x01, 0x3C, 0xDF,
		0x02, 0x40, 0x0F, 0x00, 0x32, 0x00, 0xFC, 0xF1,
		0x00, 0x19, 0x56, 0xAF, 0xF5, 0x35, 0x5F, 0x00,
		0x00, 0x4B, 0x90, 0x21, 0x43, 0x00, 0x40, 0xFB,
	})
)

func (c *Config) FromNVM(nvm *NVM) {
	c.GPIOFunction = GPIOFunction((nvm[0x08] >> 4) & 0b11)
	c.VBUSDischarge = VBUSDischargeControl((nvm[0x09] >> 5) & 1)
	c.VBUSDischargeTimeTo0V = time.Duration((nvm[0x0A] >> 4)) * time.Millisecond * 84
	c.VBUSDischargeTimeToPDO = time.Duration((nvm[0x0A] & 0x0F)) * time.Millisecond * 24
	c.PDO1Current = PDOCurrent(nvm[0x1A] >> 4)
	c.PDO2Current = PDOCurrent(nvm[0x1C] & 0x0F)
	c.PDO3Current = PDOCurrent(nvm[0x1D] >> 4)
	c.PDO2Voltage = float64((uint16(nvm[0x20])>>6)|(uint16(nvm[0x21])<<2)) * 0.05
	c.PDO3Voltage = float64(uint16(nvm[0x22])|(uint16(nvm[0x23]&0b11)<<8)) * 0.05
	c.EnableWithoutPD = nvm[0x26]&0b1000 == 0
}

func (c *Config) ToNVM(nvm *NVM) {
	nvm[0x08] = (nvm[0x08] & 0b11001111) | uint8(c.GPIOFunction)<<4
	nvm[0x09] = (nvm[0x09] & 0b11011111) | uint8(c.VBUSDischarge)<<5
	nvm[0x0A] = (nvm[0x0A] & 0x0F) | uint8(math.Ceil(float64(c.VBUSDischargeTimeTo0V)/float64(84*time.Millisecond)))<<4
	nvm[0x0A] = (nvm[0x0A] & 0xF0) | uint8(math.Ceil(float64(c.VBUSDischargeTimeToPDO)/float64(24*time.Millisecond)))
	nvm[0x1A] = (nvm[0x1A] & 0x0F) | uint8(c.PDO1Current<<4)
	nvm[0x1C] = (nvm[0x1C] & 0xF0) | uint8(c.PDO2Current)
	nvm[0x1D] = (nvm[0x1D] & 0x0F) | uint8(c.PDO3Current<<4)
	nvm[0x20] = (nvm[0x20] & 0b11000000) | uint8((uint16(math.Round(c.PDO2Voltage/0.05))&0b11)<<6)
	nvm[0x21] = uint8(uint16(math.Round(c.PDO2Voltage/0.05)) >> 2)
	nvm[0x22] = uint8(uint16(math.Round(c.PDO3Voltage/0.05)) & 0xFF)
	nvm[0x23] = (nvm[0x23] & 0b11111100) | uint8(uint16(math.Round(c.PDO3Voltage/0.05))>>8)
	if c.EnableWithoutPD {
		nvm[0x26] = nvm[0x26] & 0b11110111
	} else {
		nvm[0x26] = nvm[0x26] | 0b1000
	}
}

func batchWrite(d *i2c.Dev, ps [][]byte) error {
	for _, p := range ps {
		if p == nil {
			time.Sleep(time.Millisecond * 2)
		} else {
			if err := d.Tx(p, nil); err != nil {
				return err
			}
		}
	}
	return nil
}

func (nvm *NVM) Read(d *i2c.Dev) error {
	if err := batchWrite(d, [][]byte{
		[]byte{0x95, 0x47}, []byte{0x96, 0x40}, []byte{0x96, 0x00},
		nil,
		[]byte{0x96, 0x40}, []byte{0x97, 0x00},
	}); err != nil {
		return err
	}
	for i := byte(0); i < 5; i++ {
		if err := d.Tx([]byte{0x96, 0x50 + i}, nil); err != nil {
			return err
		}
		time.Sleep(time.Millisecond)
		if err := d.Tx([]byte{0x53}, nvm[i*8:i*8+8]); err != nil {
			return err
		}
	}
	if err := batchWrite(d, [][]byte{
		[]byte{0x96, 0x40, 0x00}, []byte{0x95, 0x00},
	}); err != nil {
		return err
	}
	return nil
}

func (nvm *NVM) Write(d *i2c.Dev) error {
	if err := batchWrite(d, [][]byte{
		[]byte{0x95, 0x47}, []byte{0x53, 0x00}, []byte{0x96, 0x40}, []byte{0x96, 0x00},
		nil,
		[]byte{0x96, 0x40}, []byte{0x97, 0xFA}, []byte{0x96, 0x50},
		nil,
		[]byte{0x97, 0x07}, []byte{0x96, 0x50},
		nil, nil, nil, nil, nil,
		[]byte{0x97, 0x05}, []byte{0x96, 0x50},
		nil, nil, nil, nil, nil,
	}); err != nil {
		return err
	}
	for i := byte(0); i < 5; i++ {
		if err := batchWrite(d, [][]byte{
			append([]byte{0x53}, nvm[i*8:i*8+8]...),
			nil,
			[]byte{0x97, 0x01}, []byte{0x96, 0x50},
			nil,
			[]byte{0x97, 0x06}, []byte{0x96, 0x50 + i},
			nil, nil,
		}); err != nil {
			return err
		}
	}
	if err := batchWrite(d, [][]byte{
		[]byte{0x96, 0x40, 0x00}, []byte{0x95, 0x00},
	}); err != nil {
		return err
	}
	return nil
}

func Reset(d *i2c.Dev) error {
	return d.Tx([]byte{0x23, 0x01}, nil)
}

func run() error {
	if _, err := host.Init(); err != nil {
		return err
	}
	b, err := i2creg.Open("")
	if err != nil {
		return err
	}
	defer b.Close()

	d := &i2c.Dev{Addr: 0x28, Bus: b}

	/////

	n := DefaultNVM
	c := Config{}
	c.FromNVM(&n)
	c.EnableWithoutPD = false
	c.PDO3Voltage = 12
	c.PDO3Current = Current2000mA
	c.PDO2Voltage = 12
	c.PDO2Current = Current2000mA
	c.ToNVM(&n)
	if err := n.Write(d); err != nil {
		return err
	}
	log.Printf("%+v", c)

	////

	if err := Reset(d); err != nil {
		return err
	}

	return nil
}

func main() {
	if err := run(); err != nil {
		log.Fatal(err)
	}
}
