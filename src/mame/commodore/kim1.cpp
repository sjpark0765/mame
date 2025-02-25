// license:GPL-2.0+
// copyright-holders:Juergen Buchmueller
/******************************************************************************

MOS Technology KIM-1

The cassette interface
======================
The KIM-1 stores data on cassette using 2 frequencies: ~3700Hz (high) and ~2400Hz
(low). A high tone is output for 9 cycles and a low tone for 6 cycles. A logic bit
is encoded using 3 sequences of high and low tones. It always starts with a high
tone and ends with a low tone. The middle tone is high for a logic 0 and low for
0 logic 1.

These high and low tone signals are fed to a circuit containing a LM565 PLL and
a 311 comparator. For a high tone a 1 is passed to DB7 of 6530-U2 for a low tone
a 0 is passed. The KIM-1 software measures the time it takes for the signal to
change from 1 to 0.

How to use cassette:
    00F1      00 to clear decimal mode
    17F5-17F6 start address low and high
    17F7-17F8 end address low and high
    17F9      2 digit program ID
    1800      press GO to save tape
    1873      press GO to load tape
NOTE: save end address is next address from program end


Keyboard and Display logic
==========================
PA0-PA6 of 6530-U2 are connected to the columns of the keyboard matrix. These
columns are also connected to segments A-G of the LEDs. PB1-PB3 of 6530-U2 are
connected to a 74145 BCD which connects outputs 0-2 to the rows of the keyboard
matrix. Outputs 4-9 of the 74145 are connected to LEDs U18-U23

When a key is pressed the corresponding input to PA0-PA6 is set low and the KIM-1
software reads this signal. The KIM-1 software sends an output signal to PA0-PA6
and the corresponding segments of an LED are illuminated.

LED: six 7-segment LEDs
    left 4 digits (address)
    right 2 digits (data)
Keyboard: 23 keys and SST switch
    0-F  16 keys to enter data
    AD   address entry mode
    DA   data entry mode
    +    increment address
    PC   recalls address stored in the Program Counter
    RS   system reset
    GO   execute program
    ST   program stop
    SST  single step slide switch

Paste test:
    R-0100=11^22^33^44^55^66^77^88^99^-0100=
    Press UP to verify data.


TODO:
- LEDs should be dark at startup (RS key to activate)
- slots for expansion & application ports
- add TTY support

******************************************************************************/

#include "emu.h"

#include "cpu/m6502/m6502.h"
#include "formats/kim1_cas.h"
#include "imagedev/cassette.h"
#include "machine/mos6530n.h"
#include "machine/timer.h"
#include "video/pwm.h"

#include "softlist_dev.h"
#include "speaker.h"

#include "kim1.lh"


namespace {

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class kim1_state : public driver_device
{
public:
	kim1_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_miot(*this, "miot%u", 0)
		, m_digit_pwm(*this, "digit_pwm")
		, m_cass(*this, "cassette")
		, m_row(*this, "ROW%u", 0U)
		, m_special(*this, "SPECIAL")
	{ }

	DECLARE_INPUT_CHANGED_MEMBER(trigger_reset);
	DECLARE_INPUT_CHANGED_MEMBER(trigger_nmi);
	void kim1(machine_config &config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	required_device<m6502_device> m_maincpu;
	required_device_array<mos6530_new_device, 2> m_miot;
	required_device<pwm_display_device> m_digit_pwm;
	required_device<cassette_image_device> m_cass;
	required_ioport_array<3> m_row;
	required_ioport m_special;

	void mem_map(address_map &map);
	void sync_map(address_map &map);

	uint8_t sync_r(offs_t offset);
	void sync_w(int state);

	uint8_t u2_read_a();
	void u2_write_a(uint8_t data);
	uint8_t u2_read_b();
	void u2_write_b(uint8_t data);

	TIMER_DEVICE_CALLBACK_MEMBER(cassette_input);

	int m_sync_state = 0;
	bool m_k7 = false;
	uint8_t m_u2_port_b = 0;
	uint8_t m_311_output = 0;
	uint32_t m_cassette_high_count = 0;
};

void kim1_state::machine_start()
{
	// Register for save states
	save_item(NAME(m_sync_state));
	save_item(NAME(m_k7));
	save_item(NAME(m_u2_port_b));
	save_item(NAME(m_311_output));
	save_item(NAME(m_cassette_high_count));
}

void kim1_state::machine_reset()
{
	m_311_output = 0;
	m_cassette_high_count = 0;
}


//**************************************************************************
//  I/O
//**************************************************************************

INPUT_CHANGED_MEMBER(kim1_state::trigger_reset)
{
	// RS key triggers system reset via 556 timer
	if (newval)
		machine().schedule_soft_reset();
}

INPUT_CHANGED_MEMBER(kim1_state::trigger_nmi)
{
	// ST key triggers NMI via 556 timer
	if (newval)
		m_maincpu->pulse_input_line(INPUT_LINE_NMI, attotime::zero);
}

uint8_t kim1_state::sync_r(offs_t offset)
{
	// A10-A12 to 74145
	if (!machine().side_effects_disabled())
		m_k7 = bool(~offset & 0x1c00);

	return m_maincpu->space(AS_PROGRAM).read_byte(offset);
}

void kim1_state::sync_w(int state)
{
	// Signal NMI at falling edge of SYNC when SST is enabled and K7 line is high
	if (m_sync_state && !state && m_k7 && BIT(m_special->read(), 2))
		m_maincpu->pulse_input_line(INPUT_LINE_NMI, attotime::zero);

	m_sync_state = state;
}

uint8_t kim1_state::u2_read_a()
{
	uint8_t data = 0x7f;

	// Read from keyboard
	offs_t const sel = (m_u2_port_b >> 1) & 0x0f;
	if (3U > sel)
		data = m_row[sel]->read() & 0x7f;

	return data | 0x80;
}

void kim1_state::u2_write_a(uint8_t data)
{
	// Write to 7-segment LEDs
	m_digit_pwm->write_mx(data & 0x7f);
}

uint8_t kim1_state::u2_read_b()
{
	if (m_u2_port_b & 0x20)
		return 0xff;

	// Load from cassette
	return 0x7f | (m_311_output ^ 0x80);
}

void kim1_state::u2_write_b(uint8_t data)
{
	m_u2_port_b = data;

	// Select 7-segment LED
	m_digit_pwm->write_my(1 << (data >> 1 & 0xf) >> 4);

	// Cassette write/speaker update
	if (data & 0x20)
		m_cass->output((data & 0x80) ? -1.0 : 1.0);
}

TIMER_DEVICE_CALLBACK_MEMBER(kim1_state::cassette_input)
{
	double tap_val = m_cass->input();

	if (tap_val <= 0)
	{
		if (m_cassette_high_count)
		{
			m_311_output = (m_cassette_high_count < 8) ? 0x80 : 0;
			m_cassette_high_count = 0;
		}
	}

	if (tap_val > 0)
		m_cassette_high_count++;
}


//**************************************************************************
//  ADDRESS MAPS
//**************************************************************************

void kim1_state::mem_map(address_map &map)
{
	map.global_mask(0x1fff);
	map(0x0000, 0x03ff).ram();
	map(0x1700, 0x170f).mirror(0x0030).m(m_miot[1], FUNC(mos6530_new_device::io_map));
	map(0x1740, 0x174f).mirror(0x0030).m(m_miot[0], FUNC(mos6530_new_device::io_map));
	map(0x1780, 0x17bf).m(m_miot[1], FUNC(mos6530_new_device::ram_map));
	map(0x17c0, 0x17ff).m(m_miot[0], FUNC(mos6530_new_device::ram_map));
	map(0x1800, 0x1bff).m(m_miot[1], FUNC(mos6530_new_device::rom_map));
	map(0x1c00, 0x1fff).m(m_miot[0], FUNC(mos6530_new_device::rom_map));
}

void kim1_state::sync_map(address_map &map)
{
	map(0x0000, 0xffff).r(FUNC(kim1_state::sync_r));
}


//**************************************************************************
//  INPUT PORTS
//**************************************************************************

static INPUT_PORTS_START( kim1 )
	PORT_START("ROW0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_CHAR('6')
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_CHAR('5')
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_CHAR('4')
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_CHAR('3')
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_CHAR('2')
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_CHAR('1')
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_CHAR('0')

	PORT_START("ROW1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_D) PORT_CHAR('D')
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_C) PORT_CHAR('C')
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_B) PORT_CHAR('B')
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_A) PORT_CHAR('A')
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_CHAR('9')
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_CHAR('8')
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_CHAR('7')

	PORT_START("ROW2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_P) PORT_CHAR('P') PORT_NAME("PC")
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_CODE(KEYCODE_X) PORT_CHAR('X') PORT_NAME("GO")
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_UP) PORT_CODE(KEYCODE_PLUS_PAD) PORT_CHAR('^') PORT_NAME("+")
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_EQUALS) PORT_CHAR('=') PORT_NAME("DA")
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_MINUS) PORT_CODE(KEYCODE_MINUS_PAD) PORT_CHAR('-') PORT_NAME("AD")
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F) PORT_CHAR('F')
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_CODE(KEYCODE_E) PORT_CHAR('E')

	PORT_START("SPECIAL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_S) PORT_CHAR('S') PORT_NAME("ST") PORT_CHANGED_MEMBER(DEVICE_SELF, kim1_state, trigger_nmi, 0)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_R) PORT_CHAR('R') PORT_NAME("RS") PORT_CHANGED_MEMBER(DEVICE_SELF, kim1_state, trigger_reset, 0)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F1) PORT_TOGGLE PORT_NAME("SST")
INPUT_PORTS_END


//**************************************************************************
//  MACHINE DRIVERS
//**************************************************************************

void kim1_state::kim1(machine_config &config)
{
	// basic machine hardware
	M6502(config, m_maincpu, 1_MHz_XTAL);
	m_maincpu->set_addrmap(AS_PROGRAM, &kim1_state::mem_map);
	m_maincpu->set_addrmap(AS_OPCODES, &kim1_state::sync_map);
	m_maincpu->sync_cb().set(FUNC(kim1_state::sync_w));

	// video hardware
	PWM_DISPLAY(config, m_digit_pwm).set_size(6, 7);
	m_digit_pwm->set_segmask(0x3f, 0x7f);
	config.set_default_layout(layout_kim1);

	// devices
	MOS6530_NEW(config, m_miot[0], 1_MHz_XTAL); // U2
	m_miot[0]->pa_rd_callback().set(FUNC(kim1_state::u2_read_a));
	m_miot[0]->pa_wr_callback().set(FUNC(kim1_state::u2_write_a));
	m_miot[0]->pb_rd_callback().set(FUNC(kim1_state::u2_read_b));
	m_miot[0]->pb_wr_callback().set(FUNC(kim1_state::u2_write_b));

	MOS6530_NEW(config, m_miot[1], 1_MHz_XTAL); // U3

	CASSETTE(config, m_cass);
	m_cass->set_formats(kim1_cassette_formats);
	m_cass->set_default_state(CASSETTE_STOPPED);
	m_cass->add_route(ALL_OUTPUTS, "mono", 0.05);
	m_cass->set_interface("kim1_cass");

	SPEAKER(config, "mono").front_center();

	TIMER(config, "cassette_timer").configure_periodic(FUNC(kim1_state::cassette_input), attotime::from_hz(44100));

	// software list
	SOFTWARE_LIST(config, "cass_list").set_original("kim1_cass");
}


//**************************************************************************
//  ROM DEFINITIONS
//**************************************************************************

ROM_START( kim1 )
	ROM_REGION( 0x400, "miot0", 0 )
	ROM_LOAD("6530-002.u2", 0x0000, 0x0400, CRC(2b08e923) SHA1(054f7f6989af3a59462ffb0372b6f56f307b5362))

	ROM_REGION( 0x400, "miot1", 0 )
	ROM_LOAD("6530-003.u3", 0x0000, 0x0400, CRC(a2a56502) SHA1(60b6e48f35fe4899e29166641bac3e81e3b9d220))
ROM_END

} // anonymous namespace


//**************************************************************************
//  SYSTEM DRIVERS
//**************************************************************************

//    YEAR  NAME  PARENT  COMPAT  MACHINE  INPUT  CLASS       INIT        COMPANY           FULLNAME  FLAGS
COMP( 1976, kim1, 0,      0,      kim1,    kim1,  kim1_state, empty_init, "MOS Technology", "KIM-1",  MACHINE_SUPPORTS_SAVE)
