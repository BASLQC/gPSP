/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"

// Special thanks to psp298 for the analog->dpad code!

void trigger_key(u32 key)
{
  u32 p1_cnt = io_registers[REG_P1CNT];
  u32 p1;

  if((p1_cnt >> 14) & 0x01)
  {
    u32 key_intersection = (p1_cnt & key) & 0x3FF;

    if(p1_cnt >> 15)
    {
      if(key_intersection == (p1_cnt & 0x3FF))
        raise_interrupt(IRQ_KEYPAD);
    }
    else
    {
      if(key_intersection)
        raise_interrupt(IRQ_KEYPAD);
    }
  }
}

u32 key = 0;

u32 gamepad_config_map[16] =
{
  BUTTON_ID_MENU,
  BUTTON_ID_A,
  BUTTON_ID_B,
  BUTTON_ID_START,
  BUTTON_ID_L,
  BUTTON_ID_R,
  BUTTON_ID_DOWN,
  BUTTON_ID_LEFT,
  BUTTON_ID_UP,
  BUTTON_ID_RIGHT,
  BUTTON_ID_SELECT,
  BUTTON_ID_START,
  BUTTON_ID_UP,
  BUTTON_ID_DOWN,
  BUTTON_ID_LEFT,
  BUTTON_ID_RIGHT
};

u32 global_enable_analog = 1;
u32 analog_sensitivity_level = 4;

#ifdef PSP_BUILD

#define PSP_ALL_BUTTON_MASK 0xFFFF

u32 last_buttons = 0;
u64 button_repeat_timestamp;

typedef enum
{
  BUTTON_NOT_HELD,
  BUTTON_HELD_INITIAL,
  BUTTON_HELD_REPEAT
} button_repeat_state_type;

button_repeat_state_type button_repeat_state = BUTTON_NOT_HELD;
u32 button_repeat = 0;
gui_action_type cursor_repeat = CURSOR_NONE;

#define BUTTON_REPEAT_START    200000
#define BUTTON_REPEAT_CONTINUE 50000

gui_action_type get_gui_input()
{
  SceCtrlData ctrl_data;
  gui_action_type new_button = CURSOR_NONE;
  u32 new_buttons;

  delay_us(25000);

  sceCtrlPeekBufferPositive(&ctrl_data, 1);
  ctrl_data.Buttons &= PSP_ALL_BUTTON_MASK;
  new_buttons = (last_buttons ^ ctrl_data.Buttons) & ctrl_data.Buttons;
  last_buttons = ctrl_data.Buttons;

  if(new_buttons & PSP_CTRL_LEFT)
    new_button = CURSOR_LEFT;

  if(new_buttons & PSP_CTRL_RIGHT)
    new_button = CURSOR_RIGHT;

  if(new_buttons & PSP_CTRL_UP)
    new_button = CURSOR_UP;

  if(new_buttons & PSP_CTRL_DOWN)
    new_button = CURSOR_DOWN;

  if(new_buttons & PSP_CTRL_START)
    new_button = CURSOR_SELECT;

  if(new_buttons & PSP_CTRL_CIRCLE)
    new_button = CURSOR_SELECT;

  if(new_buttons & PSP_CTRL_CROSS)
    new_button = CURSOR_EXIT;

  if(new_buttons & PSP_CTRL_SQUARE)
    new_button = CURSOR_BACK;

  if(new_button != CURSOR_NONE)
  {
    get_ticks_us(&button_repeat_timestamp);
    button_repeat_state = BUTTON_HELD_INITIAL;
    button_repeat = new_buttons;
    cursor_repeat = new_button;
  }
  else
  {
    if(ctrl_data.Buttons & button_repeat)
    {
      u64 new_ticks;
      get_ticks_us(&new_ticks);

      if(button_repeat_state == BUTTON_HELD_INITIAL)
      {
        if((new_ticks - button_repeat_timestamp) >
         BUTTON_REPEAT_START)
        {
          new_button = cursor_repeat;
          button_repeat_timestamp = new_ticks;
          button_repeat_state = BUTTON_HELD_REPEAT;
        }
      }

      if(button_repeat_state == BUTTON_HELD_REPEAT)
      {
        if((new_ticks - button_repeat_timestamp) >
         BUTTON_REPEAT_CONTINUE)
        {
          new_button = cursor_repeat;
          button_repeat_timestamp = new_ticks;
        }
      }
    }
  }

  return new_button;
}

#define PSP_CTRL_ANALOG_UP    (1 << 28)
#define PSP_CTRL_ANALOG_DOWN  (1 << 29)
#define PSP_CTRL_ANALOG_LEFT  (1 << 30)
#define PSP_CTRL_ANALOG_RIGHT (1 << 31)

u32 button_psp_mask_to_config[] =
{
  PSP_CTRL_TRIANGLE,
  PSP_CTRL_CIRCLE,
  PSP_CTRL_CROSS,
  PSP_CTRL_SQUARE,
  PSP_CTRL_LTRIGGER,
  PSP_CTRL_RTRIGGER,
  PSP_CTRL_DOWN,
  PSP_CTRL_LEFT,
  PSP_CTRL_UP,
  PSP_CTRL_RIGHT,
  PSP_CTRL_SELECT,
  PSP_CTRL_START,
  PSP_CTRL_ANALOG_UP,
  PSP_CTRL_ANALOG_DOWN,
  PSP_CTRL_ANALOG_LEFT,
  PSP_CTRL_ANALOG_RIGHT
};

u32 button_id_to_gba_mask[] =
{
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_A,
  BUTTON_B,
  BUTTON_L,
  BUTTON_R,
  BUTTON_START,
  BUTTON_SELECT,
  BUTTON_NONE,
  BUTTON_NONE,
  BUTTON_NONE,
  BUTTON_NONE
};

gui_action_type get_gui_input_fs_hold(u32 button_id)
{
  gui_action_type new_button = get_gui_input();
  if((last_buttons & button_psp_mask_to_config[button_id]) == 0)
    return CURSOR_BACK;

  return new_button;
}

u32 rapidfire_flag = 1;

u32 update_input()
{
  SceCtrlData ctrl_data;
  u32 buttons;
  u32 non_repeat_buttons;
  u32 button_id;
  u32 i;
  u32 new_key = 0;
  u32 analog_sensitivity = 92 - (analog_sensitivity_level * 4);
  u32 inv_analog_sensitivity = 256 - analog_sensitivity;

  sceCtrlPeekBufferPositive(&ctrl_data, 1);

  buttons = ctrl_data.Buttons;

  if(global_enable_analog)
  {
    if(ctrl_data.Lx < analog_sensitivity)
      buttons |= PSP_CTRL_ANALOG_LEFT;

    if(ctrl_data.Lx > inv_analog_sensitivity)
      buttons |= PSP_CTRL_ANALOG_RIGHT;

    if(ctrl_data.Ly < analog_sensitivity)
      buttons |= PSP_CTRL_ANALOG_UP;

    if(ctrl_data.Ly > inv_analog_sensitivity)
      buttons |= PSP_CTRL_ANALOG_DOWN;
  }

  non_repeat_buttons = (last_buttons ^ buttons) & buttons;
  last_buttons = buttons;

  for(i = 0; i < 16; i++)
  {
    if(non_repeat_buttons & button_psp_mask_to_config[i])
      button_id = gamepad_config_map[i];

    switch(button_id)
    {
      case BUTTON_ID_MENU:
      {
        u16 *screen_copy = copy_screen();
        u32 ret_val = menu(screen_copy);
        free(screen_copy);

        return ret_val;
      }

      case BUTTON_ID_LOADSTATE:
      {
        u8 current_savestate_filename[512];
        get_savestate_filename_noshot(savestate_slot,
         current_savestate_filename);
        load_state(current_savestate_filename);
        return 1;
      }

      case BUTTON_ID_SAVESTATE:
      {
        u8 current_savestate_filename[512];
        u16 *current_screen = copy_screen();
        get_savestate_filename_noshot(savestate_slot,
         current_savestate_filename);
        save_state(current_savestate_filename, current_screen);
        free(current_screen);
        return 0;
      }

      case BUTTON_ID_FASTFORWARD:
        synchronize_flag ^= 1;
        return 0;
    }

    if(buttons & button_psp_mask_to_config[i])
    {
      button_id = gamepad_config_map[i];
      if(button_id < BUTTON_ID_MENU)
      {
        new_key |= button_id_to_gba_mask[button_id];
      }
      else

      if((button_id >= BUTTON_ID_RAPIDFIRE_A) &&
       (button_id <= BUTTON_ID_RAPIDFIRE_L))
      {
        rapidfire_flag ^= 1;
        if(rapidfire_flag)
        {
          new_key |= button_id_to_gba_mask[button_id -
           BUTTON_ID_RAPIDFIRE_A + BUTTON_ID_A];
        }
        else
        {
          new_key &= ~button_id_to_gba_mask[button_id -
           BUTTON_ID_RAPIDFIRE_A + BUTTON_ID_A];
        }
      }
    }
  }

  if((new_key | key) != key)
    trigger_key(new_key);

  key = new_key;

  io_registers[REG_P1] = (~key) & 0x3FF;

  return 0;
}

void init_input()
{
  sceCtrlSetSamplingCycle(0);
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

#else

u32 key_map(SDLKey key_sym)
{
  switch(key_sym)
  {
    case SDLK_LSHIFT:
      return BUTTON_L;

    case SDLK_x:
      return BUTTON_R;

    case SDLK_DOWN:
      return BUTTON_DOWN;

    case SDLK_UP:
      return BUTTON_UP;

    case SDLK_LEFT:
      return BUTTON_LEFT;

    case SDLK_RIGHT:
      return BUTTON_RIGHT;

    case SDLK_RETURN:
      return BUTTON_START;

    case SDLK_RSHIFT:
      return BUTTON_SELECT;

    case SDLK_LCTRL:
      return BUTTON_B;

    case SDLK_LALT:
      return BUTTON_A;

    default:
      return BUTTON_NONE;
  }
}

u32 joy_map(u32 button)
{
  switch(button)
  {
    case 4:
      return BUTTON_L;

    case 5:
      return BUTTON_R;

    case 9:
      return BUTTON_START;

    case 8:
      return BUTTON_SELECT;

    case 0:
      return BUTTON_B;

    case 1:
      return BUTTON_A;

    default:
      return BUTTON_NONE;
  }
}


u32 joy_hat_map(u32 value)
{
  u32 result = BUTTON_NONE;

  if(value & SDL_HAT_UP)
    result |= BUTTON_UP;

  if(value & SDL_HAT_RIGHT)
    result |= BUTTON_RIGHT;

  if(value & SDL_HAT_DOWN)
    result |= BUTTON_DOWN;

  if(value & SDL_HAT_LEFT)
    result |= BUTTON_LEFT;

  return result;
}

gui_action_type get_gui_input()
{
  SDL_Event event;
  gui_action_type gui_action = CURSOR_NONE;

  delay_us(30000);

  while(SDL_PollEvent(&event))
  {
    switch(event.type)
    {
      case SDL_QUIT:
        quit();

      case SDL_KEYDOWN:
      {
        switch(event.key.keysym.sym)
        {
          case SDLK_ESCAPE:
            gui_action = CURSOR_EXIT;
            break;

          case SDLK_DOWN:
            gui_action = CURSOR_DOWN;
            break;

          case SDLK_UP:
            gui_action = CURSOR_UP;
            break;

          case SDLK_LEFT:
            gui_action = CURSOR_LEFT;
            break;

          case SDLK_RIGHT:
            gui_action = CURSOR_RIGHT;
            break;

          case SDLK_RETURN:
            gui_action = CURSOR_SELECT;
            break;

          case SDLK_BACKSPACE:
            gui_action = CURSOR_BACK;
            break;
        }
        break;
      }
    }
  }

  return gui_action;
}

// FIXME: Not implemented properly for x86 version.

gui_action_type get_gui_input_fs_hold(u32 button_id)
{
  return get_gui_input();
}

u32 update_input()
{
  SDL_Event event;

  while(SDL_PollEvent(&event))
  {
    switch(event.type)
    {
      case SDL_QUIT:
        quit();

      case SDL_KEYDOWN:
      {
        if(event.key.keysym.sym == SDLK_ESCAPE)
        {
          quit();
        }

        if(event.key.keysym.sym == SDLK_BACKSPACE)
        {
          u16 *screen_copy = copy_screen();
          u32 ret_val = menu(screen_copy);
          free(screen_copy);

          return ret_val;
//          return adjust_frameskip(0);
        }
        else

        if(event.key.keysym.sym == SDLK_F1)
        {
          current_debug_state = STEP;
        }
        else

        if(event.key.keysym.sym == SDLK_F2)
        {
          FILE *fp = fopen("palette_ram.bin", "wb");
          printf("writing palette RAM\n");
          fwrite(palette_ram, 1024, 1, fp);
          fclose(fp);
          printf("writing palette VRAM\n");
          fp = fopen("vram.bin", "wb");
          fwrite(vram, 1024 * 96, 1, fp);
          fclose(fp);
          printf("writing palette OAM RAM\n");
          fp = fopen("oam_ram.bin", "wb");
          fwrite(oam_ram, 1024, 1, fp);
          fclose(fp);
          printf("writing palette I/O registers\n");
          fp = fopen("io_registers.bin", "wb");
          fwrite(io_registers, 1024, 1, fp);
          fclose(fp);
        }
        else

        if(event.key.keysym.sym == SDLK_F3)
        {
          dump_translation_cache();
        }
        else

        if(event.key.keysym.sym == SDLK_F5)
        {
          u8 current_savestate_filename[512];
          u16 *current_screen = copy_screen();
          get_savestate_filename_noshot(savestate_slot,
           current_savestate_filename);
          save_state(current_savestate_filename, current_screen);
          free(current_screen);
        }
        else

        if(event.key.keysym.sym == SDLK_F7)
        {
          u8 current_savestate_filename[512];
          get_savestate_filename_noshot(savestate_slot,
           current_savestate_filename);
          load_state(current_savestate_filename);
          current_debug_state = STEP;
          return 1;
        }
        else

        if(event.key.keysym.sym == SDLK_BACKQUOTE)
        {
          synchronize_flag ^= 1;
        }
        else
        {
          key |= key_map(event.key.keysym.sym);
          trigger_key(key);
        }

        break;
      }

      case SDL_KEYUP:
      {
        key &= ~(key_map(event.key.keysym.sym));
        break;
      }

      case SDL_JOYAXISMOTION:
      {
/*        key = (key & 0xFF0F) | joy_axis_map(event.jaxis.value,
         event.jaxis.axis);
        trigger_key(key); */
        break;
      }

      case SDL_JOYHATMOTION:
      {
        key = (key & 0xFF0F) | joy_hat_map(event.jhat.value);
        trigger_key(key);
        break;
      }

      case SDL_JOYBUTTONDOWN:
      {
        key |= joy_map(event.jbutton.button);
        trigger_key(key);
        break;
      }

      case SDL_JOYBUTTONUP:
      {
        key &= ~(joy_map(event.jbutton.button));
        break;
      }
    }
  }

  io_registers[REG_P1] = (~key) & 0x3FF;

  return 0;
}

void init_input()
{
  u32 joystick_count = SDL_NumJoysticks();

  if(joystick_count > 0)
  {
    SDL_JoystickOpen(0);
    SDL_JoystickEventState(SDL_ENABLE);
  }
}

#endif

#define input_savestate_builder(type)                                         \
void input_##type##_savestate(file_tag_type savestate_file)                   \
{                                                                             \
  file_##type##_variable(savestate_file, key);                                \
}                                                                             \

input_savestate_builder(read);
input_savestate_builder(write_mem);

