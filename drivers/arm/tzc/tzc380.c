/*
 * Copyright 2017 NXP
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <debug.h>
#include <mmio.h>
#include <stddef.h>
#include <tzc380.h>

struct tzc380_instance {
	uintptr_t base;
	uint8_t addr_width;
	uint8_t num_regions;
};

struct tzc380_instance tzc380;

static unsigned int tzc380_read_build_config(uintptr_t base)
{
	return mmio_read_32(base + BUILD_CONFIG_OFF);
}

static void tzc380_write_action(uintptr_t base, tzc_action_t action)
{
	mmio_write_32(base + ACTION_OFF, action);
}

static void tzc380_write_region_base_low(uintptr_t base, unsigned int region,
				      unsigned int val)
{
	mmio_write_32(base + REGION_SETUP_LOW_OFF(region), val);
}

static void tzc380_write_region_base_high(uintptr_t base, unsigned int region,
				       unsigned int val)
{
	mmio_write_32(base + REGION_SETUP_HIGH_OFF(region), val);
}

static void tzc380_write_region_attributes(uintptr_t base, unsigned int region,
					unsigned int val)
{
	mmio_write_32(base + REGION_ATTRIBUTES_OFF(region), val);
}

void tzc380_init(uintptr_t base)
{
	unsigned int tzc_build;

	assert(base);
	tzc380.base = base;

	/* Save values we will use later. */
	tzc_build = tzc380_read_build_config(tzc380.base);
	tzc380.addr_width  = ((tzc_build >> BUILD_CONFIG_AW_SHIFT) &
			      BUILD_CONFIG_AW_MASK) + 1;
	tzc380.num_regions = ((tzc_build >> BUILD_CONFIG_NR_SHIFT) &
			       BUILD_CONFIG_NR_MASK) + 1;
}

static unsigned int addr_low(uintptr_t addr)
{
	return (unsigned int)addr;
}

static unsigned int addr_high(uintptr_t addr __unused)
{
#if (UINTPTR_MAX == UINT64_MAX)
	return addr >> 32;
#else
	return 0;
#endif
}


/*
 * `tzc380_configure_region` is used to program regions into the TrustZone
 * controller.
 */
void tzc380_configure_region(uint8_t region, uintptr_t region_base, unsigned int attr)
{
	assert(tzc380.base);

	assert(region < tzc380.num_regions);

	tzc380_write_region_base_low(tzc380.base, region, addr_low(region_base));
	tzc380_write_region_base_high(tzc380.base, region, addr_high(region_base));
	tzc380_write_region_attributes(tzc380.base, region, attr);
}

void tzc380_set_action(tzc_action_t action)
{
	assert(tzc380.base);

	/*
	 * - Currently no handler is provided to trap an error via interrupt
	 *   or exception.
	 * - The interrupt action has not been tested.
	 */
	tzc380_write_action(tzc380.base, action);
}

#if LOG_LEVEL >= LOG_LEVEL_INFO

static unsigned int tzc380_read_region_attributes(uintptr_t base, unsigned int region)
{
	return mmio_read_32(base + REGION_ATTRIBUTES_OFF(region));
}

static unsigned int tzc380_read_region_base_low(uintptr_t base, unsigned int region)
{
	return mmio_read_32(base + REGION_SETUP_LOW_OFF(region));
}

static unsigned int tzc380_read_region_base_high(uintptr_t base, unsigned int region)
{
	return mmio_read_32(base + REGION_SETUP_HIGH_OFF(region));
}

#define	REGION_MAX	16
void tzc380_dump_state(void)
{
	unsigned int n;
	unsigned int temp_32reg, temp_32reg_h;

	INFO("enter\n");
	INFO("security_inversion_en %x\n",
	     mmio_read_32(tzc380.base + SECURITY_INV_EN_OFF));
	for (n = 0; n <= REGION_MAX; n++) {
		temp_32reg = tzc380_read_region_attributes(tzc380.base, n);
		if (!(temp_32reg & TZC_ATTR_REGION_EN_MASK))
			continue;

		INFO("\n");
		INFO("region %d\n", n);
		temp_32reg = tzc380_read_region_base_low(tzc380.base, n);
		temp_32reg_h = tzc380_read_region_base_high(tzc380.base, n);
		INFO("region_base: 0x%08x%08x\n", temp_32reg_h, temp_32reg);
		temp_32reg = tzc380_read_region_attributes(tzc380.base, n);
		INFO("region sp: %x\n", temp_32reg >> TZC_ATTR_SP_SHIFT);
		INFO("region size: %x\n", (temp_32reg & TZC_REGION_SIZE_MASK) >>
				TZC_REGION_SIZE_SHIFT);
	}
	INFO("exit\n");
}

#endif
