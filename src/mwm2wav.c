#ifndef LIBMOONSOUND_LIBRARY
#include <getopt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "mwm_parser.h"
#include "wav_writer.h"

// libvgm headers
#include "SoundDevs.h"
#include "SoundEmu.h"

#include "libmoonsound.h"

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024

static uint16_t amiga_frq_tab[96] = {
    0xc000, 0xc07a, 0xc0fa, 0xc184, 0xc214, 0xc2ae, 0xc350, 0xc3fc, 0xc4b2,
    0xc574, 0xc642, 0xc71a, 0xd000, 0xd07a, 0xd0fa, 0xd184, 0xd214, 0xd2ae,
    0xd350, 0xd3fc, 0xd4b2, 0xd574, 0xd642, 0xd71a, 0xe000, 0xe07a, 0xe0fa,
    0xe184, 0xe214, 0xe2ae, 0xe350, 0xe3fc, 0xe4b2, 0xe574, 0xe642, 0xe71a,
    0xf000, 0xf07a, 0xf0fa, 0xf184, 0xf214, 0xf2ae, 0xf350, 0xf3fc, 0xf4b2,
    0xf574, 0xf642, 0xf71a, 0x0000, 0x007a, 0x00fa, 0x0184, 0x0214, 0x02ae,
    0x0350, 0x03fc, 0x04b2, 0x0574, 0x0642, 0x071a, 0x1000, 0x107a, 0x10fa,
    0x1184, 0x1214, 0x12ae, 0x1350, 0x13fc, 0x14b2, 0x1574, 0x1642, 0x171a,
    0x2000, 0x207a, 0x20fa, 0x2184, 0x2214, 0x22ae, 0x2350, 0x23fc, 0x24b2,
    0x2574, 0x2642, 0x271a, 0x3000, 0x307a, 0x30fa, 0x3184, 0x3214, 0x32ae,
    0x3350, 0x33fc, 0x34b2, 0x3574, 0x3642, 0x371a};

#define STEP_BUFFER_SIZE 25
#define NOTE_ON 96
#define NOTE_OFF 97
#define WAVE_CHANGE 98
#define VOLUME_CHANGE 146
#define STEREO_SET 178
#define NOTE_LINK 193
#define COMMAND_CHANNEL 24
#define COMMAND_TEMPO 23
#define COMMAND_PATTERN_END 24
#define COMMAND_STATUS_BYTE 25
#define COMMAND_TRANSPOSE 28
#define COMMAND_FREQUENCY 77

#define PITCH_BEND 212
#define DETUNE 231
#define MODULATION 238
#define REVERB_ON 241
#define DAMP 242
#define LFO 243
#define REVERB_OFF 245
#define XTRA_LFO 246

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
  MSX_PATCH_PART parts[1]; // Flexible array
} MSX_PATCH;

typedef struct {
  uint8_t tone;
  uint16_t freq;
  uint16_t hdr_ptr;
} MSX_GM_DRUM_PATCH;
#pragma pack(pop)

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

typedef struct {
  DEV_INFO ymf278b;
  uint8_t *rom;
  uint8_t *ram;
  uint8_t *waves;           // Melodic Waves patch data
  uint16_t waves_ptrs[179]; // Pointers to patches
  MwmSong song;
  MwkKit kit;
  int seconds;
  bool dump;
  bool noteon;
  bool debug;
  int solo_ch; // -1 = all channels, 0-23 = solo that channel
  uint32_t current_sample;
  uint32_t total_samples;
} Player;

typedef struct {
  uint8_t last_note;
  uint8_t current_wave;
  uint8_t volume;
  uint8_t key_on_flag;
  uint8_t current_stereo;
  uint8_t damp_flag;
  uint8_t lfo_flag;
  uint8_t pseudo_reverb;
  uint8_t frequency_mode;
  int16_t pitch_bend_speed;
  int8_t modulation_index;
  uint8_t modulation_count;
  int8_t detune_value;
  uint16_t next_frequency;
  uint16_t pitch_frequency;
  uint8_t next_tone_low;
  uint8_t next_tone_high;
  uint8_t next_frequency_high;
  uint8_t next_frequency_low;
  uint16_t freq_table_ptr;
  uint8_t patch_header[5];
  int dbg_mapped_note;
  int dbg_index;
  uint16_t dbg_freq;
  uint16_t dbg_freq_ptr;
  uint16_t dbg_table_val;
  uint8_t dbg_table_type;
  uint8_t dbg_full_table;
} StatusTable;

StatusTable g_status[NR_WAVE_CHANNELS];
uint8_t g_step_buffer[STEP_BUFFER_SIZE];
uint32_t g_samples_per_step;
uint32_t g_sample_counter;
uint32_t g_samples_per_tick;
uint32_t g_tick_sample_counter;
int g_position;
int g_step;
uint8_t *g_pattern_data;
int8_t g_transpose_value;
uint8_t g_speed;
uint8_t g_speed_count;
uint8_t g_base_frequency;
bool g_step_ready;

static void copy_trimmed_field(char *dst, size_t dst_size, const char *src,
                               size_t src_size) {
  if (!dst || dst_size == 0)
    return;
  dst[0] = '\0';
  if (!src || src_size == 0)
    return;

  size_t len = src_size;
  while (len > 0 &&
         (((unsigned char)src[len - 1]) == 0 || src[len - 1] == ' '))
    len--;

  if (len >= dst_size)
    len = dst_size - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
}

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

static bool make_same_name_mwk_path(const char *mwm_path, char *out,
                                    size_t out_size) {
  if (!mwm_path || !out || out_size == 0)
    return false;

  size_t len = strlen(mwm_path);
  if (len + 5 >= out_size)
    return false;
  strcpy(out, mwm_path);

  char *last_slash = strrchr(out, '/');
  char *dot = strrchr(out, '.');
  if (dot && (!last_slash || dot > last_slash))
    strcpy(dot, ".MWK");
  else
    strcat(out, ".MWK");
  return true;
}

static bool make_header_name_mwk_path(const char *mwm_path, const char *kit_name,
                                      char *out, size_t out_size) {
  if (!mwm_path || !kit_name || !kit_name[0] || !out || out_size == 0)
    return false;

  const char *last_slash = strrchr(mwm_path, '/');
  if (!last_slash)
    return snprintf(out, out_size, "%s.MWK", kit_name) < (int)out_size;

  size_t dir_len = (size_t)(last_slash - mwm_path + 1);
  if (dir_len + strlen(kit_name) + 4 + 1 > out_size)
    return false;
  memcpy(out, mwm_path, dir_len);
  out[dir_len] = '\0';
  strcat(out, kit_name);
  strcat(out, ".MWK");
  return true;
}

static void get_song_kit_name(const MwmSong *song, char *out, size_t out_size) {
  if (!song || !out || out_size == 0)
    return;
  copy_trimmed_field(out, out_size, song->header.wave_kit_name,
                     WAVE_KIT_NAME_LENGTH);
}

void write_ymf278b(Player *p, uint8_t reg, uint8_t val) {
  DEVFUNC_WRITE_A8D8 w;
  SndEmu_GetDeviceFunc(p->ymf278b.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8,
                       0, (void **)&w);
  if (w) {
    w(p->ymf278b.dataPtr, 4, reg);
    w(p->ymf278b.dataPtr, 5, val);
  }
}

static uint8_t compose_wave_ctrl(int ch) {
  return g_status[ch].key_on_flag | (g_status[ch].current_stereo & 0x0F) |
         g_status[ch].damp_flag | g_status[ch].lfo_flag;
}

uint32_t samples_per_tick(uint8_t base_frequency) {
  double refresh_hz = 60.0;
  if (base_frequency == 0) {
    refresh_hz = 60.0;
  } else if (base_frequency == 1) {
    refresh_hz = 50.0;
  } else {
    double s = base_frequency * 0.0000808;
    refresh_hz = 1.0 / s + 0.5;
  }
  double spt = (double)SAMPLE_RATE / refresh_hz;
  if (spt < 1.0)
    spt = 1.0;
  return (uint32_t)(spt + 0.5);
}

void calculate_wave(Player *p, int ch) {
  uint8_t note = g_step_buffer[ch] - 1;
  uint8_t wave = g_status[ch].current_wave;
  uint16_t freq = 0;
  g_status[ch].dbg_mapped_note = -1;
  g_status[ch].dbg_index = -1;
  g_status[ch].dbg_freq = 0;
  g_status[ch].dbg_freq_ptr = 0;
  g_status[ch].dbg_table_val = 0;
  g_status[ch].dbg_table_type = 0;
  g_status[ch].dbg_full_table = 0;

  if (wave == 175 && note > 35) {
    // GM Drum patch from WAVES.DAT
    uint8_t note_idx = note;
    if (note_idx > 89)
      note_idx = 89;
    note_idx -= 36;

    uint16_t ptr = p->waves_ptrs[175];
    if (!ptr)
      return; // fail gracefully

    // In MSX, PATCH struct is 8 bytes (bool + 16-bit ptr + 5-byte PATCH_PART)
    MSX_GM_DRUM_PATCH *drums =
        (MSX_GM_DRUM_PATCH *)&p->waves[(ptr - 0x8000) + 8];
    MSX_GM_DRUM_PATCH *dp = &drums[note_idx];

    g_status[ch].next_tone_low = dp->tone;
    g_status[ch].next_tone_high = 0; // ROM tone

    g_status[ch].patch_header[0] = dp->hdr_ptr & 0xFF;
    g_status[ch].patch_header[1] = (dp->hdr_ptr >> 8) & 0xFF;

    freq = dp->freq;
    g_status[ch].last_note = note;
    g_status[ch].freq_table_ptr = 0;
    g_status[ch].dbg_mapped_note = note;
    g_status[ch].dbg_index = note;
    g_status[ch].dbg_freq = freq;
  } else if (wave >= 176) {
    // RAM Patch
    OWN_PATCH *patch = &p->kit.own_patches[wave - 176];
    OWN_PATCH_PART *part = &patch->patch_part[0];
    if (patch->transpose)
      note += g_transpose_value;
    uint8_t min_note = 0;
    for (int i = 0; i < 8; i++) {
      if (note < patch->patch_part[i].next_patch_note ||
          patch->patch_part[i].next_patch_note == 0) {
        part = &patch->patch_part[i];
        break;
      }
      min_note = patch->patch_part[i].next_patch_note;
    }
    g_status[ch].next_tone_low = part->tone + 128;
    g_status[ch].next_tone_high = 1;

    int relative_note = part->tone_note + note - min_note;
    if (relative_note < 0)
      relative_note = 0;
    if (relative_note > 95)
      relative_note = 95;

    uint8_t type = (p->kit.own_tone_info[part->tone] & 0x06) >> 1;
    uint16_t *freq_table = NULL;
    if (type == 0x00)
      freq_table = (uint16_t *)&p->waves[p->waves_ptrs[177] - 0x8000]; // AMIGA
    else if (type == 0x01)
      freq_table = (uint16_t *)&p->waves[p->waves_ptrs[176] - 0x8000]; // PC
    else
      freq_table = (uint16_t *)&p->waves[p->waves_ptrs[178] - 0x8000]; // TURBO

    if (!freq_table)
      return;
    freq = freq_table[relative_note];
    g_status[ch].last_note = relative_note;
    if (type == 0x00)
      g_status[ch].freq_table_ptr = p->waves_ptrs[177];
    else if (type == 0x01)
      g_status[ch].freq_table_ptr = p->waves_ptrs[176];
    else
      g_status[ch].freq_table_ptr = p->waves_ptrs[178];
    g_status[ch].dbg_mapped_note = note;
    g_status[ch].dbg_index = relative_note;
    g_status[ch].dbg_freq = freq;
    g_status[ch].dbg_table_val = freq;
    g_status[ch].dbg_table_type = type;
  } else {
    // ROM Patch lookup via WAVES.DAT
    if (!p->waves) {
      g_status[ch].next_tone_low = wave & 0xFF;
      g_status[ch].next_tone_high = (wave >> 8) & 0x01;
      memset(g_status[ch].patch_header, 0, 5);
      uint16_t fnums[] = {0x0157, 0x016B, 0x0181, 0x0198, 0x01B0, 0x01CA,
                          0x01E5, 0x0202, 0x0220, 0x0241, 0x0263, 0x0287};
      uint16_t fnum = fnums[note % 12];
      int oct = (note / 12) + 1;
      g_status[ch].next_frequency_low =
          ((fnum & 0x7F) * 2) | g_status[ch].next_tone_high;
      g_status[ch].next_frequency_high = (oct << 4) | ((fnum >> 7) & 0x07);
      return;
    }
    uint16_t ptr = p->waves_ptrs[wave];
    if (!ptr)
      return;

    MSX_PATCH *patch = (MSX_PATCH *)&p->waves[ptr - 0x8000];

    int mapped_note = note;
    if (patch->transpose)
      mapped_note += g_transpose_value;
    if (mapped_note < 0)
      mapped_note = 0;
    if (mapped_note > 95)
      mapped_note = 95;

    MSX_PATCH_PART *part = &patch->parts[0];
    uint8_t min_note = 0;
    while (mapped_note >= part->next_patch_note &&
           part->next_patch_note != 0xFF) {
      min_note = part->next_patch_note;
      part++;
    }

    g_status[ch].next_tone_low = part->tone_low;
    g_status[ch].next_tone_high = part->tone_high & 0x01;

    g_status[ch].patch_header[0] = patch->hdr_ptr & 0xFF;
    g_status[ch].patch_header[1] = (patch->hdr_ptr >> 8) & 0xFF;

    int index = mapped_note - min_note + (part->tone_high >> 1);
    if (index < 0)
      index = 0;
    if (index > 191)
      index = 191;

    uint16_t *freq_table = (uint16_t *)&p->waves[part->freq_ptr - 0x8000];
    uint16_t octave = g_tabdiv12[index][0] << 8;
    uint8_t tbl_idx = g_tabdiv12[index][1] >> 1;
    freq = freq_table[tbl_idx];
    freq = (freq << 1) + octave;
    g_status[ch].dbg_table_val = freq_table[tbl_idx];

    g_status[ch].dbg_mapped_note = mapped_note;
    g_status[ch].dbg_index = index;
    g_status[ch].dbg_freq = freq;
    g_status[ch].dbg_freq_ptr = part->freq_ptr;
    g_status[ch].dbg_full_table = 0;
    g_status[ch].last_note = index;
    g_status[ch].freq_table_ptr = part->freq_ptr;
  }

  int detune_value = (g_status[ch].detune_value) * 2;
  if (detune_value >= 0)
    detune_value += 0x0800;
  freq += detune_value;
  freq &= 0xF7FF;
  g_status[ch].dbg_freq = freq;

  g_status[ch].next_frequency = freq;
  g_status[ch].next_frequency_low = (freq & 0xFF) | g_status[ch].next_tone_high;
  g_status[ch].next_frequency_high = (freq >> 8) & 0xFF;
}

void note_on(Player *p, int ch) {
  if (p->noteon) {
    uint8_t oct = g_status[ch].next_frequency_high >> 4;
    printf("CH%02d NoteOn: Note=%d Tone=%d%s Freq=%02X%02X Oct=%d Vol=%02X "
           "Pan=%02X\n",
           ch, g_step_buffer[ch], g_status[ch].next_tone_low,
           g_status[ch].next_tone_high ? "(RAM)" : "",
           g_status[ch].next_frequency_high, g_status[ch].next_frequency_low,
           oct, g_status[ch].volume, g_status[ch].current_stereo);
  }
  if (p->debug) {
    printf("DBG CH%02d wave=%u note=%u mapped=%d idx=%d toneH=%u toneL=%u "
           "freq_ptr=%04X full=%u tbl=%04X freq=%04X detune=%d\n",
           ch, g_status[ch].current_wave, g_step_buffer[ch],
           g_status[ch].dbg_mapped_note, g_status[ch].dbg_index,
           g_status[ch].next_tone_high, g_status[ch].next_tone_low,
           g_status[ch].dbg_freq_ptr, g_status[ch].dbg_full_table,
           g_status[ch].dbg_table_val, g_status[ch].dbg_freq,
           (int)(p->song.header.detune[ch] << 1));
  }

  g_status[ch].key_on_flag = 0x80;
  if (p->solo_ch >= 0 && ch != p->solo_ch)
    return;

  /* Apply WAVES.DAT envelope if available (GM drums included) */
  if (p->waves) {
    uint16_t hdr_ptr =
        g_status[ch].patch_header[0] | (g_status[ch].patch_header[1] << 8);
    if (hdr_ptr) {
      uint32_t hdr_off = hdr_ptr - 0x8000;
      uint8_t *headers = &p->waves[hdr_off];

      int idx = 0;
      uint8_t reg = 0x80;
      uint8_t val = headers[idx++];
      while (reg != 0xFF && idx < 50) {
        write_ymf278b(p, reg + ch, val);
        reg = headers[idx++];
        if (reg == 0xFF)
          break;
        val = headers[idx++];
      }
    }
  }

  write_ymf278b(p, 0x50 + ch, g_status[ch].volume | 0x01);
  write_ymf278b(p, 0x68 + ch, compose_wave_ctrl(ch));
}

void note_off(Player *p, int ch) {
  g_status[ch].key_on_flag = 0x00;
  if (p->solo_ch >= 0 && ch != p->solo_ch)
    return;
  write_ymf278b(p, 0x68 + ch, compose_wave_ctrl(ch));
}

#ifndef LIBMOONSOUND_LIBRARY
void get_executable_dir(const char *argv0, char *out, size_t out_size) {
  if (!out || out_size == 0)
    return;
  out[0] = '\0';

#ifdef __APPLE__
  uint32_t size = (uint32_t)out_size;
  if (_NSGetExecutablePath(out, &size) == 0) {
    char resolved[512] = {0};
    if (realpath(out, resolved)) {
      char *slash = strrchr(resolved, '/');
      if (slash) {
        *slash = '\0';
        strncpy(out, resolved, out_size - 1);
        out[out_size - 1] = '\0';
        return;
      }
    }
  }
#endif

  char resolved[512] = {0};
  if (realpath(argv0, resolved)) {
    char *slash = strrchr(resolved, '/');
    if (slash) {
      *slash = '\0';
      strncpy(out, resolved, out_size - 1);
      out[out_size - 1] = '\0';
      return;
    }
  }

  // Fallback to current directory
  strncpy(out, ".", out_size - 1);
  out[out_size - 1] = '\0';
}

static bool file_exists_readable(const char *path) {
  if (!path || !path[0])
    return false;
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  fclose(f);
  return true;
}

static void resolve_cli_asset_path(const char *exe_dir, const char *filename,
                                   char *out, size_t out_size) {
  if (!out || out_size == 0)
    return;
  out[0] = '\0';
  if (!filename || !filename[0])
    return;

  if (exe_dir && exe_dir[0]) {
    char exe_candidate[512] = {0};
    snprintf(exe_candidate, sizeof(exe_candidate), "%s/%s", exe_dir, filename);
    if (file_exists_readable(exe_candidate)) {
      strncpy(out, exe_candidate, out_size - 1);
      out[out_size - 1] = '\0';
      return;
    }
  }

  // Fallback: current working directory
  strncpy(out, filename, out_size - 1);
  out[out_size - 1] = '\0';
}
#endif

void handle_pitch_bend(Player *p, int ch) {
  uint16_t frequency =
      g_status[ch].pitch_frequency + g_status[ch].pitch_bend_speed;

  if (frequency & 0x0800) {
    if (g_status[ch].pitch_bend_speed < 0)
      frequency = frequency & 0xF7FF;
    else
      frequency += 0x0800;
  }

  g_status[ch].pitch_frequency = frequency;

  if (p->solo_ch < 0 || ch == p->solo_ch) {
    write_ymf278b(p, 0x20 + ch,
                  (frequency & 0xFF) | g_status[ch].next_tone_high);
    write_ymf278b(p, 0x38 + ch,
                  (frequency >> 8) | g_status[ch].pseudo_reverb);
  }
}

void handle_modulation(Player *p, int ch) {
  uint16_t frequency = g_status[ch].pitch_frequency;
  uint8_t count = g_status[ch].modulation_count;
  int8_t value = p->song.header
                     .modulation[g_status[ch].modulation_index][count];
  frequency = frequency + (4 * value);

  if (frequency & 0x0800) {
    if (value < 0)
      frequency = frequency & 0xF7FF;
    else
      frequency += 0x0800;
  }

  g_status[ch].pitch_frequency = frequency;

  count = (count + 1) % MODULATION_WAVE_LENGTH;
  g_status[ch].modulation_count =
      (g_status[ch].modulation_count + 1) % MODULATION_WAVE_LENGTH;
  if (p->song.header.modulation[g_status[ch].modulation_index][count] == 10)
    count = 0;
  g_status[ch].modulation_count = count;

  if (p->solo_ch < 0 || ch == p->solo_ch) {
    write_ymf278b(p, 0x20 + ch,
                  (frequency & 0xFF) | g_status[ch].next_tone_high);
    write_ymf278b(p, 0x38 + ch,
                  (frequency >> 8) | g_status[ch].pseudo_reverb);
  }
}

void handle_frequency_mode(Player *p) {
  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    if (g_status[i].frequency_mode == FREQUENCY_MODE_PITCH_BEND)
      handle_pitch_bend(p, i);
    else if (g_status[i].frequency_mode >= FREQUENCY_MODE_MODULATION)
      handle_modulation(p, i);
  }
}

void play_waves(Player *p) {
  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    uint8_t ev = g_step_buffer[i];
    if (ev == 0 || ev > NOTE_ON)
      continue;

    g_status[i].pitch_frequency = g_status[i].next_frequency;

    if (p->solo_ch >= 0 && i != p->solo_ch)
      continue;

    // Re-trigger note without re-applying WAVES.DAT envelope headers.
    write_ymf278b(p, 0x68 + i, 0x00);
    write_ymf278b(p, 0x50 + i, 0xFF);
    write_ymf278b(p, 0x20 + i,
                  g_status[i].next_frequency_low | g_status[i].next_tone_high);
    write_ymf278b(p, 0x38 + i,
                  g_status[i].next_frequency_high | g_status[i].pseudo_reverb);
    write_ymf278b(p, 0x08 + i, g_status[i].next_tone_low);
  }
}

void play_line(Player *p) {
  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    uint8_t ev = g_step_buffer[i];
    if (ev == 0)
      continue;

    if (ev <= 96) {
      g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      note_on(p, i);
    } else if (ev == 97) {
      g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      note_off(p, i);
    } else if (ev >= 98 && ev <= 145) {
      g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      uint8_t wave = ev - WAVE_CHANGE;
      g_status[i].current_wave = p->song.header.wave_numbers[wave];

      uint8_t volume = p->song.header.wave_volumes[wave];
      uint8_t level_direct = g_status[i].volume & 0x01;
      g_status[i].volume = (4 * volume) | level_direct;
    } else if (ev >= 146 && ev <= 177) {
      uint8_t vol = ev - 146;
      vol = (vol ^ 0x1F) << 1;
      uint8_t level_direct = g_status[i].volume & 0x01;
      g_status[i].volume = (4 * vol) | level_direct;
      if (p->solo_ch < 0 || i == p->solo_ch)
        write_ymf278b(p, 0x50 + i, g_status[i].volume);
    } else if (ev >= 178 && ev <= 192) {
      g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      g_status[i].current_stereo = (ev - (178 + 7)) & 0x0F;
      if (p->solo_ch < 0 || i == p->solo_ch)
        write_ymf278b(p, 0x68 + i, compose_wave_ctrl(i));
    } else if (ev >= NOTE_LINK && ev <= NOTE_LINK + 18) {
      g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      int note = g_status[i].last_note + (ev - (NOTE_LINK + 9));
      if (note < 0)
        note = 0;
      if (note > 95)
        note = 95;

      if (!p->waves || !g_status[i].freq_table_ptr)
        continue;

      uint16_t *freq_table =
          (uint16_t *)&p->waves[g_status[i].freq_table_ptr - 0x8000];

      uint16_t freq = 0;
      if (g_status[i].next_tone_high == 1 &&
          g_status[i].next_tone_low >= 128) {
        freq = freq_table[note];
      } else {
        uint16_t octave = g_tabdiv12[note][0] << 8;
        uint8_t tbl_idx = g_tabdiv12[note][1] >> 1;
        freq = freq_table[tbl_idx];
        freq = (freq << 1) + octave;
      }

  int detune_value = (g_status[i].detune_value) * 2;
      if (detune_value >= 0)
        detune_value += 0x0800;
      freq += detune_value;
      freq &= 0xF7FF;

      g_status[i].last_note = note;
      g_status[i].pitch_frequency = freq;
      g_status[i].next_frequency_low =
          (freq & 0xFF) | g_status[i].next_tone_high;
      g_status[i].next_frequency_high = (freq >> 8) & 0xFF;

      if (p->solo_ch < 0 || i == p->solo_ch) {
        write_ymf278b(p, 0x20 + i,
                      g_status[i].next_frequency_low |
                          g_status[i].next_tone_high);
        write_ymf278b(p, 0x38 + i,
                      g_status[i].next_frequency_high |
                          g_status[i].pseudo_reverb);
      }
    } else if (ev >= PITCH_BEND && ev < DETUNE) {
      int8_t value = ev - (PITCH_BEND + 9);
      g_status[i].frequency_mode = FREQUENCY_MODE_PITCH_BEND;
      g_status[i].pitch_bend_speed = value * 4;
    } else if (ev >= DETUNE && ev < MODULATION) {
      int value = ev - (DETUNE + 3);
      g_status[i].detune_value = value * 4;
    } else if (ev >= MODULATION && ev < REVERB_ON) {
      int8_t value = ev - MODULATION;
      g_status[i].pitch_bend_speed = 0;
      g_status[i].frequency_mode = FREQUENCY_MODE_MODULATION;
      g_status[i].modulation_index = value;
      g_status[i].modulation_count = 0;
    } else if (ev == REVERB_ON) {
      g_status[i].pseudo_reverb = REVERB_ENABLED;
    } else if (ev >= DAMP && ev < LFO) {
      g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      g_status[i].damp_flag = 0x40;
      if (p->solo_ch < 0 || i == p->solo_ch) {
        write_ymf278b(p, 0x68 + i, compose_wave_ctrl(i));
      }
    } else if (ev >= LFO && ev < REVERB_OFF) {
      g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      g_status[i].lfo_flag ^= 0x20;
      if (p->solo_ch < 0 || i == p->solo_ch) {
        write_ymf278b(p, 0x68 + i, compose_wave_ctrl(i));
      }
    } else if (ev == REVERB_OFF) {
      g_status[i].pseudo_reverb = REVERB_DISABLED;
    } else if (ev >= XTRA_LFO && ev < 255) {
      uint8_t index = ev - XTRA_LFO;
      static const uint8_t g_xls_table[][2] = {
          {49, 0}, {50, 0}, {51, 0}, {52, 0}, {53, 0},
          {54, 0}, {55, 0}, {58, 5}, {3, 0}};
      if (p->solo_ch < 0 || i == p->solo_ch) {
        g_status[i].lfo_flag |= 0x20;
        write_ymf278b(p, 0x68 + i, compose_wave_ctrl(i));
        write_ymf278b(p, 0x80 + i, g_xls_table[index][0]);
        write_ymf278b(p, 0xE0 + i, g_xls_table[index][1]);
        g_status[i].lfo_flag = 0x00;
        write_ymf278b(p, 0x68 + i, compose_wave_ctrl(i));
      } else {
        g_status[i].lfo_flag = 0x00;
      }
    }
  }
}

bool next_step(Player *p) {
  bool looped = false;
  g_step++;
  if (g_step > 15) {
    g_step = 0;
    g_position++;
    if (g_position > p->song.header.song_length) {
      g_position = p->song.header.loop_position;
      looped = true;
    }
    uint8_t patt_idx = p->song.position_table[g_position];
    g_pattern_data = p->song.patterns[patt_idx];
  }

  if (!g_pattern_data) {
    memset(g_step_buffer, 0, 25);
  } else {
    uint8_t val = *g_pattern_data++;
    if (val == 0xFF) {
      memset(g_step_buffer, 0, 25);
    } else {
      g_step_buffer[0] = val;
      uint8_t mask[3];
      mask[0] = *g_pattern_data++;
      mask[1] = *g_pattern_data++;
      mask[2] = *g_pattern_data++;
      for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 8; j++) {
          int ch = 1 + i * 8 + j;
          if (mask[i] & 0x80)
            g_step_buffer[ch] = *g_pattern_data++;
          else
            g_step_buffer[ch] = 0;
          mask[i] <<= 1;
        }
      }
    }
  }

  if (p->dump) {
    static const char *note_names[] = {"C ", "C#", "D ", "D#", "E ", "F ",
                                       "F#", "G ", "G#", "A ", "A#", "B "};
    printf("%02X:%02X", g_position, g_step);
    for (int i = 0; i < 24; i++) {
      uint8_t ev = g_step_buffer[i];
      if (ev >= 1 && ev <= 96) {
        uint8_t semitone = (ev - 1) % 12;
        uint8_t octave = (ev - 1) / 12 + 1;
        printf(" |%s%d", note_names[semitone], octave);
      } else if (ev == 97) {
        printf(" | OFF");
      } else if (ev > 0) {
        printf(" | %3d", ev);
      } else {
        printf(" |----");
      }
    }
    if (g_step_buffer[24])
      printf(" CMD:%02X", g_step_buffer[24]);
    printf("\n");
  }

  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    if (g_step_buffer[i] > 0 && g_step_buffer[i] <= NOTE_ON) {
      g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
      calculate_wave(p, i);
    }
  }

  g_step_ready = true;
  return looped;
}

bool song_supports_loop(const MwmSong *song) {
  return song->header.loop_position < song->header.song_length;
}

bool song_requires_mwk(const MwmSong *song) {
  for (int i = 0; i < NR_WAVES; i++) {
    if (song->header.wave_numbers[i] >= 176)
      return true;
  }
  return false;
}

bool next_step_calc(Player *p, bool allow_loop, bool *ended) {
  bool looped = false;
  if (g_step > 15) {
    g_step = 0;
    g_position++;
    if (g_position > p->song.header.song_length) {
      if (allow_loop && song_supports_loop(&p->song)) {
        g_position = p->song.header.loop_position;
        looped = true;
      } else {
        if (ended)
          *ended = true;
        return false;
      }
    }
    uint8_t patt_idx = p->song.position_table[g_position];
    g_pattern_data = p->song.patterns[patt_idx];
  }

  if (!g_pattern_data) {
    memset(g_step_buffer, 0, 25);
  } else {
    uint8_t val = *g_pattern_data++;
    if (val == 0xFF) {
      memset(g_step_buffer, 0, 25);
    } else {
      g_step_buffer[0] = val;
      uint8_t mask[3];
      mask[0] = *g_pattern_data++;
      mask[1] = *g_pattern_data++;
      mask[2] = *g_pattern_data++;
      for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 8; j++) {
          int ch = 1 + i * 8 + j;
          if (mask[i] & 0x80)
            g_step_buffer[ch] = *g_pattern_data++;
          else
            g_step_buffer[ch] = 0;
          mask[i] <<= 1;
        }
      }
    }
  }

  g_step++;
  if (ended)
    *ended = false;
  return looped;
}

void init_player(Player *p) {
  g_samples_per_step = (SAMPLE_RATE * 10) / 600;
  g_transpose_value = 0;
  g_base_frequency = p->song.header.base_frequency;
  g_samples_per_tick = samples_per_tick(g_base_frequency);
  g_tick_sample_counter = g_samples_per_tick;
  g_speed = p->song.header.tempo;
  if (g_speed == 0)
    g_speed = 1;
  g_speed_count = g_speed - 3;
  g_step_ready = false;
  for (int i = 0; i < NR_WAVE_CHANNELS; i++) {
    uint8_t wave_idx = p->song.header.start_waves[i];
    if (wave_idx > 0 && wave_idx <= NR_WAVES) {
      g_status[i].current_wave = p->song.header.wave_numbers[wave_idx - 1];
      uint8_t vol = p->song.header.wave_volumes[wave_idx - 1];
      uint8_t level_direct = vol & 0x01;
      g_status[i].volume = (4 * vol) | level_direct;
    } else {
      g_status[i].current_wave = 0;
      g_status[i].volume = 0xFF;
    }
    g_status[i].key_on_flag = 0x00;
    g_status[i].current_stereo = p->song.header.stereo[i] & 0x0F;
    g_status[i].damp_flag = 0;
    g_status[i].lfo_flag = 0;
    g_status[i].pseudo_reverb = REVERB_DISABLED;
    g_status[i].frequency_mode = FREQUENCY_MODE_NORMAL;
    g_status[i].pitch_bend_speed = 0;
    g_status[i].modulation_index = 0;
    g_status[i].modulation_count = 0;
    g_status[i].detune_value = p->song.header.detune[i] << 1;
    g_status[i].pitch_frequency = 0;

    write_ymf278b(p, 0x50 + i, g_status[i].volume);
    write_ymf278b(p, 0x68 + i, compose_wave_ctrl(i));
  }
}

void process_command(Player *p) {
  uint8_t cmd = g_step_buffer[24];
  if (!cmd)
    return;

  if (cmd <= COMMAND_TEMPO) {
    g_speed = (COMMAND_TEMPO + 2) - cmd;
    if (g_speed == 0)
      g_speed = 1;
  } else if (cmd == COMMAND_PATTERN_END) {
    g_step = 16;
  } else if (cmd < COMMAND_TRANSPOSE) {
    // Status byte command not supported
  } else if (cmd < COMMAND_FREQUENCY) {
    g_transpose_value = (int8_t)cmd - (COMMAND_TRANSPOSE + 24);
  } else {
    g_base_frequency = -(cmd - COMMAND_FREQUENCY);
    g_samples_per_tick = samples_per_tick(g_base_frequency);
  }
}

uint32_t calculate_track_samples(Player *p, bool supports_loop, uint32_t loops,
                                 uint32_t *intro_samples,
                                 uint32_t *loop_samples) {
  g_samples_per_step = (SAMPLE_RATE * 10) / 600;
  g_transpose_value = 0;
  g_base_frequency = p->song.header.base_frequency;
  g_samples_per_tick = samples_per_tick(g_base_frequency);
  g_tick_sample_counter = g_samples_per_tick;
  g_speed = p->song.header.tempo;
  if (g_speed == 0)
    g_speed = 1;
  g_speed_count = g_speed - 3;
  g_step_ready = false;

  g_position = -1;
  g_step = 16;
  g_pattern_data = NULL;

  uint64_t total_samples = 0;
  uint64_t first_loop_samples = 0;
  bool saw_first_loop = false;
  bool allow_loop = supports_loop && (loops > 0);
  while (true) {
    g_speed_count++;
    if (g_speed_count >= g_speed) {
      g_speed_count = 0;
      bool ended = false;
      bool looped = next_step_calc(p, allow_loop, &ended);
      if (ended)
        break;
      if (looped) {
        if (!saw_first_loop) {
          saw_first_loop = true;
          first_loop_samples = total_samples;
          if (loops == 0) {
            break;
          }
        } else {
          break;
        }
      }
      process_command(p);
    }

    total_samples += g_samples_per_tick;
    if (total_samples > UINT32_MAX) {
      total_samples = UINT32_MAX;
      break;
    }
  }

  if (intro_samples)
    *intro_samples = (uint32_t)first_loop_samples;

  if (!saw_first_loop) {
    if (loop_samples)
      *loop_samples = 0;
    return (uint32_t)total_samples;
  }

  uint64_t one_loop = total_samples - first_loop_samples;
  if (loop_samples)
    *loop_samples = (uint32_t)one_loop;

  uint64_t total = first_loop_samples + one_loop * loops;
  if (total > UINT32_MAX)
    total = UINT32_MAX;
  return (uint32_t)total;
}

#ifdef __cplusplus
extern "C" {
#endif

struct MSContext {
  Player p;
  bool mwm_loaded;
  bool rom_loaded;
  bool waves_loaded;
  bool mwk_loaded;
  bool supports_loop;
  bool requires_mwk;
  bool seconds_set;
  int seconds_limit;
  int loop_count;
  char mwm_path[512];
  bool mwm_path_set;
  char mwk_header_name[WAVE_KIT_NAME_LENGTH + 1];
  char mwk_path[512];
  bool mwk_path_set;
  char resolved_mwk_path[512];
  char last_error[256];
  bool prepared;
};

static void ms_clear_error(MSContext *ctx) {
  if (!ctx)
    return;
  ctx->last_error[0] = '\0';
}

static void ms_set_error(MSContext *ctx, const char *fmt, ...) {
  if (!ctx || !fmt)
    return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(ctx->last_error, sizeof(ctx->last_error), fmt, ap);
  va_end(ap);
}

static bool ms_try_load_mwk_path(MSContext *ctx, const char *path) {
  if (!ctx || !path || !path[0] || !ctx->p.ram)
    return false;
  if (!load_mwk(path, &ctx->p.kit, ctx->p.ram))
    return false;
  ctx->mwk_loaded = true;
  strncpy(ctx->resolved_mwk_path, path, sizeof(ctx->resolved_mwk_path) - 1);
  ctx->resolved_mwk_path[sizeof(ctx->resolved_mwk_path) - 1] = '\0';
  return true;
}

MSContext *ms_create(void) {
  MSContext *ctx = (MSContext *)calloc(1, sizeof(MSContext));
  if (!ctx)
    return NULL;
  ctx->loop_count = 1;
  ctx->p.solo_ch = -1;
  ms_clear_error(ctx);
  return ctx;
}

void ms_stop(MSContext *ctx) {
  if (!ctx)
    return;

  if (ctx->prepared) {
    SndEmu_Stop(&ctx->p.ymf278b);
    SndEmu_FreeDevLinkData(&ctx->p.ymf278b);
    ctx->prepared = false;
  }

  if (ctx->p.rom) {
    free(ctx->p.rom);
    ctx->p.rom = NULL;
  }
  if (ctx->p.ram) {
    free(ctx->p.ram);
    ctx->p.ram = NULL;
  }
  if (ctx->p.waves) {
    free(ctx->p.waves);
    ctx->p.waves = NULL;
  }
  free_mwm(&ctx->p.song);
}

void ms_destroy(MSContext *ctx) {
  if (!ctx)
    return;
  ms_stop(ctx);
  free(ctx);
}

static bool ms_load_file(const char *path, uint8_t *dst, size_t size) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;
  size_t read = fread(dst, 1, size, f);
  fclose(f);
  return read == size;
}

int ms_load_rom_file(MSContext *ctx, const char *rom_path) {
  if (!ctx || !rom_path)
    return 0;
  if (!ctx->p.rom)
    ctx->p.rom = (uint8_t *)malloc(2 * 1024 * 1024);
  if (!ctx->p.rom)
    return 0;
  if (!ms_load_file(rom_path, ctx->p.rom, 2 * 1024 * 1024))
    return 0;
  ctx->rom_loaded = true;
  return 1;
}

int ms_load_waves_file(MSContext *ctx, const char *waves_path) {
  if (!ctx || !waves_path)
    return 0;
  if (!ctx->p.waves)
    ctx->p.waves = (uint8_t *)malloc(128 * 1024);
  if (!ctx->p.waves)
    return 0;
  FILE *wf = fopen(waves_path, "rb");
  if (!wf)
    return 0;
  size_t read = fread(ctx->p.waves, 1, 128 * 1024, wf);
  fclose(wf);
  if (read == 0)
    return 0;
  for (int i = 0; i < 179; i++) {
    ctx->p.waves_ptrs[i] =
        ctx->p.waves[i * 2] | (ctx->p.waves[i * 2 + 1] << 8);
  }
  ctx->waves_loaded = true;
  return 1;
}

int ms_load_mwm_file(MSContext *ctx, const char *mwm_path) {
  if (!ctx || !mwm_path)
    return 0;
  ms_clear_error(ctx);
  free_mwm(&ctx->p.song);
  memset(&ctx->p.song, 0, sizeof(ctx->p.song));
  if (!load_mwm(mwm_path, &ctx->p.song))
  {
    ms_set_error(ctx, "Failed to load MWM: %s", mwm_path);
    return 0;
  }
  ctx->mwm_loaded = true;
  ctx->supports_loop = song_supports_loop(&ctx->p.song);
  ctx->requires_mwk = song_requires_mwk(&ctx->p.song);
  ctx->mwk_loaded = false;
  ctx->mwk_path_set = false;
  ctx->mwk_path[0] = '\0';
  ctx->resolved_mwk_path[0] = '\0';

  strncpy(ctx->mwm_path, mwm_path, sizeof(ctx->mwm_path) - 1);
  ctx->mwm_path[sizeof(ctx->mwm_path) - 1] = '\0';
  ctx->mwm_path_set = true;
  get_song_kit_name(&ctx->p.song, ctx->mwk_header_name,
                    sizeof(ctx->mwk_header_name));
  return 1;
}

int ms_load_mwk_file(MSContext *ctx, const char *mwk_path) {
  if (!ctx || !mwk_path)
    return 0;
  ms_clear_error(ctx);
  strncpy(ctx->mwk_path, mwk_path, sizeof(ctx->mwk_path) - 1);
  ctx->mwk_path[sizeof(ctx->mwk_path) - 1] = '\0';
  ctx->mwk_path_set = true;
  if (!ctx->p.ram)
    return 1;
  if (!ms_try_load_mwk_path(ctx, ctx->mwk_path)) {
    ms_set_error(ctx, "Failed to load MWK: %s", ctx->mwk_path);
    return 0;
  }
  return 1;
}

void ms_set_seconds_limit(MSContext *ctx, int seconds) {
  if (!ctx)
    return;
  ctx->seconds_set = true;
  ctx->seconds_limit = seconds;
}

void ms_clear_seconds_limit(MSContext *ctx) {
  if (!ctx)
    return;
  ctx->seconds_set = false;
  ctx->seconds_limit = 0;
}

void ms_set_loop_count(MSContext *ctx, int loops) {
  if (!ctx)
    return;
  ctx->loop_count = loops;
}

bool ms_supports_loop(MSContext *ctx) {
  if (!ctx)
    return false;
  return ctx->supports_loop;
}

bool ms_requires_mwk(MSContext *ctx) {
  if (!ctx)
    return false;
  return ctx->requires_mwk;
}

const char *ms_get_expected_mwk_name(MSContext *ctx) {
  if (!ctx)
    return "";
  return ctx->mwk_header_name;
}

const char *ms_get_resolved_mwk_path(MSContext *ctx) {
  if (!ctx)
    return "";
  return ctx->resolved_mwk_path;
}

const char *ms_get_last_error(MSContext *ctx) {
  if (!ctx)
    return "";
  return ctx->last_error;
}

uint32_t ms_calculate_length_samples(MSContext *ctx, int loops) {
  if (!ctx || !ctx->mwm_loaded)
    return 0;
  if (loops < 0)
    loops = 0;
  if (!ctx->supports_loop)
    loops = 0;
  return calculate_track_samples(&ctx->p, ctx->supports_loop,
                                 (uint32_t)loops, NULL, NULL);
}

uint32_t ms_get_total_samples(MSContext *ctx) {
  if (!ctx)
    return 0;
  return ctx->p.total_samples;
}

int ms_prepare(MSContext *ctx) {
  if (!ctx || !ctx->mwm_loaded || !ctx->rom_loaded || !ctx->waves_loaded) {
    if (ctx)
      ms_set_error(ctx, "Missing required assets (MWM/ROM/WAVES).");
    return 0;
  }
  ms_clear_error(ctx);
  ctx->resolved_mwk_path[0] = '\0';
  ctx->mwk_loaded = false;

  if (ctx->loop_count < 0)
    ctx->loop_count = 0;

  if (ctx->seconds_set) {
    ctx->p.total_samples = (uint32_t)ctx->seconds_limit * SAMPLE_RATE;
    ctx->p.seconds = ctx->seconds_limit;
  } else {
    uint32_t loops = (uint32_t)ctx->loop_count;
    if (!ctx->supports_loop)
      loops = 0;
    ctx->p.total_samples =
        calculate_track_samples(&ctx->p, ctx->supports_loop, loops, NULL, NULL);
    ctx->p.seconds =
        (int)((ctx->p.total_samples + SAMPLE_RATE - 1) / SAMPLE_RATE);
  }

  DEV_GEN_CFG cfg;
  cfg.emuCore = 0;
  cfg.srMode = DEVRI_SRMODE_NATIVE;
  cfg.clock = 33868800; // OPL4 clock
  cfg.smplRate = SAMPLE_RATE;

  if (SndEmu_Start(DEVID_YMF278B, &cfg, &ctx->p.ymf278b) != 0) {
    ms_set_error(ctx, "Failed to start YMF278B emulator.");
    return 0;
  }

  if (!ctx->p.ram)
    ctx->p.ram = (uint8_t *)malloc(2 * 1024 * 1024);
  if (!ctx->p.ram) {
    ms_set_error(ctx, "Failed to allocate OPL4 RAM.");
    return 0;
  }
  memset(ctx->p.ram, 0, 2 * 1024 * 1024);

  if (ctx->requires_mwk) {
    if (ctx->mwk_path_set) {
      if (!ms_try_load_mwk_path(ctx, ctx->mwk_path)) {
        ms_set_error(ctx, "Failed to load MWK: %s", ctx->mwk_path);
        return 0;
      }
    } else {
      char header_path[512] = {0};
      char fallback_path[512] = {0};
      bool have_header_path = false;
      bool have_fallback_path = false;

      if (ctx->mwm_path_set &&
          ctx->mwk_header_name[0] &&
          !is_none_kit_name(ctx->mwk_header_name)) {
        have_header_path = make_header_name_mwk_path(
            ctx->mwm_path, ctx->mwk_header_name, header_path,
            sizeof(header_path));
      }
      if (ctx->mwm_path_set) {
        have_fallback_path =
            make_same_name_mwk_path(ctx->mwm_path, fallback_path,
                                    sizeof(fallback_path));
      }

      if (have_header_path && ms_try_load_mwk_path(ctx, header_path)) {
        /* loaded */
      } else if (have_fallback_path &&
                 (!have_header_path || strcmp(header_path, fallback_path) != 0) &&
                 ms_try_load_mwk_path(ctx, fallback_path)) {
        /* loaded */
      } else {
        if (have_header_path && have_fallback_path &&
            strcmp(header_path, fallback_path) != 0) {
          ms_set_error(ctx,
                       "MWK load failed. Header kit=\"%s\". Tried \"%s\" then \"%s\".",
                       ctx->mwk_header_name[0] ? ctx->mwk_header_name : "(empty)",
                       header_path, fallback_path);
        } else if (have_fallback_path) {
          ms_set_error(ctx,
                       "MWK load failed. Tried \"%s\" (header kit=\"%s\").",
                       fallback_path,
                       ctx->mwk_header_name[0] ? ctx->mwk_header_name : "(empty)");
        } else {
          ms_set_error(ctx,
                       "MWK load failed. Could not derive MWK path from MWM path.");
        }
        return 0;
      }
    }
  } else if (ctx->mwk_path_set) {
    if (ms_try_load_mwk_path(ctx, ctx->mwk_path))
      ctx->mwk_loaded = true;
  }

  // Upload ROM/RAM to emulator
  void (*write_rom)(void *, uint32_t, uint32_t, const uint8_t *);
  void (*alloc_rom)(void *, uint32_t);
  void (*write_ram)(void *, uint32_t, uint32_t, const uint8_t *);
  void (*alloc_ram)(void *, uint32_t);
  DEVFUNC_WRITE_A8D8 write8;

  SndEmu_GetDeviceFunc(ctx->p.ymf278b.devDef, RWF_REGISTER | RWF_WRITE,
                       DEVRW_A8D8, 0, (void **)&write8);
  SndEmu_GetDeviceFunc(ctx->p.ymf278b.devDef, RWF_MEMORY | RWF_WRITE,
                       DEVRW_MEMSIZE, 0x524F, (void **)&alloc_rom);
  SndEmu_GetDeviceFunc(ctx->p.ymf278b.devDef, RWF_MEMORY | RWF_WRITE,
                       DEVRW_BLOCK, 0x524F, (void **)&write_rom);
  SndEmu_GetDeviceFunc(ctx->p.ymf278b.devDef, RWF_MEMORY | RWF_WRITE,
                       DEVRW_MEMSIZE, 0x5241, (void **)&alloc_ram);
  SndEmu_GetDeviceFunc(ctx->p.ymf278b.devDef, RWF_MEMORY | RWF_WRITE,
                       DEVRW_BLOCK, 0x5241, (void **)&write_ram);

  if (alloc_rom)
    alloc_rom(ctx->p.ymf278b.dataPtr, 2 * 1024 * 1024);
  if (write_rom)
    write_rom(ctx->p.ymf278b.dataPtr, 0, 2 * 1024 * 1024, ctx->p.rom);
  if (alloc_ram)
    alloc_ram(ctx->p.ymf278b.dataPtr, 2 * 1024 * 1024);
  if (write_ram)
    write_ram(ctx->p.ymf278b.dataPtr, 0, 2 * 1024 * 1024, ctx->p.ram);

  // Enable OPL4 WaveTable (NEW2 bit in OPL3 Port B, Reg 0x05)
  if (write8) {
    write8(ctx->p.ymf278b.dataPtr, 2, 0x05);
    write8(ctx->p.ymf278b.dataPtr, 3, 0x03); // NEW + NEW2
  }

  // Unmute PCM and set to sound generation mode
  write_ymf278b(&ctx->p, 0x02, 0x11); // Sound Generation Mode + SRAM
  write_ymf278b(&ctx->p, 0xF8, 0x00); // Master FM Volume (0 = max)
  write_ymf278b(&ctx->p, 0xF9, 0x00); // Master PCM Volume (0 = max)

  init_player(&ctx->p);

  // Pre-roll: 100ms of silence to let emulator initialize
  {
    for (int k = 0; k < 4410; k++) {
      DEV_SMPL pcm_s[2] = {0, 0};
      DEV_SMPL *pcm_ptrs[2] = {&pcm_s[0], &pcm_s[1]};
      ctx->p.ymf278b.devDef->Update(ctx->p.ymf278b.dataPtr, 1, pcm_ptrs);
    }
  }

  g_tick_sample_counter = g_samples_per_tick;
  g_position = -1;
  g_step = 15;
  g_step_ready = false;

  ctx->p.current_sample = 0;
  ctx->prepared = true;
  return 1;
}

uint32_t ms_render(MSContext *ctx, int16_t *out_interleaved,
                   uint32_t frames) {
  if (!ctx || !ctx->prepared || !out_interleaved || frames == 0)
    return 0;

  uint32_t remaining = frames;
  if (ctx->p.total_samples > 0) {
    if (ctx->p.current_sample >= ctx->p.total_samples)
      return 0;
    uint32_t left = ctx->p.total_samples - ctx->p.current_sample;
    if (left < remaining)
      remaining = left;
  }

  for (uint32_t i = 0; i < remaining; i++) {
    if (g_tick_sample_counter >= g_samples_per_tick) {
      g_tick_sample_counter = 0;
      handle_frequency_mode(&ctx->p);
      g_speed_count++;
      if (g_speed_count >= g_speed) {
        g_speed_count = 0;
        if (!g_step_ready)
          next_step(&ctx->p);
        play_waves(&ctx->p);
        play_line(&ctx->p);
        process_command(&ctx->p);
        g_step_ready = false;
      } else if (g_speed_count == (g_speed - 1)) {
        next_step(&ctx->p);
      }
    }

    DEV_SMPL pcm_s[2] = {0, 0};
    DEV_SMPL *pcm_ptrs[2] = {&pcm_s[0], &pcm_s[1]};
    ctx->p.ymf278b.devDef->Update(ctx->p.ymf278b.dataPtr, 1, pcm_ptrs);

    int32_t l = pcm_s[0];
    int32_t r = pcm_s[1];

    out_interleaved[i * 2] =
        (l > 32767) ? 32767 : (l < -32768) ? -32768 : (int16_t)l;
    out_interleaved[i * 2 + 1] =
        (r > 32767) ? 32767 : (r < -32768) ? -32768 : (int16_t)r;

    g_tick_sample_counter++;
  }

  ctx->p.current_sample += remaining;
  return remaining;
}

#ifdef __cplusplus
}
#endif

#ifndef LIBMOONSOUND_LIBRARY
int main(int argc, char **argv) {
  int seconds = 60;
  bool seconds_set = false;
  bool loop_flag = false;
  int loop_count = 1;
  bool dump = false;
  bool noteon = false;
  bool debug = false;
  int solo_ch = -1;
  char rom_path[512] = {0};
  char waves_path[512] = {0};

  static struct option long_options[] = {{"seconds", required_argument, 0, 's'},
                                         {"loop", optional_argument, 0, 'l'},
                                         {"dump", no_argument, 0, 'd'},
                                         {"noteon", no_argument, 0, 'n'},
                                         {"debug", no_argument, 0, 'D'},
                                         {"solo", required_argument, 0, 'C'},
                                         {0, 0, 0, 0}};

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "s:l::dnDC:", long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 's':
      seconds = atoi(optarg);
      seconds_set = true;
      break;
    case 'l':
      loop_flag = true;
      if (optarg)
        loop_count = atoi(optarg);
      break;
    case 'd':
      dump = true;
      break;
    case 'n':
      noteon = true;
      break;
    case 'D':
      debug = true;
      break;
    case 'C':
      solo_ch = atoi(optarg);
      break;
    }
  }

  if (argc - optind < 2) {
    printf("Usage: %s [--seconds N] [--loop[=N]] [--dump] [--noteon] [--debug] "
           "[--solo CH] "
           "input.mwm output.wav\n",
           argv[0]);
    return 1;
  }

  char *mwm_file = argv[optind];
  char *output_file = argv[optind + 1];

  // Resolve ROM/WAVES paths relative to the executable location.
  char exe_path[512] = {0};
  get_executable_dir(argv[0], exe_path, sizeof(exe_path));
  resolve_cli_asset_path(exe_path, "yrw801.rom", rom_path, sizeof(rom_path));
  resolve_cli_asset_path(exe_path, "waves.dat", waves_path, sizeof(waves_path));

  Player p = {0};
  p.seconds = seconds;
  p.dump = dump;
  p.noteon = noteon;
  p.debug = debug;
  p.solo_ch = solo_ch;

  if (!load_mwm(mwm_file, &p.song)) {
    printf("Failed to load MWM: %s\n", mwm_file);
    return 1;
  }

  bool supports_loop = song_supports_loop(&p.song);
  if (loop_flag) {
    printf("Loop support: %s (loop_position=%u, song_length=%u)\n",
           supports_loop ? "yes" : "no", p.song.header.loop_position,
           p.song.header.song_length);
    if (!supports_loop)
      printf("Looping disabled: track has no loop point.\n");
  }

  bool requires_mwk = song_requires_mwk(&p.song);
  printf("MWK required: %s\n", requires_mwk ? "yes" : "no");

  char mwk_header_name[WAVE_KIT_NAME_LENGTH + 1] = {0};
  char mwk_header_path[512] = {0};
  char mwk_fallback_path[512] = {0};
  bool have_header_path = false;
  bool have_fallback_path = false;

  get_song_kit_name(&p.song, mwk_header_name, sizeof(mwk_header_name));
  if (mwm_file && mwk_header_name[0] && !is_none_kit_name(mwk_header_name)) {
    have_header_path = make_header_name_mwk_path(
        mwm_file, mwk_header_name, mwk_header_path, sizeof(mwk_header_path));
  }
  if (mwm_file) {
    have_fallback_path =
        make_same_name_mwk_path(mwm_file, mwk_fallback_path,
                                sizeof(mwk_fallback_path));
  }

  if (loop_count < 0)
    loop_count = 0;

  if (!seconds_set) {
    uint32_t loops = loop_flag ? (uint32_t)loop_count : 1;
    if (!supports_loop)
      loops = 0;
    p.total_samples = calculate_track_samples(&p, supports_loop, loops, NULL, NULL);
    p.seconds = (int)((p.total_samples + SAMPLE_RATE - 1) / SAMPLE_RATE);
  } else {
    p.total_samples = (uint32_t)p.seconds * SAMPLE_RATE;
  }

  DEV_GEN_CFG cfg;
  cfg.emuCore = 0;
  cfg.srMode = DEVRI_SRMODE_NATIVE;
  cfg.clock = 33868800; // OPL4 clock
  cfg.smplRate = SAMPLE_RATE;

  if (SndEmu_Start(DEVID_YMF278B, &cfg, &p.ymf278b) != 0) {
    printf("Failed to start YMF278B\n");
    return 1;
  }
  // NOTE: ymf278b already auto-creates and links its own OPL3 — do NOT create a
  // separate one

  // Allocate ROM/RAM
  p.rom = malloc(2 * 1024 * 1024);
  FILE *rf = fopen(rom_path, "rb");
  if (rf) {
    fread(p.rom, 1, 2 * 1024 * 1024, rf);
    fclose(rf);
  } else {
    printf("Error: Could not open ROM file %s\n", rom_path);
    return 1;
  }

  p.ram = malloc(2 * 1024 * 1024);
  memset(p.ram, 0, 2 * 1024 * 1024);

  p.waves = malloc(128 * 1024); // WAVES.DAT is usually small
  FILE *wf = fopen(waves_path, "rb");
  if (wf) {
    fread(p.waves, 1, 128 * 1024, wf);
    fclose(wf);
    for (int i = 0; i < 179; i++) {
      p.waves_ptrs[i] = p.waves[i * 2] | (p.waves[i * 2 + 1] << 8);
    }
  } else {
    printf("Error: Could not open waves.dat file %s\n", waves_path);
    free(p.waves);
    p.waves = NULL;
    return 1;
  }

  if (requires_mwk) {
    bool loaded = false;
    if (have_header_path)
      loaded = load_mwk(mwk_header_path, &p.kit, p.ram);
    if (!loaded && have_fallback_path &&
        (!have_header_path || strcmp(mwk_header_path, mwk_fallback_path) != 0))
      loaded = load_mwk(mwk_fallback_path, &p.kit, p.ram);

    if (!loaded) {
      if (have_header_path && have_fallback_path &&
          strcmp(mwk_header_path, mwk_fallback_path) != 0) {
        printf("Error: MWK required but load failed. Header kit=\"%s\". Tried:\n",
               mwk_header_name[0] ? mwk_header_name : "(empty)");
        printf("  1) %s\n", mwk_header_path);
        printf("  2) %s\n", mwk_fallback_path);
      } else if (have_fallback_path) {
        printf("Error: MWK required but load failed. Tried: %s\n",
               mwk_fallback_path);
      } else {
        printf("Error: MWK required but no MWK path could be derived.\n");
      }
      return 1;
    }
  }

  // Upload ROM/RAM to emulator
  void (*write_rom)(void *, uint32_t, uint32_t, const uint8_t *);
  void (*alloc_rom)(void *, uint32_t);
  void (*write_ram)(void *, uint32_t, uint32_t, const uint8_t *);
  void (*alloc_ram)(void *, uint32_t);
  DEVFUNC_WRITE_A8D8 write8;

  SndEmu_GetDeviceFunc(p.ymf278b.devDef, RWF_REGISTER | RWF_WRITE, DEVRW_A8D8,
                       0, (void **)&write8);
  SndEmu_GetDeviceFunc(p.ymf278b.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE,
                       0x524F, (void **)&alloc_rom);
  SndEmu_GetDeviceFunc(p.ymf278b.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK,
                       0x524F, (void **)&write_rom);
  SndEmu_GetDeviceFunc(p.ymf278b.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_MEMSIZE,
                       0x5241, (void **)&alloc_ram);
  SndEmu_GetDeviceFunc(p.ymf278b.devDef, RWF_MEMORY | RWF_WRITE, DEVRW_BLOCK,
                       0x5241, (void **)&write_ram);

  if (alloc_rom)
    alloc_rom(p.ymf278b.dataPtr, 2 * 1024 * 1024);
  if (write_rom)
    write_rom(p.ymf278b.dataPtr, 0, 2 * 1024 * 1024, p.rom);
  if (alloc_ram)
    alloc_ram(p.ymf278b.dataPtr, 2 * 1024 * 1024);
  if (write_ram)
    write_ram(p.ymf278b.dataPtr, 0, 2 * 1024 * 1024, p.ram);

  // Enable OPL4 WaveTable (NEW2 bit in OPL3 Port B, Reg 0x05)
  if (write8) {
    write8(p.ymf278b.dataPtr, 2, 0x05);
    write8(p.ymf278b.dataPtr, 3, 0x03); // NEW + NEW2
  }

  // Unmute PCM and set to sound generation mode
  write_ymf278b(&p, 0x02, 0x11); // Sound Generation Mode + SRAM
  write_ymf278b(&p, 0xF8, 0x00); // Master FM Volume (0 = max)
  write_ymf278b(&p, 0xF9, 0x00); // Master PCM Volume (0 = max)

  // Initial state
  init_player(&p);

  FILE *out = fopen(output_file, "wb");
  if (!out) {
    printf("Failed to open output file: %s\n", output_file);
    return 1;
  }

  write_wav_header(out, SAMPLE_RATE, 2, p.total_samples);

  // Pre-roll: 100ms of silence to let emulator initialize
  {
    int16_t silent[4410 * 2] = {0};
    for (int k = 0; k < 4410; k++) {
      DEV_SMPL pcm_s[2] = {0, 0};
      DEV_SMPL *pcm_ptrs[2] = {&pcm_s[0], &pcm_s[1]};
      p.ymf278b.devDef->Update(p.ymf278b.dataPtr, 1, pcm_ptrs);
    }
    fwrite(silent, 4, 4410, out);
  }

  g_tick_sample_counter = g_samples_per_tick;
  g_position = -1;
  g_step = 15;
  g_step_ready = false;

  int16_t buffer[BUFFER_SIZE * 2];

  while (p.current_sample < p.total_samples) {
    uint32_t to_render = BUFFER_SIZE;
    if (p.current_sample + to_render > p.total_samples)
      to_render = p.total_samples - p.current_sample;

    for (uint32_t i = 0; i < to_render; i++) {
      if (g_tick_sample_counter >= g_samples_per_tick) {
        g_tick_sample_counter = 0;
        handle_frequency_mode(&p);
        g_speed_count++;
        if (g_speed_count >= g_speed) {
          g_speed_count = 0;
          if (!g_step_ready)
            next_step(&p);
          play_waves(&p);
          play_line(&p);
          process_command(&p);
          g_step_ready = false;
        } else if (g_speed_count == (g_speed - 1)) {
          next_step(&p);
        }
      }

      DEV_SMPL pcm_s[2] = {0, 0};
      DEV_SMPL *pcm_ptrs[2] = {&pcm_s[0], &pcm_s[1]};
      p.ymf278b.devDef->Update(p.ymf278b.dataPtr, 1, pcm_ptrs);

      int32_t l = pcm_s[0];
      int32_t r = pcm_s[1];

      static int scnt = 0;
      if (p.noteon && scnt++ % 44100 == 0)
        printf("PCM raw: %d %d\n", (int)pcm_s[0], (int)pcm_s[1]);

      buffer[i * 2] = (l > 32767) ? 32767 : (l < -32768) ? -32768 : (int16_t)l;
      buffer[i * 2 + 1] = (r > 32767)    ? 32767
                          : (r < -32768) ? -32768
                                         : (int16_t)r;

      g_tick_sample_counter++;
    }

    fwrite(buffer, 4, to_render, out);
    p.current_sample += to_render;
  }

  update_wav_header(out, p.current_sample);
  fclose(out);
  printf("Generated %.2f seconds (%u samples)\n",
         (double)p.current_sample / SAMPLE_RATE, p.current_sample);

  SndEmu_Stop(&p.ymf278b);
  SndEmu_FreeDevLinkData(&p.ymf278b);
  free(p.rom);
  free(p.ram);
  free_mwm(&p.song);

  return 0;
}
#endif
