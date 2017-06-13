/*
	GTA Remastered Controls Patch
	Copyright (C) 2016, TheFloW

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <systemctrl.h>

PSP_MODULE_INFO("GTARemasteredControls", 0x1007, 1, 0);

int sceKernelQuerySystemCall(void *function);

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

#define REDIRECT_FUNCTION(a, f) \
{ \
	u32 func = a; \
	_sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), func); \
	_sw(0, func + 4); \
}

enum GtaPad {
	PAD_LX = 1,
	PAD_LY = 2,
	PAD_RX = 3,
	PAD_RY = 4,
	PAD_RTRIGGER = 7,
	PAD_CROSS = 21,
};

STMOD_HANDLER previous;

static u32 vcsAccelerationNormalStub;

static u32 MakeSyscallStub(void *function) {
	SceUID block_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "", PSP_SMEM_High, 2 * sizeof(u32), NULL);
	u32 stub = (u32)sceKernelGetBlockHeadAddr(block_id);
	_sw(0x03E00008, stub);
	_sw(0x0000000C | (sceKernelQuerySystemCall(function) << 6), stub + 4);
	return stub;
}

short cameraX(short *pad) {
	return pad[PAD_RX];
}

short cameraY(short *pad) {
	return pad[PAD_RY];
}

short aimX(short *pad) {
	return pad[PAD_LX] ? pad[PAD_LX] : pad[PAD_RX];
}

short aimY(short *pad) {
	return pad[PAD_LY] ? pad[PAD_LY] : pad[PAD_RY];
}

short vcsAcceleration(short *pad) {
	if (pad[77] == 0)
		return pad[PAD_RTRIGGER];
	return 0;
}

short vcsAccelerationNormal(short *pad) {
	if (pad[77] == 0)
		return pad[PAD_CROSS];
	return 0;
}

short lcsAcceleration(short *pad) {
	if (pad[67] == 0)
		return pad[PAD_RTRIGGER];
	return 0;
}

int PatchVCS(u32 addr, u32 text_addr) {
	// Implement right analog stick
	if (_lw(addr) == 0x10000006 && _lw(addr + 0x4) == 0xA3A70003) {
		_sw(0x10000003, addr + 0x00);
		_sw(0x93A6000E, addr + 0x10); // lbu $a2, 14($sp)
		_sw(0x93A7000F, addr + 0x14); // lbu $a3, 15($sp)
		_sw(0x00000000, addr + 0x18); // nop
		_sw(0xA3A60000, addr + 0x1C); // sb $a2, 0($sp)
		_sw(0xA3A70001, addr + 0x20); // sb $a3, 1($sp)
		_sw(0x8E060054, addr + 0x24); // lw $a2, 84($s0)
		return 1;
	}

	// Redirect camera movement
	if (_lw(addr) == 0x14800036 && _lw(addr + 0x10) == 0x10400016) {
		_sw(0x00000000, addr + 0x00);
		_sw(0x10000016, addr + 0x10);
		MAKE_CALL(addr + 0x8C, MakeSyscallStub(cameraX));
		_sw(0x00000000, addr + 0x108);
		_sw(0x10000002, addr + 0x118);
		MAKE_CALL(addr + 0x144, MakeSyscallStub(cameraY));
		return 1;
	}

	// Redirect gun aim movement
	if (_lw(addr) == 0x04800040 && _lw(addr + 0x8) == 0x1080003E) {
		u32 aimXStub = MakeSyscallStub(aimX);
		u32 aimYStub = MakeSyscallStub(aimY);
		MAKE_CALL(addr + 0x50, aimXStub);
		MAKE_CALL(addr + 0x7C, aimXStub);
		MAKE_CALL(addr + 0x8C, aimXStub);
		MAKE_CALL(addr + 0x158, aimYStub);
		MAKE_CALL(addr + 0x1BC, aimYStub);
		return 1;
	}

	// Swap R trigger and cross button
	if (_lw(addr + 0x00) == 0x9485009A && _lw(addr + 0x04) == 0x0005282B &&
		_lw(addr + 0x08) == 0x30A500FF && _lw(addr + 0x0C) == 0x10A00003 &&
		_lw(addr + 0x10) == 0x00000000 && _lw(addr + 0x14) == 0x10000002 &&
		_lw(addr + 0x18) == 0x00001025 && _lw(addr + 0x1C) == 0x8482002A) {
		REDIRECT_FUNCTION(addr, MakeSyscallStub(vcsAcceleration));
		return 1;
	}

	// Use normal button for flying plane
	if (_lw(addr) == 0xC60E0780 && _lw(addr + 0x4) == 0x460D6302 && _lw(addr + 0x8) == 0x460D7342) {
		MAKE_CALL(addr + 0x1C, vcsAccelerationNormalStub);
		MAKE_CALL(addr + 0x3D0, vcsAccelerationNormalStub);
		return 1;
	}

	// Use normal button for flying helicoper
	if (_lw(addr) == 0x16400018 && _lw(addr + 0xC) == 0x02202025) {
		MAKE_CALL(addr + 0x14, vcsAccelerationNormalStub);
		return 1;
	}

	if (_lw(addr) == 0x1480000C && _lw(addr + 0x10) == 0x50A0000A) {
		_sh(PAD_RTRIGGER * 2, addr + 0x20);
		_sh(PAD_RTRIGGER * 2, addr + 0x68);
		_sh(PAD_CROSS * 2, addr + 0x80);
		return 1;
	}

	// Allow using L trigger when walking
	if (_lw(addr) == 0x1480000E && _lw(addr + 0x10) == 0x10800008 && _lw(addr + 0x1C) == 0x04800003) {
		_sw(0, addr + 0x10);
		_sw(0, addr + 0x9C);
		return 1;
	}

	// Force L trigger value in the L+camera movement function
	if (_lw(addr) == 0x84C7000A) {
		_sw(0x2407FFFF, addr);
		return 1;
	}

	return 0;
}

int PatchLCS(u32 addr, u32 text_addr) {
	// Implement right analog stick
	if (_lw(addr) == 0x10000006 && _lw(addr + 0x4) == 0xA3A70013) {
		_sw(0x10000003, addr + 0x00);
		_sw(0x93A6001E, addr + 0x10); // lbu $a2, 30($sp)
		_sw(0x93A7001F, addr + 0x14); // lbu $a3, 31($sp)
		_sw(0x00000000, addr + 0x18); // nop
		_sw(0xA3A60010, addr + 0x1C); // sb $a2, 16($sp)
		_sw(0xA3A70011, addr + 0x20); // sb $a3, 17($sp)
		_sw(0x8E060054, addr + 0x24); // lw $a2, 84($s0)
		return 1;
	}

	// Redirect camera movement
	if (_lw(addr) == 0x14800034 && _lw(addr + 0x10) == 0x10400014) {
		_sw(0x00000000, addr + 0x00);
		_sw(0x10000014, addr + 0x10);
		MAKE_CALL(addr + 0x84, MakeSyscallStub(cameraX));
		_sw(0x00000000, addr + 0x100);
		_sw(0x10000002, addr + 0x110);
		MAKE_CALL(addr + 0x13C, MakeSyscallStub(cameraY));
		return 1;
	}

	// Redirect gun aim movement
	if (_lw(addr) == 0x04800036 && _lw(addr + 0x8) == 0x10800034) {
		u32 aimXStub = MakeSyscallStub(aimX);
		u32 aimYStub = MakeSyscallStub(aimY);
		MAKE_CALL(addr + 0x3C, aimXStub);
		MAKE_CALL(addr + 0x68, aimXStub);
		MAKE_CALL(addr + 0x78, aimXStub);
		MAKE_CALL(addr + 0x130, aimYStub);
		MAKE_CALL(addr + 0x198, aimYStub);
		return 1;
	}

	// Swap R trigger and cross button
	if (_lw(addr + 0x00) == 0x94850086 && _lw(addr + 0x04) == 0x10A00003 &&
		_lw(addr + 0x08) == 0x00000000 && _lw(addr + 0x0C) == 0x10000002 &&
		_lw(addr + 0x10) == 0x00001025 && _lw(addr + 0x14) == 0x8482002A) {
		REDIRECT_FUNCTION(addr, MakeSyscallStub(lcsAcceleration));
		return 1;
	}

	if (_lw(addr) == 0x14A0000E && _lw(addr + 0x10) == 0x50C0000C) {
		_sh(PAD_RTRIGGER * 2, addr + 0x20);
		_sh(PAD_CROSS * 2, addr + 0x50);
		return 1;
	}

	// Allow using L trigger when walking
	if (_lw(addr) == 0x14A0000E && _lw(addr + 0x10) == 0x10A00008 && _lw(addr + 0x1C) == 0x04A00003) {
		_sw(0, addr + 0x10);
		_sw(0, addr + 0x74);
		return 1;
	}

	// Force L trigger value in the L+camera movement function
	if (_lw(addr) == 0x850A000A) {
		_sw(0x240AFFFF, addr);
		return 1;
	}

	return 0;
}

int OnModuleStart(SceModule2 *mod) {
	char *modname = mod->modname;
	u32 text_addr = mod->text_addr;

	int gta_version = -1;

	if (strcmp(modname, "GTA3") == 0) {
		vcsAccelerationNormalStub = MakeSyscallStub(vcsAccelerationNormal);

		u32 i;
		for (i = 0; i < mod->text_size; i += 4) {
			u32 addr = text_addr + i;

			if ((gta_version == -1 || gta_version == 0) && PatchVCS(addr, text_addr)) {
				gta_version = 0;
				continue;
			}

			if ((gta_version == -1 || gta_version == 1) && PatchLCS(addr, text_addr)) {
				gta_version = 1;
				continue;
			}
		}

		sceKernelDcacheWritebackAll();
		sceKernelIcacheClearAll();
	}

	if (!previous)
		return 0;

	return previous(mod);
}

int module_start(SceSize args, void *argp) {
	previous = sctrlHENSetStartModuleHandler(OnModuleStart);
	return 0;
}