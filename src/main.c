#include <genesis.h>
#include <kdebug.h>

#include "dungeon_map.h"
#include "d3d_renderer.h"

#include "scenes.h"


u16 joy;
u16 ind;

const u16 palette_data[] = {
	0x000, 0x4E4, 0xAEA, 0x4A4, 0xEA0, 0xE40, 0x440, 0xEE0, 
	0xA00, 0x000, 0xE0E, 0xE0E, 0xE0E, 0xE0E, 0xE0E, 0xE0E
};

const u8 test_dungeon[] = {
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 0, 0, 0, 0, 1,
	1, 1, 0, 1, 0, 1, 0, 1,
	1, 1, 0, 0, 0, 0, 0, 1, 
	1, 1, 1, 1, 1, 1, 0, 1,
	1, 1, 1, 1, 1, 1, 0, 1,
	1, 1, 1, 1, 1, 1, 0, 1,
	1, 1, 1, 1, 1, 1, 1, 1
};

DungeonMap current_map;
D3DRenderer renderer;

int main(bool hard) {

	ind = TILE_USER_INDEX;	
		
    // initialization
    VDP_setScreenWidth320();
	VDP_setScreenHeight224();

    PAL_setColors(0, palette_data, 16, CPU);

    // just to monitor frame CPU usage
    SYS_showFrameLoad(TRUE);

	// initialize
	DM_init(&current_map, 8, 8, test_dungeon);
	D3D_init(&renderer, &current_map, &scenes_data, 6, 6, 1);

	if (TRUE) {

		D3D_build_front_scene(&renderer, FALSE);
		D3D_next_frame(&renderer);
		
		while (TRUE) {
			joy = JOY_readJoypad(JOY_1);

			if (joy & BUTTON_UP) {
				if (!DM_get_at_towards(&current_map, &(renderer.pos), &forward)) {
					D3D_move_forward(&renderer);
					continue;
				}
			}

			else if (joy & BUTTON_DOWN) {
				if (!DM_get_at_towards(&current_map, &(renderer.pos), &backward)) {
					D3D_move_backward(&renderer);
					continue;
				}
			}

			else if (joy & BUTTON_LEFT) {
				D3D_turn_left(&renderer);
				continue;
			}

			else if (joy & BUTTON_RIGHT) {
				D3D_turn_right(&renderer);
				continue;
			}

			D3D_doVBlankProcess();
		}
	}
}
