// license:BSD-3-Clause
// copyright-holders:Peter Trauner, hap
/*******************************************************************************

Commodore Chessmate / Novag Chess Champion MK II
Initial driver version by PeT mess@utanet.at September 2000.
Driver mostly rewritten later.

The hardware is pretty similar to KIM-1. In fact, the chess engine is Peter
R. Jennings's Microchess, originally made for the KIM-1. Jennings went on to
co-found Personal Software (later named VisiCorp, known for VisiCalc).

Jennings also licensed Chessmate to Novag, and they released it as the MK II.
The hardware is almost identical and the software is the same(identical ROM labels).
Two designs were made, one jukebox shape, and one brick shape. The one in MAME came
from the jukebox, but both models have the same ROMs.

Note that like MK I, although it is a Winkler/Auge production, it doesn't involve
SciSys company. SciSys was founded by Winkler after MK II.

TODO:
- is there an older version of chmate? chips on pcb photos are dated 1979, but
  the game is known to be released in 1978

================================================================================

Hardware notes:

MOS MPS 6504 2179
MOS MPS 6530 024 1879
 layout of 6530 dumped with my adapter
 0x1300-0x133f io
 0x1380-0x13bf ram
 0x1400-0x17ff rom

2*MPS6111 RAM (256x4)
MOS MPS 6332 005 2179
74145 bcd to decimal encoder

4x 7 segment led display
4 single leds
19 buttons (11 on brick model)

*******************************************************************************/

#include "emu.h"
#include "cpu/m6502/m6504.h"
#include "machine/mos6530n.h"
#include "sound/dac.h"
#include "video/pwm.h"
#include "speaker.h"

// internal artwork
#include "chessmate.lh" // clickable
#include "novag_mk2.lh" // clickable
#include "novag_mk2a.lh" // clickable


namespace {

class chmate_state : public driver_device
{
public:
	chmate_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, "maincpu"),
		m_miot(*this, "miot"),
		m_display(*this, "display"),
		m_dac(*this, "dac"),
		m_inputs(*this, "IN.%u", 0)
	{ }

	// machine configs
	void chmate(machine_config &config);
	void mk2(machine_config &config);
	void mk2a(machine_config &config);

	DECLARE_INPUT_CHANGED_MEMBER(reset_button);

protected:
	virtual void machine_start() override;

private:
	// devices/pointers
	required_device<cpu_device> m_maincpu;
	required_device<mos6530_new_device> m_miot;
	required_device<pwm_display_device> m_display;
	required_device<dac_bit_interface> m_dac;
	optional_ioport_array<5> m_inputs;

	// address maps
	void main_map(address_map &map);

	// I/O handlers
	void update_display();
	void control_w(u8 data);
	void digit_w(u8 data);
	u8 input_r();

	u8 m_inp_mux = 0;
	u8 m_7seg_data = 0;
	u8 m_led_data = 0;
};

void chmate_state::machine_start()
{
	// register for savestates
	save_item(NAME(m_inp_mux));
	save_item(NAME(m_7seg_data));
	save_item(NAME(m_led_data));
}

INPUT_CHANGED_MEMBER(chmate_state::reset_button)
{
	// assume that NEW GAME button is tied to reset pin(s)
	m_maincpu->set_input_line(INPUT_LINE_RESET, newval ? ASSERT_LINE : CLEAR_LINE);
	if (newval)
		m_miot->reset();
}



/*******************************************************************************
    I/O
*******************************************************************************/

// 6530 ports

void chmate_state::update_display()
{
	m_display->write_row(4, m_led_data);
	m_display->matrix_partial(0, 4, 1 << m_inp_mux, m_7seg_data);
}

void chmate_state::control_w(u8 data)
{
	// d0-d2: 74145 to input mux/digit select
	m_inp_mux = data & 7;

	// 74145 Q7: speaker out
	m_dac->write(BIT(1 << m_inp_mux, 7) & ~m_inputs[4].read_safe(0));

	// d3-d5: leds (direct)
	m_led_data = data >> 3 & 7;
	update_display();

	// d6: chipselect used?
	// d7: IRQ out
	m_maincpu->set_input_line(M6502_IRQ_LINE, (data & 0x80) ? CLEAR_LINE : ASSERT_LINE);
}

void chmate_state::digit_w(u8 data)
{
	m_7seg_data = data;
	update_display();
}

u8 chmate_state::input_r()
{
	u8 data = 0;

	// multiplexed inputs (74145 Q4,Q5)
	if (m_inp_mux == 4 || m_inp_mux == 5)
	{
		// note that number/letter buttons are electronically the same
		u8 i = m_inp_mux - 4;
		data = m_inputs[i]->read() | m_inputs[i | 2].read_safe(0);
	}

	return ~data;
}



/*******************************************************************************
    Address Maps
*******************************************************************************/

void chmate_state::main_map(address_map &map)
{
	map.global_mask(0x1fff);
	map(0x0000, 0x00ff).mirror(0x0100).ram();
	map(0x0b00, 0x0b0f).mirror(0x0030).m(m_miot, FUNC(mos6530_new_device::io_map));
	map(0x0b80, 0x0bbf).m(m_miot, FUNC(mos6530_new_device::ram_map));
	map(0x0c00, 0x0fff).m(m_miot, FUNC(mos6530_new_device::rom_map));
	map(0x1000, 0x1fff).rom();
}



/*******************************************************************************
    Input Ports
*******************************************************************************/

static INPUT_PORTS_START( chmate )
	PORT_START("IN.0")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_F) PORT_NAME("F / Skill Level")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_E) PORT_NAME("E / Stop Clock")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_D) PORT_NAME("D / Display Time")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_C) PORT_NAME("C / Chess Clock")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_B) PORT_NAME("B / Board Verify")
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_A) PORT_NAME("A / White")

	PORT_START("IN.1")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("Enter")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_DEL) PORT_CODE(KEYCODE_BACKSPACE) PORT_NAME("Clear")
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_H) PORT_NAME("H / Black")
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_G) PORT_NAME("G / Game Moves")

	PORT_START("IN.2")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_NAME("6")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_NAME("5")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_NAME("4")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_NAME("3")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_NAME("2")
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_NAME("1")

	PORT_START("IN.3")
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_NAME("8")
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_NAME("7")

	PORT_START("RESET")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_N) PORT_CHANGED_MEMBER(DEVICE_SELF, chmate_state, reset_button, 0) PORT_NAME("New Game")
INPUT_PORTS_END

static INPUT_PORTS_START( mk2 ) // meaning of black/white reversed
	PORT_INCLUDE( chmate )

	PORT_MODIFY("IN.0")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_F) PORT_NAME("F / Level")
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_A) PORT_NAME("A / Black")

	PORT_MODIFY("IN.1")
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_H) PORT_NAME("H / White")
INPUT_PORTS_END

static INPUT_PORTS_START( mk2a )
	PORT_START("IN.0")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_CODE(KEYCODE_F) PORT_NAME("6 / F / Level")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_CODE(KEYCODE_E) PORT_NAME("5 / E / Stop Clock / Rook")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_CODE(KEYCODE_D) PORT_NAME("4 / D / Display Time")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_CODE(KEYCODE_C) PORT_NAME("3 / C / Chess Clock / Bishop")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_CODE(KEYCODE_B) PORT_NAME("2 / B / Board Verify / Knight")
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_CODE(KEYCODE_A) PORT_NAME("1 / A / White / Pawn")

	PORT_START("IN.1")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("Enter")
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_DEL) PORT_CODE(KEYCODE_BACKSPACE) PORT_NAME("Clear")
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_CODE(KEYCODE_H) PORT_NAME("8 / H / Black / Queen")
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_CODE(KEYCODE_G) PORT_NAME("7 / G / Game Moves")

	PORT_START("IN.4")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_OTHER) PORT_CODE(KEYCODE_S) PORT_TOGGLE PORT_NAME("Sound Switch")

	PORT_START("RESET")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_N) PORT_CHANGED_MEMBER(DEVICE_SELF, chmate_state, reset_button, 0) PORT_NAME("New Game")
INPUT_PORTS_END



/*******************************************************************************
    Machine Configs
*******************************************************************************/

void chmate_state::chmate(machine_config &config)
{
	// basic machine hardware
	M6504(config, m_maincpu, 8_MHz_XTAL/8);
	m_maincpu->set_addrmap(AS_PROGRAM, &chmate_state::main_map);

	MOS6530_NEW(config, m_miot, 8_MHz_XTAL/8);
	m_miot->pa_rd_callback().set(FUNC(chmate_state::input_r));
	m_miot->pa_wr_callback().set(FUNC(chmate_state::digit_w));
	m_miot->pb_wr_callback().set(FUNC(chmate_state::control_w));

	// video hardware
	PWM_DISPLAY(config, m_display).set_size(4+1, 8);
	m_display->set_segmask(0xf, 0xff);
	config.set_default_layout(layout_chessmate);

	// sound hardware
	SPEAKER(config, "speaker").front_center();
	DAC_1BIT(config, m_dac).add_route(ALL_OUTPUTS, "speaker", 0.25);
}

void chmate_state::mk2(machine_config &config)
{
	chmate(config);
	config.set_default_layout(layout_novag_mk2);
}

void chmate_state::mk2a(machine_config &config)
{
	chmate(config);
	config.set_default_layout(layout_novag_mk2a);
}



/*******************************************************************************
    ROM Definitions
*******************************************************************************/

ROM_START( chmate )
	ROM_REGION( 0x2000, "maincpu", 0 )
	ROM_LOAD("6332_005", 0x1000, 0x1000, CRC(6f10991b) SHA1(90cdc5a15d9ad813ad20410f21081c6e3e481812) )

	ROM_REGION( 0x400, "miot", 0 )
	ROM_LOAD("6530_024", 0x0000, 0x0400, CRC(4f28c443) SHA1(e33f8b7f38e54d7a6e0f0763f2328cc12cb0eade) )
ROM_END

#define rom_ccmk2 rom_chmate
#define rom_ccmk2a rom_chmate

} // anonymous namespace



/*******************************************************************************
    Drivers
*******************************************************************************/

//    YEAR  NAME    PARENT  COMPAT  MACHINE  INPUT   CLASS         INIT        COMPANY, FULLNAME, FLAGS
SYST( 1978, chmate, 0,      0,      chmate,  chmate, chmate_state, empty_init, "Commodore", "Chessmate", MACHINE_SUPPORTS_SAVE | MACHINE_CLICKABLE_ARTWORK )

SYST( 1979, ccmk2,  chmate, 0,      mk2,     mk2,    chmate_state, empty_init, "Novag", "Chess Champion: MK II (ver. 1)", MACHINE_SUPPORTS_SAVE | MACHINE_CLICKABLE_ARTWORK ) // 1st version (jukebox model), aka version B
SYST( 1979, ccmk2a, chmate, 0,      mk2a,    mk2a,   chmate_state, empty_init, "Novag", "Chess Champion: MK II (ver. 2)", MACHINE_SUPPORTS_SAVE | MACHINE_CLICKABLE_ARTWORK )
