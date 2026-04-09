#ifndef MWM_PARSER_H
#define MWM_PARSER_H

#include <stdbool.h>
#include <stdint.h>

#define NR_WAVE_CHANNELS 24
#define NR_WAVES 48
#define MAX_OWN_TONES 64
#define MAX_OWN_PATCHES 48
#define MAX_POSITION 219
#define SONG_NAME_LENGTH 50
#define WAVE_KIT_NAME_LENGTH 8
#define MODULATION_WAVE_LENGTH 16
#define NR_MODULATION_WAVES 3

#pragma pack(push, 1)
typedef struct {
  uint8_t song_length;
  uint8_t loop_position;
  uint8_t stereo[NR_WAVE_CHANNELS];
  uint8_t tempo;
  uint8_t base_frequency;
  int8_t detune[NR_WAVE_CHANNELS];
  int8_t modulation[NR_MODULATION_WAVES][MODULATION_WAVE_LENGTH];
  uint8_t start_waves[NR_WAVE_CHANNELS];
  uint8_t wave_numbers[NR_WAVES];
  uint8_t wave_volumes[NR_WAVES];
  char song_name[SONG_NAME_LENGTH];
  char wave_kit_name[WAVE_KIT_NAME_LENGTH];
} MWM_HEADER;

typedef struct {
  uint16_t size;
  uint8_t nr_of_patterns;
} MWM_PATTERN_HEADER;

typedef struct {
  uint8_t tone;
  uint16_t frequency;
  uint8_t header_bytes[16];
} GM_DRUM_PATCH;

typedef struct {
  uint8_t next_patch_note;
  uint8_t tone;
  uint8_t tone_note;
} OWN_PATCH_PART;

typedef struct {
  uint8_t transpose;
  OWN_PATCH_PART patch_part[8];
} OWN_PATCH;

#pragma pack(pop)

typedef struct {
  uint8_t own_tone_info[MAX_OWN_TONES];
  OWN_PATCH own_patches[MAX_OWN_PATCHES];
  uint8_t nr_of_waves;
  uint32_t total_sample_size;
} MwkKit;

typedef struct {
  MWM_HEADER header;
  uint8_t position_table[MAX_POSITION + 1];
  uint8_t **patterns;
  uint8_t max_pattern;
  uint8_t xlfo[18];
  bool has_xlfo;
} MwmSong;

bool load_mwm(const char *filename, MwmSong *song);
void free_mwm(MwmSong *song);

bool load_mwk(const char *filename, MwkKit *kit, uint8_t *opl4_ram);

#endif
