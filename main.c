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

PSP_MODULE_INFO("gta_patch", 0x0007, 1, 0);

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

#define REDIRECT_FUNCTION(a, f) \
{ \
	u32 func = a; \
	_sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), func); \
	_sw(0, func + 4); \
}

#define MAX_VALUE 0xFF
#define CENTER 0x80
#define DEADZONE 0x20
#define FACTOR 1.2f

STMOD_HANDLER previous;

int cameraX() {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);

	char rx = pad.Rsrv[0] - CENTER;
	if (rx < -DEADZONE || rx > DEADZONE)
		return (int)((float)rx * FACTOR);

	return 0;
}

int cameraY() {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);

	char ry = MAX_VALUE - pad.Rsrv[1] - CENTER;
	if (ry < -DEADZONE || ry > DEADZONE)
		return (int)((float)ry * FACTOR);

	return 0;
}

int aimX() {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);

	char lx = pad.Lx - CENTER;
	if (lx < -DEADZONE || lx > DEADZONE)
		return (int)((float)lx * FACTOR);

	char rx = pad.Rsrv[0] - CENTER;
	if (rx < -DEADZONE || rx > DEADZONE)
		return (int)((float)rx * FACTOR);

	return 0;
}

int aimY() {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);

	char ly = MAX_VALUE - pad.Ly - CENTER;
	if (ly < -DEADZONE || ly > DEADZONE)
		return (int)((float)ly * FACTOR);

	char ry = MAX_VALUE - pad.Rsrv[1] - CENTER;
	if (ry < -DEADZONE || ry > DEADZONE)
		return (int)((float)ry * FACTOR);

	return 0;
}

int crossPressedLCS(void *a0) {
	if (((u16 *)a0)[67] == 0) {
		return ((u16 *)a0)[7];
	}

	return 0;
}

int crossPressedVCS(void *a0) {
	if (((u16 *)a0)[77] == 0) {
		return ((u16 *)a0)[7];
	}

	return 0;
}

int OnModuleStart(SceModule2 *mod) {
	char *modname = mod->modname;
	u32 text_addr = mod->text_addr;

	if (strcmp(modname, "GTA3") == 0) {
		u32 i;
		for (i = 0; i < mod->text_size; i += 4) {
			u32 addr = text_addr + i;

			// Redirect camera movement
			if (_lw(addr) == 0x14800036 && _lw(addr + 0x10) == 0x10400016) {
				// log("VCS CAMERA: 0x%08X, 0x%08X\n", addr - 0x18 - text_addr, addr - 0x18 + 0x108 - text_addr);
				REDIRECT_FUNCTION(addr - 0x18, cameraX);
				REDIRECT_FUNCTION(addr - 0x18 + 0x108, cameraY);
				continue;
			}

			// Redirect gun aim movement x
			if (_lw(addr) == 0x04800040 && _lw(addr + 0x8) == 0x1080003E) {
				// log("VCS AIM X: 0x%08X\n", addr - 0x14 - text_addr);
				REDIRECT_FUNCTION(addr - 0x14, aimX);
				continue;
			}

			// Redirect gun aim movement y
			if (_lw(addr) == 0x0480003F && _lw(addr + 0x8) == 0x1080003D) {
				// log("VCS AIM Y: 0x%08X\n", addr - 0x14 - text_addr);
				REDIRECT_FUNCTION(addr - 0x14, aimY);
				continue;
			}

			// Swap R trigger and cross button
			if (_lw(addr + 0x00) == 0x9485009A && _lw(addr + 0x04) == 0x0005282B &&
				_lw(addr + 0x08) == 0x30A500FF && _lw(addr + 0x0C) == 0x10A00003 &&
				_lw(addr + 0x10) == 0x00000000 && _lw(addr + 0x14) == 0x10000002 &&
				_lw(addr + 0x18) == 0x00001025 && _lw(addr + 0x1C) == 0x8482002A) {
				// log("VCS CROSS: 0x%08X\n", addr - text_addr);
				REDIRECT_FUNCTION(addr, crossPressedVCS);
				continue;
			}

			if (_lw(addr) == 0x1480000C && _lw(addr + 0x10) == 0x50A0000A) {
				// log("VCS CROSS/R: 0x%08X, 0x%08X\n", addr + 0x20 - text_addr, addr + 0x50 - text_addr);
				_sh(14, addr + 0x20);
				_sh(14, addr + 0x68);
				_sh(42, addr + 0x80);
				continue;
			}

			// Allow using L trigger when walking
			if (_lw(addr) == 0x1480000E && _lw(addr + 0x10) == 0x10800008 && _lw(addr + 0x1C) == 0x04800003) {
				// log("VCS WALK: 0x%08X, 0x%08X\n", addr + 0x10 - text_addr, addr + 0x74 - text_addr);
				_sw(0, addr + 0x10);
				_sw(0, addr + 0x9C);
				continue;
			}

			// Force L trigger value in the L+camera movement function
			if (_lw(addr) == 0x84C7000A) {
				// log("VCS L+CAMERA: 0x%08X\n", addr - text_addr);
				_sw(0x2407FFFF, addr);
				continue;
			}

			///////////////////////////////////////////////////////////////////////////////////////////////

			// Redirect camera movement
			if (_lw(addr) == 0x14800034 && _lw(addr + 0x10) == 0x10400014) {
				// log("LCS CAMERA: 0x%08X, 0x%08X\n", addr - 0x18 - text_addr, addr - 0x18 + 0x100 - text_addr);
				REDIRECT_FUNCTION(addr - 0x18, cameraX);
				REDIRECT_FUNCTION(addr - 0x18 + 0x100, cameraY);
				continue;
			}

			// Redirect gun aim movement x
			if (_lw(addr) == 0x04800036 && _lw(addr + 0x8) == 0x10800034) {
				// log("LCS AIM X: 0x%08X\n", addr - 0x14 - text_addr)
				REDIRECT_FUNCTION(addr - 0x14, aimX);
				continue;
			}

			// Redirect gun aim movement y
			if (_lw(addr) == 0x04800041 && _lw(addr + 0x8) == 0x1080003F) {
				// log("LCS AIM Y: 0x%08X\n", addr - 0x14 - text_addr)
				REDIRECT_FUNCTION(addr - 0x14, aimY);
				continue;
			}

			// Swap R trigger and cross button
			if (_lw(addr + 0x00) == 0x94850086 && _lw(addr + 0x04) == 0x10A00003 &&
				_lw(addr + 0x08) == 0x00000000 && _lw(addr + 0x0C) == 0x10000002 &&
				_lw(addr + 0x10) == 0x00001025 && _lw(addr + 0x14) == 0x8482002A) {
				// log("LCS CROSS: 0x%08X\n", addr - text_addr);
				REDIRECT_FUNCTION(addr, crossPressedLCS);
				continue;
			}

			if (_lw(addr) == 0x14A0000E && _lw(addr + 0x10) == 0x50C0000C) {
				// log("LCS CROSS/R: 0x%08X, 0x%08X\n", addr + 0x20 - text_addr, addr + 0x50 - text_addr);
				_sh(14, addr + 0x20);
				_sh(42, addr + 0x50);
				continue;
			}

			// Allow using L trigger when walking
			if (_lw(addr) == 0x14A0000E && _lw(addr + 0x10) == 0x10A00008 && _lw(addr + 0x1C) == 0x04A00003) {
				// log("LCS WALK: 0x%08X, 0x%08X\n", addr + 0x10 - text_addr, addr + 0x74 - text_addr);
				_sw(0, addr + 0x10);
				_sw(0, addr + 0x74);
				continue;
			}

			// Force L trigger value in the L+camera movement function
			if (_lw(addr) == 0x850A000A) {
				// log("LCS L+CAMERA: 0x%08X\n", addr - text_addr);
				_sw(0x240AFFFF, addr);
				continue;
			}
		}

		sceKernelDcacheWritebackAll();
	}

	if (!previous)
		return 0;

	return previous(mod);
}

int module_start(SceSize args, void *argp) {
	previous = sctrlHENSetStartModuleHandler(OnModuleStart);
	return 0;
}