/*
 * Copyright (c) 2018-2020 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <exosphere.hpp>
#include "secmon_boot.hpp"
#include "secmon_boot_cache.hpp"
#include "secmon_boot_functions.hpp"

namespace ams::secmon::boot {

    namespace {

        constexpr inline uintptr_t SYSCTR0 = MemoryRegionVirtualDeviceSysCtr0.GetAddress();

        constinit const u8 BootConfigRsaPublicModulus[se::RsaSize] = {
            0xB5, 0x96, 0x87, 0x31, 0x39, 0xAA, 0xBB, 0x3C, 0x28, 0xF3, 0xF0, 0x65, 0xF1, 0x50, 0x70, 0x64,
            0xE6, 0x6C, 0x97, 0x50, 0xCD, 0xA6, 0xEE, 0xEA, 0xC3, 0x8F, 0xE6, 0xB5, 0x81, 0x54, 0x65, 0x33,
            0x1B, 0x88, 0x4B, 0xCE, 0x9F, 0x53, 0xDF, 0xE4, 0xF6, 0xAD, 0xC3, 0x78, 0xD7, 0x3C, 0xD1, 0xDB,
            0x27, 0x21, 0xA0, 0x24, 0x30, 0x2D, 0x98, 0x41, 0xA8, 0xDF, 0x50, 0x7D, 0xAB, 0xCE, 0x00, 0xD9,
            0xCB, 0xAC, 0x8F, 0x37, 0xF5, 0x53, 0xE4, 0x97, 0x1F, 0x13, 0x3C, 0x19, 0xFF, 0x05, 0xA7, 0x3B,
            0xF6, 0xF4, 0x01, 0xDE, 0xF0, 0xC3, 0x77, 0x7B, 0x83, 0xBA, 0xAF, 0x99, 0x30, 0x94, 0x87, 0x25,
            0x4E, 0x54, 0x42, 0x3F, 0xAC, 0x27, 0xF9, 0xCC, 0x87, 0xDD, 0xAE, 0xF2, 0x54, 0xF3, 0x97, 0x49,
            0xF4, 0xB0, 0xF8, 0x6D, 0xDA, 0x60, 0xE0, 0xFD, 0x57, 0xAE, 0x55, 0xA9, 0x76, 0xEA, 0x80, 0x24,
            0xA0, 0x04, 0x7D, 0xBE, 0xD1, 0x81, 0xD3, 0x0C, 0x95, 0xCF, 0xB7, 0xE0, 0x2D, 0x21, 0x21, 0xFF,
            0x97, 0x1E, 0xB3, 0xD7, 0x9F, 0xBB, 0x33, 0x0C, 0x23, 0xC5, 0x88, 0x4A, 0x33, 0xB9, 0xC9, 0x4E,
            0x1E, 0x65, 0x51, 0x45, 0xDE, 0xF9, 0x64, 0x7C, 0xF0, 0xBF, 0x11, 0xB4, 0x93, 0x8D, 0x5D, 0xC6,
            0xAB, 0x37, 0x9E, 0xE9, 0x39, 0xC1, 0xC8, 0xDB, 0xB9, 0xFE, 0x45, 0xCE, 0x7B, 0xDD, 0x72, 0xD9,
            0x6F, 0x68, 0x13, 0xC0, 0x4B, 0xBA, 0x00, 0xF4, 0x1E, 0x89, 0x71, 0x91, 0x26, 0xA6, 0x46, 0x12,
            0xDF, 0x29, 0x6B, 0xC2, 0x5A, 0x53, 0xAF, 0xB9, 0x5B, 0xFD, 0x13, 0x9F, 0xD1, 0x8A, 0x7C, 0xB5,
            0x04, 0xFD, 0x69, 0xEA, 0x23, 0xB4, 0x6D, 0x16, 0x21, 0x98, 0x54, 0xB4, 0xDF, 0xE6, 0xAB, 0x93,
            0x36, 0xB6, 0xD2, 0x43, 0xCF, 0x2B, 0x98, 0x1D, 0x45, 0xC9, 0xBB, 0x20, 0x42, 0xB1, 0x9D, 0x1D
        };

    }

    void ClearIram() {
        /* Clear the boot code image from where it was loaded in IRAM. */
        util::ClearMemory(MemoryRegionPhysicalIramBootCodeImage.GetPointer(), MemoryRegionPhysicalIramBootCodeImage.GetSize());
    }

    void WaitForNxBootloader(const pkg1::SecureMonitorParameters &params, pkg1::BootloaderState state) {
        /* Check NX Bootloader's state once per microsecond until it's advanced enough. */
        while (params.bootloader_state < state) {
            util::WaitMicroSeconds(1);
        }
    }

    void LoadBootConfig(const void *src) {
        pkg1::BootConfig * const dst = secmon::impl::GetBootConfigStorage();

        if (pkg1::IsProduction()) {
            std::memset(dst, 0, sizeof(*dst));
        } else {
            hw::FlushDataCache(src, sizeof(*dst));
            hw::DataSynchronizationBarrierInnerShareable();
            std::memcpy(dst, src, sizeof(*dst));
        }
    }

    void VerifyOrClearBootConfig() {
        /* On production hardware, the boot config is already cleared. */
        if (pkg1::IsProduction()) {
            return;
        }

        pkg1::BootConfig * const bc = secmon::impl::GetBootConfigStorage();

        /* Determine if the bc is valid for the device. */
        bool valid_for_device = false;
        {
            const bool valid_signature = secmon::boot::VerifyBootConfigSignature(*bc, BootConfigRsaPublicModulus, util::size(BootConfigRsaPublicModulus));
            if (valid_signature) {
                valid_for_device = secmon::boot::VerifyBootConfigEcid(*bc);
            }
        }

        /* If the boot config is not valid for the device, clear its signed data. */
        if (!valid_for_device) {
            util::ClearMemory(std::addressof(bc->signed_data), sizeof(bc->signed_data));
        }
    }

    void EnableTsc(u64 initial_tsc_value) {
        /* Write the initial value to the CNTCV registers. */
        const u32 lo = static_cast<u32>(initial_tsc_value >>  0);
        const u32 hi = static_cast<u32>(initial_tsc_value >> 32);

        reg::Write(SYSCTR0 + SYSCTR0_CNTCV0, lo);
        reg::Write(SYSCTR0 + SYSCTR0_CNTCV1, hi);

        /* Configure the system counter control register. */
        reg::Write(SYSCTR0 + SYSCTR0_CNTCR, SYSCTR0_REG_BITS_ENUM(CNTCR_HDBG, ENABLE),
                                            SYSCTR0_REG_BITS_ENUM(CNTCR_EN,   ENABLE));
    }

    void WriteGpuCarveoutMagicNumbers() {
        /* Define the magic numbers. */
        constexpr u32 GpuMagicNumber       = 0xC0EDBBCC;
        constexpr u32 SkuInfo              = 0x83;
        constexpr u32 HdcpMicroCodeVersion = 0x2;
        constexpr u32 ChipIdErista         = 0x210;
        constexpr u32 ChipIdMariko         = 0x214;

        /* Get our pointers. */
        u32 *gpu_magic  = MemoryRegionDramGpuCarveout.GetEndPointer<u32>() - (0x004 / sizeof(*gpu_magic));
        u32 *tsec_magic = MemoryRegionDramGpuCarveout.GetEndPointer<u32>() - (0x100 / sizeof(*tsec_magic));

        /* Write the gpu magic number. */
        gpu_magic[0] = GpuMagicNumber;

        /* Write the tsec magic numbers. */
        tsec_magic[0] = SkuInfo;
        tsec_magic[1] = HdcpMicroCodeVersion;
        tsec_magic[2] = (false /* TODO: IsMariko */) ? ChipIdMariko : ChipIdErista;

        /* Flush the magic numbers. */
        hw::FlushDataCache(gpu_magic,  1 * sizeof(u32));
        hw::FlushDataCache(tsec_magic, 3 * sizeof(u32));
        hw::DataSynchronizationBarrierInnerShareable();
    }

}