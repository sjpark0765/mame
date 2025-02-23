// license:BSD-3-Clause
// copyright-holders:Ryan Holtz
/*********************************************************************

    SGI Indigo workstation

    To-Do:
    - IP12 (R3000):
     * Everything
    - IP20 (R4000):
     * Figure out why the keyboard/mouse diagnostic fails
     * Work out a proper RAM mapping, or why the installer bails due
       to trying to access virtual address ffffa02c:
       88002584: lw        $sp,-$5fd4($0)

**********************************************************************/

#include "emu.h"
//#include "cpu/dsp56156/dsp56156.h"
#include "cpu/mips/mips1.h"
#include "cpu/mips/r4000.h"
#include "machine/eepromser.h"

#include "hpc1.h"
#include "mc.h"
#include "light.h"

#define LOG_UNKNOWN     (1U << 1)
#define LOG_INT         (1U << 2)
#define LOG_DSP         (1U << 3)
#define LOG_ALL         (LOG_UNKNOWN | LOG_INT | LOG_DSP)

#define VERBOSE         (LOG_UNKNOWN)
#include "logmacro.h"


namespace {

class indigo_state : public driver_device
{
public:
	indigo_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_hpc(*this, "hpc")
		, m_eeprom(*this, "eeprom")
		, m_gfx(*this, "lg1")
	{
	}

	void indigo_base(machine_config &config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

	uint32_t int_r(offs_t offset, uint32_t mem_mask = ~0);
	void int_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);
	uint32_t dsp_ram_r(offs_t offset, uint32_t mem_mask = ~0);
	void dsp_ram_w(offs_t offset, uint32_t data, uint32_t mem_mask = ~0);

	void indigo_map(address_map &map);

	required_device<hpc1_device> m_hpc;
	required_device<eeprom_serial_93cxx_device> m_eeprom;
	std::unique_ptr<uint32_t[]> m_dsp_ram;
	required_device<sgi_lg1_device> m_gfx;
	address_space *m_space = nullptr;
};

class indigo3k_state : public indigo_state
{
public:
	indigo3k_state(const machine_config &mconfig, device_type type, const char *tag)
		: indigo_state(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
	{
	}

	void indigo3k(machine_config &config);

protected:
	void mem_map(address_map &map);

	required_device<r3000a_device> m_maincpu;
};

class indigo4k_state : public indigo_state
{
public:
	indigo4k_state(const machine_config &mconfig, device_type type, const char *tag)
		: indigo_state(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_mem_ctrl(*this, "memctrl")
	{
	}

	void indigo4k(machine_config &config);

protected:
	virtual void machine_reset() override;

	void mem_map(address_map &map);

	required_device<r4000_device> m_maincpu;
	required_device<sgi_mc_device> m_mem_ctrl;
};

void indigo_state::machine_start()
{
	m_dsp_ram = std::make_unique<uint32_t[]>(0x8000);
	save_pointer(NAME(&m_dsp_ram[0]), 0x8000);
}

void indigo_state::machine_reset()
{
}

void indigo4k_state::machine_reset()
{
	indigo_state::machine_reset();
}

uint32_t indigo_state::int_r(offs_t offset, uint32_t mem_mask)
{
	LOGMASKED(LOG_INT, "%s: INT Read: %08x & %08x\n", machine().describe_context(), 0x1fbd9000 + offset*4, mem_mask);
	return 0;
}

void indigo_state::int_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	LOGMASKED(LOG_INT, "%s: INT Write: %08x = %08x & %08x\n", machine().describe_context(), 0x1fbd9000 + offset*4, data, mem_mask);
}

uint32_t indigo_state::dsp_ram_r(offs_t offset, uint32_t mem_mask)
{
	LOGMASKED(LOG_DSP, "%s: DSP RAM Read: %08x = %08x & %08x\n", machine().describe_context(), 0x1fbe0000 + offset*4, m_dsp_ram[offset], mem_mask);
	return m_dsp_ram[offset];
}

void indigo_state::dsp_ram_w(offs_t offset, uint32_t data, uint32_t mem_mask)
{
	LOGMASKED(LOG_DSP, "%s: DSP RAM Write: %08x = %08x & %08x\n", machine().describe_context(), 0x1fbe0000 + offset*4, data, mem_mask);
	m_dsp_ram[offset] = data;
}


void indigo_state::indigo_map(address_map &map)
{
	map(0x1f3f0000, 0x1f3f7fff).m(m_gfx, FUNC(sgi_lg1_device::map));
	map(0x1fb80000, 0x1fb8ffff).rw(m_hpc, FUNC(hpc1_device::read), FUNC(hpc1_device::write));
	map(0x1fbd9000, 0x1fbd903f).rw(FUNC(indigo_state::int_r), FUNC(indigo_state::int_w));
	map(0x1fbe0000, 0x1fbfffff).rw(FUNC(indigo_state::dsp_ram_r), FUNC(indigo_state::dsp_ram_w));
}

void indigo3k_state::mem_map(address_map &map)
{
	indigo_map(map);
	map(0x1fc00000, 0x1fc3ffff).rom().region("user1", 0);
}

void indigo4k_state::mem_map(address_map &map)
{
	indigo_map(map);
	map(0x1fa00000, 0x1fa1ffff).rw(m_mem_ctrl, FUNC(sgi_mc_device::read), FUNC(sgi_mc_device::write));
	map(0x1fc00000, 0x1fc7ffff).rom().region("user1", 0);
}

static INPUT_PORTS_START(indigo)
INPUT_PORTS_END

void indigo_state::indigo_base(machine_config &config)
{
	SGI_LG1(config, m_gfx);

	EEPROM_93C56_16BIT(config, m_eeprom);
}

void indigo3k_state::indigo3k(machine_config &config)
{
	indigo_base(config);

	R3000A(config, m_maincpu, 33.333_MHz_XTAL, 32768, 32768);
	downcast<r3000a_device &>(*m_maincpu).set_endianness(ENDIANNESS_BIG);
	m_maincpu->set_addrmap(AS_PROGRAM, &indigo3k_state::mem_map);

	SGI_HPC1(config, m_hpc, m_maincpu, m_eeprom);
}

static DEVICE_INPUT_DEFAULTS_START(ip20_mc)
	DEVICE_INPUT_DEFAULTS("VALID", 0x0f, 0x07)
DEVICE_INPUT_DEFAULTS_END

void indigo4k_state::indigo4k(machine_config &config)
{
	indigo_base(config);

	R4000(config, m_maincpu, 50000000);
	//m_maincpu->set_icache_size(32768);
	//m_maincpu->set_dcache_size(32768);
	m_maincpu->set_addrmap(AS_PROGRAM, &indigo4k_state::mem_map);

	SGI_MC(config, m_mem_ctrl, m_maincpu, m_eeprom, 50000000);
	m_mem_ctrl->eisa_present().set_constant(0);
	m_mem_ctrl->set_input_default(DEVICE_INPUT_DEFAULTS_NAME(ip20_mc));

	SGI_HPC1(config, m_hpc, m_maincpu, m_eeprom);
}

ROM_START( indigo3k )
	ROM_REGION32_BE( 0x40000, "user1", 0 )
	ROM_SYSTEM_BIOS( 0, "401-rev-c", "SGI Version 4.0.1 Rev C LG1/GR2, Jul 9, 1992" ) // dumped over serial connection from boot monitor and swapped
	ROMX_LOAD( "ip12prom.070-8088-xxx.u56", 0x000000, 0x040000, CRC(25ca912f) SHA1(94b3753d659bfe50b914445cef41290122f43880), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(0) )
	ROM_SYSTEM_BIOS( 1, "401-rev-d", "SGI Version 4.0.1 Rev D LG1/GR2, Mar 24, 1992" ) // dumped with EPROM programmer
	ROMX_LOAD( "ip12prom.070-8088-002.u56", 0x000000, 0x040000, CRC(ea4329ef) SHA1(b7d67d0e30ae8836892f7170dd4757732a0a3fd6), ROM_GROUPWORD | ROM_REVERSE | ROM_BIOS(1) )
ROM_END

ROM_START( indigo4k )
	ROM_REGION64_BE( 0x80000, "user1", 0 )
	ROM_SYSTEM_BIOS( 0, "405d-rev-a", "SGI Version 4.0.5D Rev A IP20, Aug 19, 1992" )
	ROMX_LOAD( "ip20prom.070-8116-004.bin", 0x000000, 0x080000, CRC(940d960e) SHA1(596aba530b53a147985ff3f6f853471ce48c866c), ROM_GROUPDWORD | ROM_REVERSE | ROM_BIOS(0) )
	ROM_SYSTEM_BIOS( 1, "405g-rev-b", "SGI Version 4.0.5G Rev B IP20, Nov 10, 1992" ) // dumped over serial connection from boot monitor and swapped
	ROMX_LOAD( "ip20prom.070-8116-005.bin", 0x000000, 0x080000, CRC(1875b645) SHA1(52f5d7baea3d1bc720eb2164104c177e23504345), ROM_GROUPDWORD | ROM_REVERSE | ROM_BIOS(1) )
ROM_END

} // anonymous namespace


//    YEAR  NAME      PARENT  COMPAT  MACHINE   INPUT   CLASS           INIT        COMPANY                 FULLNAME                                          FLAGS
COMP( 1991, indigo3k, 0,      0,      indigo3k, indigo, indigo3k_state, empty_init, "Silicon Graphics Inc", "IRIS Indigo (R3000, 33MHz)",                     MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
COMP( 1993, indigo4k, 0,      0,      indigo4k, indigo, indigo4k_state, empty_init, "Silicon Graphics Inc", "IRIS Indigo (R4400, 150MHz)",                    MACHINE_NOT_WORKING | MACHINE_NO_SOUND )
