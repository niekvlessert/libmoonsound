#include "mwm_parser.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define STEP_BUFFER_SIZE 25
#define COMMAND_CHANNEL 24
#define MAX_STEP 15

#define NOTE_ON 96
#define NOTE_OFF 97
#define WAVE_CHANGE 98
#define VOLUME_CHANGE 146
#define STEREO_SET 178
#define NOTE_LINK 193
#define PITCH_BEND 212
#define DETUNE 231
#define MODULATION 238
#define REVERB_ON 241
#define DAMP 242
#define LFO 243
#define REVERB_OFF 245
#define XTRA_LFO 246
#define NO_EVENT 255

#define COMMAND_TEMPO 23
#define COMMAND_PATTERN_END 24
#define COMMAND_TRANSPOSE 28
#define COMMAND_FREQUENCY 77

#define FREQUENCY_MODE_NORMAL 0
#define FREQUENCY_MODE_PITCH_BEND 1
#define FREQUENCY_MODE_MODULATION 2

#define REVERB_ENABLED 0x08
#define REVERB_DISABLED 0x00

#pragma pack(push, 1)
typedef struct {
  uint8_t next_patch_note;
  uint8_t tone_low;
  uint8_t tone_high;
  uint16_t freq_ptr;
} MSX_PATCH_PART;

typedef struct {
  uint8_t transpose;
  uint16_t hdr_ptr;
  MSX_PATCH_PART parts[1];
} MSX_PATCH;

typedef struct {
  uint8_t tone;
  uint16_t freq;
  uint16_t hdr_ptr;
} MSX_GM_DRUM_PATCH;
#pragma pack(pop)

typedef struct {
  uint8_t last_note;
  uint8_t frequency_mode;
  int16_t pitch_bend_speed;
  int8_t modulation_index;
  uint8_t modulation_count;
  int8_t detune_value;
  uint8_t next_tone_low;
  uint8_t next_tone_high;
  uint16_t next_frequency;
  uint8_t current_wave;
  uint8_t current_stereo;
  uint8_t pseudo_reverb;
  uint16_t patch_header_ptr;
  uint8_t volume;
  uint16_t freq_table_ptr;
  uint16_t pitch_frequency;
  uint16_t *freq_table_data;
  bool freq_table_is_full;
} RefStatus;

typedef struct {
  MwmSong song;
  MwkKit kit;
  uint8_t *waves;
  uint16_t waves_ptrs[179];
  uint8_t wave_regs[256];
  RefStatus st[NR_WAVE_CHANNELS];
  uint8_t step_buffer[STEP_BUFFER_SIZE];
  uint8_t *pattern_data;
  int8_t transpose_value;
  uint8_t speed;
  uint8_t speed_count;
  uint8_t base_frequency;
  uint8_t position;
  uint8_t step;
  FILE *trace_fp;
  uint64_t tick;
  uint64_t seq;
} RefCtx;

static const uint8_t g_tabdiv12[192][2] = {
    {0xB0, 0x00}, {0xB0, 0x02}, {0xB0, 0x04}, {0xB0, 0x06}, {0xB0, 0x08},
    {0xB0, 0x0A}, {0xB0, 0x0C}, {0xB0, 0x0E}, {0xB0, 0x10}, {0xB0, 0x12},
    {0xB0, 0x14}, {0xB0, 0x16}, {0xC0, 0x00}, {0xC0, 0x02}, {0xC0, 0x04},
    {0xC0, 0x06}, {0xC0, 0x08}, {0xC0, 0x0A}, {0xC0, 0x0C}, {0xC0, 0x0E},
    {0xC0, 0x10}, {0xC0, 0x12}, {0xC0, 0x14}, {0xC0, 0x16}, {0xD0, 0x00},
    {0xD0, 0x02}, {0xD0, 0x04}, {0xD0, 0x06}, {0xD0, 0x08}, {0xD0, 0x0A},
    {0xD0, 0x0C}, {0xD0, 0x0E}, {0xD0, 0x10}, {0xD0, 0x12}, {0xD0, 0x14},
    {0xD0, 0x16}, {0xE0, 0x00}, {0xE0, 0x02}, {0xE0, 0x04}, {0xE0, 0x06},
    {0xE0, 0x08}, {0xE0, 0x0A}, {0xE0, 0x0C}, {0xE0, 0x0E}, {0xE0, 0x10},
    {0xE0, 0x12}, {0xE0, 0x14}, {0xE0, 0x16}, {0xF0, 0x00}, {0xF0, 0x02},
    {0xF0, 0x04}, {0xF0, 0x06}, {0xF0, 0x08}, {0xF0, 0x0A}, {0xF0, 0x0C},
    {0xF0, 0x0E}, {0xF0, 0x10}, {0xF0, 0x12}, {0xF0, 0x14}, {0xF0, 0x16},
    {0x00, 0x00}, {0x00, 0x02}, {0x00, 0x04}, {0x00, 0x06}, {0x00, 0x08},
    {0x00, 0x0A}, {0x00, 0x0C}, {0x00, 0x0E}, {0x00, 0x10}, {0x00, 0x12},
    {0x00, 0x14}, {0x00, 0x16}, {0x10, 0x00}, {0x10, 0x02}, {0x10, 0x04},
    {0x10, 0x06}, {0x10, 0x08}, {0x10, 0x0A}, {0x10, 0x0C}, {0x10, 0x0E},
    {0x10, 0x10}, {0x10, 0x12}, {0x10, 0x14}, {0x10, 0x16}, {0x20, 0x00},
    {0x20, 0x02}, {0x20, 0x04}, {0x20, 0x06}, {0x20, 0x08}, {0x20, 0x0A},
    {0x20, 0x0C}, {0x20, 0x0E}, {0x20, 0x10}, {0x20, 0x12}, {0x20, 0x14},
    {0x20, 0x16}, {0x30, 0x00}, {0x30, 0x02}, {0x30, 0x04}, {0x30, 0x06},
    {0x30, 0x08}, {0x30, 0x0A}, {0x30, 0x0C}, {0x30, 0x0E}, {0x30, 0x10},
    {0x30, 0x12}, {0x30, 0x14}, {0x30, 0x16}, {0x40, 0x00}, {0x40, 0x02},
    {0x40, 0x04}, {0x40, 0x06}, {0x40, 0x08}, {0x40, 0x0A}, {0x40, 0x0C},
    {0x40, 0x0E}, {0x40, 0x10}, {0x40, 0x12}, {0x40, 0x14}, {0x40, 0x16},
    {0x50, 0x00}, {0x50, 0x02}, {0x50, 0x04}, {0x50, 0x06}, {0x50, 0x08},
    {0x50, 0x0A}, {0x50, 0x0C}, {0x50, 0x0E}, {0x50, 0x10}, {0x50, 0x12},
    {0x50, 0x14}, {0x50, 0x16}, {0x60, 0x00}, {0x60, 0x02}, {0x60, 0x04},
    {0x60, 0x06}, {0x60, 0x08}, {0x60, 0x0A}, {0x60, 0x0C}, {0x60, 0x0E},
    {0x60, 0x10}, {0x60, 0x12}, {0x60, 0x14}, {0x60, 0x16}, {0x70, 0x00},
    {0x70, 0x02}, {0x70, 0x04}, {0x70, 0x06}, {0x70, 0x08}, {0x70, 0x0A},
    {0x70, 0x0C}, {0x70, 0x0E}, {0x70, 0x10}, {0x70, 0x12}, {0x70, 0x14},
    {0x70, 0x16}, {0x80, 0x00}, {0x80, 0x02}, {0x80, 0x04}, {0x80, 0x06},
    {0x80, 0x08}, {0x80, 0x0A}, {0x80, 0x0C}, {0x80, 0x0E}, {0x80, 0x10},
    {0x80, 0x12}, {0x80, 0x14}, {0x80, 0x16}, {0x90, 0x00}, {0x90, 0x02},
    {0x90, 0x04}, {0x90, 0x06}, {0x90, 0x08}, {0x90, 0x0A}, {0x90, 0x0C},
    {0x90, 0x0E}, {0x90, 0x10}, {0x90, 0x12}, {0x90, 0x14}, {0x90, 0x16},
    {0xA0, 0x00}, {0xA0, 0x02}, {0xA0, 0x04}, {0xA0, 0x06}, {0xA0, 0x08},
    {0xA0, 0x0A}, {0xA0, 0x0C}, {0xA0, 0x0E}, {0xA0, 0x10}, {0xA0, 0x12},
    {0xA0, 0x14}, {0xA0, 0x16}};

static const uint8_t g_xls_table[][2] = {
    {49, 0}, {50, 0}, {51, 0}, {52, 0}, {53, 0},
    {54, 0}, {55, 0}, {58, 5}, {3, 0}};

static void trace_line(RefCtx *c, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(c->trace_fp, fmt, ap);
  va_end(ap);
  fputc('\n', c->trace_fp);
}

static void write_wave(RefCtx *c, uint8_t reg, uint8_t val) {
  c->wave_regs[reg] = val;
  trace_line(c, "REG,%" PRIu64 ",%" PRIu64 ",%02X,%02X", c->tick, c->seq++, reg,
             val);
}

static uint8_t read_wave(RefCtx *c, uint8_t reg) { return c->wave_regs[reg]; }

static int ascii_casecmp(const char *a, const char *b) {
  unsigned char ca, cb;
  if (!a || !b)
    return (a == b) ? 0 : 1;
  while (*a && *b) {
    ca = (unsigned char)toupper((unsigned char)*a);
    cb = (unsigned char)toupper((unsigned char)*b);
    if (ca != cb)
      return (int)ca - (int)cb;
    a++;
    b++;
  }
  ca = (unsigned char)toupper((unsigned char)*a);
  cb = (unsigned char)toupper((unsigned char)*b);
  return (int)ca - (int)cb;
}

static bool is_none_kit_name(const char *name) {
  return name && name[0] && ascii_casecmp(name, "NONE") == 0;
}

static void copy_trimmed_field(char *dst, size_t dst_size, const char *src,
                               size_t src_size) {
  size_t len = src_size;
  dst[0] = '\0';
  while (len > 0 && (((unsigned char)src[len - 1]) == 0 || src[len - 1] == ' '))
    len--;
  if (len >= dst_size)
    len = dst_size - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
}

static bool make_same_name_mwk_path(const char *mwm_path, char *out,
                                    size_t out_size) {
  size_t len = strlen(mwm_path);
  if (len + 5 >= out_size)
    return false;
  strcpy(out, mwm_path);
  char *slash = strrchr(out, '/');
  char *dot = strrchr(out, '.');
  if (dot && (!slash || dot > slash))
    strcpy(dot, ".MWK");
  else
    strcat(out, ".MWK");
  return true;
}

static bool make_header_name_mwk_path(const char *mwm_path, const char *kit_name,
                                      char *out, size_t out_size) {
  const char *slash = strrchr(mwm_path, '/');
  if (!slash)
    return snprintf(out, out_size, "%s.MWK", kit_name) < (int)out_size;
  size_t dir_len = (size_t)(slash - mwm_path + 1);
  if (dir_len + strlen(kit_name) + 5 > out_size)
    return false;
  memcpy(out, mwm_path, dir_len);
  out[dir_len] = '\0';
  strcat(out, kit_name);
  strcat(out, ".MWK");
  return true;
}

static bool load_waves_dat(RefCtx *c, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  c->waves = (uint8_t *)malloc(128 * 1024);
  if (!c->waves) {
    fclose(f);
    return false;
  }
  size_t r = fread(c->waves, 1, 128 * 1024, f);
  fclose(f);
  if (r < 179 * 2)
    return false;
  for (int i = 0; i < 179; i++)
    c->waves_ptrs[i] = c->waves[i * 2] | (c->waves[i * 2 + 1] << 8);
  return true;
}

static void calculate_wave(RefCtx *c, int ch) {
  uint8_t note = c->step_buffer[ch] - 1;
  uint8_t wave = c->st[ch].current_wave;
  uint16_t freq = 0;

  c->st[ch].freq_table_data = NULL;
  c->st[ch].freq_table_ptr = 0;
  c->st[ch].patch_header_ptr = 0;
  c->st[ch].freq_table_is_full = false;

  if (wave == 175 && note > 35) {
    uint8_t note_idx = note > 89 ? 89 : note;
    note_idx -= 36;
    uint16_t ptr = c->waves_ptrs[175];
    if (!ptr)
      return;
    MSX_GM_DRUM_PATCH *drums = (MSX_GM_DRUM_PATCH *)&c->waves[(ptr - 0x8000) + 8];
    MSX_GM_DRUM_PATCH *dp = &drums[note_idx];
    c->st[ch].next_tone_low = dp->tone;
    c->st[ch].next_tone_high = 0;
    c->st[ch].patch_header_ptr = dp->hdr_ptr;
    freq = dp->freq;
  } else if (wave >= 176) {
    OWN_PATCH *patch = &c->kit.own_patches[wave - 176];
    OWN_PATCH_PART *part = &patch->patch_part[0];
    uint8_t min_note = 0;
    if (patch->transpose)
      note += c->transpose_value;
    for (int i = 0; i < 8; i++) {
      if (note < patch->patch_part[i].next_patch_note ||
          patch->patch_part[i].next_patch_note == 0) {
        part = &patch->patch_part[i];
        break;
      }
      min_note = patch->patch_part[i].next_patch_note;
    }
    c->st[ch].next_tone_low = part->tone + 128;
    c->st[ch].next_tone_high = 1;
    int rel_note = part->tone_note + note - min_note;
    if (rel_note < 0)
      rel_note = 0;
    if (rel_note > 95)
      rel_note = 95;
    c->st[ch].last_note = (uint8_t)rel_note;
    uint8_t type = (c->kit.own_tone_info[part->tone] & 0x06) >> 1;
    if (type == 0x00)
      c->st[ch].freq_table_ptr = c->waves_ptrs[177];
    else if (type == 0x01)
      c->st[ch].freq_table_ptr = c->waves_ptrs[176];
    else
      c->st[ch].freq_table_ptr = c->waves_ptrs[178];
    c->st[ch].freq_table_data = (uint16_t *)&c->waves[c->st[ch].freq_table_ptr - 0x8000];
    c->st[ch].freq_table_is_full = true;
    freq = c->st[ch].freq_table_data[rel_note];
  } else {
    uint16_t ptr = c->waves_ptrs[wave];
    if (!ptr)
      return;
    MSX_PATCH *patch = (MSX_PATCH *)&c->waves[ptr - 0x8000];
    int mapped_note = note;
    if (patch->transpose)
      mapped_note += c->transpose_value;
    if (mapped_note < 0)
      mapped_note = 0;
    if (mapped_note > 95)
      mapped_note = 95;
    MSX_PATCH_PART *part = &patch->parts[0];
    uint8_t min_note = 0;
    while (mapped_note >= part->next_patch_note && part->next_patch_note != 0xFF) {
      min_note = part->next_patch_note;
      part++;
    }
    c->st[ch].next_tone_low = part->tone_low;
    c->st[ch].next_tone_high = part->tone_high & 0x01;
    c->st[ch].patch_header_ptr = patch->hdr_ptr;
    int index = mapped_note - min_note + (part->tone_high >> 1);
    if (index < 0)
      index = 0;
    if (index > 191)
      index = 191;
    c->st[ch].last_note = (uint8_t)index;
    c->st[ch].freq_table_ptr = part->freq_ptr;
    c->st[ch].freq_table_data = (uint16_t *)&c->waves[part->freq_ptr - 0x8000];
    c->st[ch].freq_table_is_full = false;
    uint16_t octave = (uint16_t)g_tabdiv12[index][0] << 8;
    uint8_t tbl_idx = g_tabdiv12[index][1] >> 1;
    freq = c->st[ch].freq_table_data[tbl_idx];
    freq = (freq << 1) + octave;
  }

  int detune_value = c->st[ch].detune_value * 2;
  if (detune_value >= 0)
    detune_value += 0x0800;
  freq += detune_value;
  freq &= 0xF7FF;
  c->st[ch].next_frequency = freq;
}

static void play_waves(RefCtx *c) {
  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    uint8_t ev = c->step_buffer[i];
    if (ev == 0 || ev > NOTE_ON)
      continue;
    c->st[i].pitch_frequency = c->st[i].next_frequency;
    write_wave(c, 0x68 + i, 0x00);
    write_wave(c, 0x50 + i, 0xFF);
    write_wave(c, 0x20 + i,
               (c->st[i].next_frequency & 0xFF) | c->st[i].next_tone_high);
    write_wave(c, 0x38 + i,
               (c->st[i].next_frequency >> 8) | c->st[i].pseudo_reverb);
    write_wave(c, 0x08 + i, c->st[i].next_tone_low);
  }
}

static void note_on_event(RefCtx *c, int ch) {
  if (c->st[ch].patch_header_ptr) {
    uint16_t off = c->st[ch].patch_header_ptr - 0x8000;
    uint8_t *hdr = &c->waves[off];
    int idx = 0;
    uint8_t reg = 0x80;
    uint8_t val = hdr[idx++];
    while (reg != 0xFF && idx < 64) {
      write_wave(c, (uint8_t)(reg + ch), val);
      reg = hdr[idx++];
      if (reg == 0xFF)
        break;
      val = hdr[idx++];
    }
  }
  write_wave(c, 0x50 + ch, (uint8_t)(c->st[ch].volume | 1));
  write_wave(c, 0x68 + ch, (uint8_t)(0x80 | c->st[ch].current_stereo));
}

static void note_off_event(RefCtx *c, int ch) {
  c->st[ch].frequency_mode = FREQUENCY_MODE_NORMAL;
  uint8_t value = read_wave(c, (uint8_t)(0x68 + ch));
  write_wave(c, (uint8_t)(0x68 + ch), (uint8_t)(value & 0x7F));
}

static void wave_change_event(RefCtx *c, int ch) {
  c->st[ch].frequency_mode = FREQUENCY_MODE_NORMAL;
  uint8_t wave = c->step_buffer[ch] - WAVE_CHANGE;
  c->st[ch].current_wave = c->song.header.wave_numbers[wave];
  uint8_t volume = c->song.header.wave_volumes[wave];
  uint8_t level_direct = c->st[ch].volume & 1;
  c->st[ch].volume = (uint8_t)((4 * volume) | level_direct);
}

static void volume_change_event(RefCtx *c, int ch) {
  uint8_t volume = c->step_buffer[ch] - VOLUME_CHANGE;
  volume = (uint8_t)((volume ^ 0x1F) << 1);
  uint8_t level_direct = c->st[ch].volume & 1;
  c->st[ch].volume = (uint8_t)((4 * volume) | level_direct);
  write_wave(c, 0x50 + ch, c->st[ch].volume);
}

static void stereo_change_event(RefCtx *c, int ch) {
  c->st[ch].frequency_mode = FREQUENCY_MODE_NORMAL;
  c->st[ch].current_stereo = (c->step_buffer[ch] - (STEREO_SET + 7)) & 0x0F;
  uint8_t value = read_wave(c, (uint8_t)(0x68 + ch)) & 0xF0;
  write_wave(c, 0x68 + ch, (uint8_t)(value & c->st[ch].current_stereo));
}

static void note_link_event(RefCtx *c, int ch) {
  c->st[ch].frequency_mode = FREQUENCY_MODE_NORMAL;
  uint8_t note = (uint8_t)(c->st[ch].last_note + c->step_buffer[ch] - (NOTE_LINK + 9));
  c->st[ch].last_note = note;
  if (!c->st[ch].freq_table_data)
    return;
  uint16_t frequency;
  if (c->st[ch].next_tone_high == 1 && c->st[ch].next_tone_low >= 128) {
    frequency = c->st[ch].freq_table_data[note];
  } else {
    uint16_t octave = (uint16_t)g_tabdiv12[note][0] << 8;
    uint8_t idx = g_tabdiv12[note][1] >> 1;
    frequency = c->st[ch].freq_table_data[idx];
    frequency = (frequency << 1) + octave;
  }
  int detune_value = c->st[ch].detune_value * 2;
  if (detune_value >= 0)
    detune_value += 0x0800;
  frequency += detune_value;
  frequency &= 0xF7FF;
  c->st[ch].pitch_frequency = frequency;
  write_wave(c, 0x20 + ch, (uint8_t)((frequency & 0xFF) | c->st[ch].next_tone_high));
  write_wave(c, 0x38 + ch, (uint8_t)((frequency >> 8) | c->st[ch].pseudo_reverb));
}

static void pitch_bend_event(RefCtx *c, int ch) {
  int8_t value = (int8_t)(c->step_buffer[ch] - (PITCH_BEND + 9));
  c->st[ch].frequency_mode = FREQUENCY_MODE_PITCH_BEND;
  c->st[ch].pitch_bend_speed = value * 4;
}

static void detune_event(RefCtx *c, int ch) {
  int value = c->step_buffer[ch] - (DETUNE + 3);
  c->st[ch].detune_value = value * 4;
}

static void modulation_event(RefCtx *c, int ch) {
  int8_t value = (int8_t)(c->step_buffer[ch] - MODULATION);
  c->st[ch].pitch_bend_speed = 0;
  c->st[ch].frequency_mode = FREQUENCY_MODE_MODULATION;
  c->st[ch].modulation_index = value;
  c->st[ch].modulation_count = 0;
}

static void damp_event(RefCtx *c, int ch) {
  c->st[ch].frequency_mode = FREQUENCY_MODE_NORMAL;
  uint8_t value = read_wave(c, (uint8_t)(0x68 + ch));
  write_wave(c, 0x68 + ch, (uint8_t)(value | 0x40));
}

static void lfo_event(RefCtx *c, int ch) {
  c->st[ch].frequency_mode = FREQUENCY_MODE_NORMAL;
  uint8_t value = read_wave(c, (uint8_t)(0x68 + ch));
  write_wave(c, 0x68 + ch, (uint8_t)(value ^ 0x20));
}

static void extra_lfo_event(RefCtx *c, int ch) {
  uint8_t idx = c->step_buffer[ch] - XTRA_LFO;
  if (idx >= (sizeof(g_xls_table) / sizeof(g_xls_table[0])))
    return;
  uint8_t value = read_wave(c, (uint8_t)(0x68 + ch));
  write_wave(c, 0x68 + ch, (uint8_t)(value | 0x20));
  write_wave(c, 0x80 + ch, g_xls_table[idx][0]);
  write_wave(c, 0xE0 + ch, g_xls_table[idx][1]);
  write_wave(c, 0x68 + ch, (uint8_t)(value & 0xDF));
}

static void handle_pitch_bend(RefCtx *c, int ch) {
  uint16_t frequency = c->st[ch].pitch_frequency + c->st[ch].pitch_bend_speed;
  if (frequency & 0x0800) {
    if (c->st[ch].pitch_bend_speed < 0)
      frequency &= 0xF7FF;
    else
      frequency += 0x0800;
  }
  c->st[ch].pitch_frequency = frequency;
  write_wave(c, 0x20 + ch, (uint8_t)((frequency & 0xFF) | c->st[ch].next_tone_high));
  write_wave(c, 0x38 + ch, (uint8_t)((frequency >> 8) | c->st[ch].pseudo_reverb));
}

static void handle_modulation(RefCtx *c, int ch) {
  uint16_t frequency = c->st[ch].pitch_frequency;
  uint8_t count = c->st[ch].modulation_count;
  int8_t value = c->song.header.modulation[c->st[ch].modulation_index][count];
  frequency = frequency + (4 * value);
  if (frequency & 0x0800) {
    if (value < 0)
      frequency &= 0xF7FF;
    else
      frequency += 0x0800;
  }
  c->st[ch].pitch_frequency = frequency;
  count = (count + 1) % MODULATION_WAVE_LENGTH;
  c->st[ch].modulation_count = (c->st[ch].modulation_count + 1) % MODULATION_WAVE_LENGTH;
  if (c->song.header.modulation[c->st[ch].modulation_index][count] == 10)
    count = 0;
  c->st[ch].modulation_count = count;
  write_wave(c, 0x20 + ch, (uint8_t)((frequency & 0xFF) | c->st[ch].next_tone_high));
  write_wave(c, 0x38 + ch, (uint8_t)((frequency >> 8) | c->st[ch].pseudo_reverb));
}

static void handle_frequency_mode(RefCtx *c) {
  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    if (c->st[i].frequency_mode == FREQUENCY_MODE_PITCH_BEND)
      handle_pitch_bend(c, i);
    else if (c->st[i].frequency_mode >= FREQUENCY_MODE_MODULATION)
      handle_modulation(c, i);
  }
}

static void decode_step(RefCtx *c) {
  c->step++;
  if (c->step > MAX_STEP) {
    c->step = 0;
    if (c->position < c->song.header.song_length)
      c->position++;
    else {
      if (c->song.header.loop_position != MAX_POSITION && c->position != MAX_POSITION)
        c->position = c->song.header.loop_position;
      else
        c->position = 0;
    }
    uint8_t patt_idx = c->song.position_table[c->position];
    c->pattern_data = c->song.patterns[patt_idx];
  }

  if (!c->pattern_data) {
    memset(c->step_buffer, 0, sizeof(c->step_buffer));
  } else {
    uint8_t val = *c->pattern_data++;
    if (val == 0xFF) {
      memset(c->step_buffer, 0, sizeof(c->step_buffer));
    } else {
      c->step_buffer[0] = val;
      uint8_t data[3];
      data[0] = *c->pattern_data++;
      data[1] = *c->pattern_data++;
      data[2] = *c->pattern_data++;
      int cc = 1;
      for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 8; j++) {
          if (data[i] & 0x80)
            c->step_buffer[cc++] = *c->pattern_data++;
          else
            c->step_buffer[cc++] = 0;
          data[i] <<= 1;
        }
      }
    }
  }

  trace_line(c, "STEP,%" PRIu64 ",%u,%u", c->tick, c->position, c->step);
  for (int i = 0; i < STEP_BUFFER_SIZE; i++) {
    if (c->step_buffer[i] != 0)
      trace_line(c, "EV,%" PRIu64 ",%d,%u", c->tick, i, c->step_buffer[i]);
  }

  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    if (c->step_buffer[i] > 0 && c->step_buffer[i] <= NOTE_ON) {
      c->st[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      calculate_wave(c, i);
    }
  }
}

static void process_events(RefCtx *c) {
  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    uint8_t ev = c->step_buffer[i];
    if (!ev)
      continue;
    if (ev <= NOTE_ON)
      note_on_event(c, i);
    else if (ev == NOTE_OFF)
      note_off_event(c, i);
    else if (ev < VOLUME_CHANGE)
      wave_change_event(c, i);
    else if (ev < STEREO_SET)
      volume_change_event(c, i);
    else if (ev < NOTE_LINK)
      stereo_change_event(c, i);
    else if (ev < PITCH_BEND)
      note_link_event(c, i);
    else if (ev < DETUNE)
      pitch_bend_event(c, i);
    else if (ev < MODULATION)
      detune_event(c, i);
    else if (ev < REVERB_ON)
      modulation_event(c, i);
    else if (ev == REVERB_ON)
      c->st[i].pseudo_reverb = REVERB_ENABLED;
    else if (ev < LFO)
      damp_event(c, i);
    else if (ev < REVERB_OFF)
      lfo_event(c, i);
    else if (ev == REVERB_OFF)
      c->st[i].pseudo_reverb = REVERB_DISABLED;
    else if (ev < NO_EVENT)
      extra_lfo_event(c, i);
  }
}

static void process_command(RefCtx *c) {
  uint8_t cmd = c->step_buffer[COMMAND_CHANNEL];
  if (!cmd)
    return;
  if (cmd <= COMMAND_TEMPO) {
    c->speed = (uint8_t)((COMMAND_TEMPO + 2) - cmd);
    if (c->speed == 0)
      c->speed = 1;
  } else if (cmd == COMMAND_PATTERN_END) {
    c->step = MAX_STEP;
  } else if (cmd < COMMAND_TRANSPOSE) {
  } else if (cmd < COMMAND_FREQUENCY) {
    c->transpose_value = (int8_t)cmd - (COMMAND_TRANSPOSE + 24);
  } else {
    c->base_frequency = (uint8_t)(-(cmd - COMMAND_FREQUENCY));
  }
}

static bool song_requires_mwk(const MwmSong *song) {
  for (int i = 0; i < NR_WAVES; i++)
    if (song->header.wave_numbers[i] >= 176)
      return true;
  return false;
}

int main(int argc, char **argv) {
  if (argc < 5) {
    fprintf(stderr,
            "Usage: %s --ticks N --trace out.csv input.mwm [--waves waves.dat] [--mwk file]\n",
            argv[0]);
    return 1;
  }

  const char *mwm_path = NULL;
  const char *trace_path = NULL;
  const char *waves_path = NULL;
  const char *mwk_path = NULL;
  uint64_t ticks = 0;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--ticks") && i + 1 < argc)
      ticks = (uint64_t)strtoull(argv[++i], NULL, 10);
    else if (!strcmp(argv[i], "--trace") && i + 1 < argc)
      trace_path = argv[++i];
    else if (!strcmp(argv[i], "--waves") && i + 1 < argc)
      waves_path = argv[++i];
    else if (!strcmp(argv[i], "--mwk") && i + 1 < argc)
      mwk_path = argv[++i];
    else if (argv[i][0] != '-')
      mwm_path = argv[i];
  }

  if (!ticks || !trace_path || !mwm_path) {
    fprintf(stderr, "Missing required args.\n");
    return 1;
  }

  RefCtx c;
  memset(&c, 0, sizeof(c));
  uint8_t *dummy_ram = (uint8_t *)calloc(1, 2 * 1024 * 1024);

  if (!load_mwm(mwm_path, &c.song)) {
    fprintf(stderr, "Failed to load MWM: %s\n", mwm_path);
    return 1;
  }

  char waves_default[512] = {0};
  if (!waves_path) {
    snprintf(waves_default, sizeof(waves_default), "%s", "waves.dat");
    waves_path = waves_default;
  }
  if (!load_waves_dat(&c, waves_path)) {
    fprintf(stderr, "Failed to load waves.dat: %s\n", waves_path);
    free_mwm(&c.song);
    return 1;
  }

  if (song_requires_mwk(&c.song)) {
    char resolved_mwk[512] = {0};
    bool loaded = false;
    if (mwk_path) {
      loaded = load_mwk(mwk_path, &c.kit, dummy_ram);
    } else {
      char kit_name[WAVE_KIT_NAME_LENGTH + 1] = {0};
      copy_trimmed_field(kit_name, sizeof(kit_name), c.song.header.wave_kit_name,
                         WAVE_KIT_NAME_LENGTH);
      char header_path[512] = {0};
      char fallback_path[512] = {0};
      bool have_header = kit_name[0] && !is_none_kit_name(kit_name) &&
                         make_header_name_mwk_path(mwm_path, kit_name, header_path,
                                                   sizeof(header_path));
      bool have_fallback =
          make_same_name_mwk_path(mwm_path, fallback_path, sizeof(fallback_path));
      if (have_header && load_mwk(header_path, &c.kit, dummy_ram)) {
        loaded = true;
        strncpy(resolved_mwk, header_path, sizeof(resolved_mwk) - 1);
      } else if (have_fallback && load_mwk(fallback_path, &c.kit, dummy_ram)) {
        loaded = true;
        strncpy(resolved_mwk, fallback_path, sizeof(resolved_mwk) - 1);
      }
    }
    if (!loaded) {
      fprintf(stderr, "MWK required but not found/loaded.\n");
      free(dummy_ram);
      free(c.waves);
      free_mwm(&c.song);
      return 1;
    }
  }

  c.trace_fp = fopen(trace_path, "w");
  if (!c.trace_fp) {
    fprintf(stderr, "Failed to open trace: %s\n", trace_path);
    free(dummy_ram);
    free(c.waves);
    free_mwm(&c.song);
    return 1;
  }

  trace_line(&c, "TRACE_VERSION,1");
  trace_line(&c, "SONG,%s", mwm_path);

  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    c.st[i].last_note = 0;
    c.st[i].frequency_mode = FREQUENCY_MODE_NORMAL;
    c.st[i].pitch_bend_speed = 0;
    c.st[i].modulation_index = 0;
    c.st[i].modulation_count = 0;
    c.st[i].detune_value = c.song.header.detune[i] << 1;
    c.st[i].next_tone_low = 0;
    c.st[i].next_tone_high = 0;
    c.st[i].next_frequency = 0;
    c.st[i].pseudo_reverb = REVERB_DISABLED;
    c.st[i].pitch_frequency = 0;
    if (c.song.header.start_waves[i] > 0 && c.song.header.start_waves[i] <= NR_WAVES) {
      c.st[i].current_wave =
          c.song.header.wave_numbers[c.song.header.start_waves[i] - 1];
      uint8_t volume = c.song.header.wave_volumes[c.song.header.start_waves[i] - 1];
      uint8_t level_direct = volume & 1;
      c.st[i].volume = (uint8_t)((4 * volume) | level_direct);
    } else {
      c.st[i].current_wave = 0;
      c.st[i].volume = 0xFF;
    }
    write_wave(&c, 0x50 + i, c.st[i].volume);
    c.st[i].current_stereo = c.song.header.stereo[i] & 0x0F;
    write_wave(&c, 0x68 + i, c.st[i].current_stereo);
  }

  c.base_frequency = c.song.header.base_frequency;
  c.speed = c.song.header.tempo ? c.song.header.tempo : 1;
  c.speed_count = c.speed - 3;
  c.transpose_value = 0;
  c.position = MAX_POSITION;
  c.step = MAX_STEP;

  for (uint64_t t = 1; t <= ticks; t++) {
    c.tick = t;
    c.seq = 0;
    trace_line(&c, "TICK,%" PRIu64, c.tick);
    handle_frequency_mode(&c);
    c.speed_count++;
    if (c.speed_count >= c.speed) {
      c.speed_count = 0;
      play_waves(&c);
      process_events(&c);
      process_command(&c);
    } else if (c.speed_count == (c.speed - 1)) {
      decode_step(&c);
    }
  }

  fclose(c.trace_fp);
  free(dummy_ram);
  free(c.waves);
  free_mwm(&c.song);
  return 0;
}
