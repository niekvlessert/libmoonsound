#include "mwm_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool load_mwm(const char *filename, MwmSong *song) {
  FILE *f = fopen(filename, "rb");
  if (!f)
    return false;

  char signature[6];
  if (fread(signature, 1, 6, f) != 6) {
    fclose(f);
    return false;
  }

  bool edit_mode = false;
  if (strncmp(signature, "MBMS\x10\x08", 6) != 0) {
    if (strncmp(signature, "MBMS\x10\x07", 6) != 0) {
      fclose(f);
      return false;
    }
    edit_mode = true;
  }

  memset(song, 0, sizeof(MwmSong));

  // The header is 278 bytes.
  fread(&song->header, sizeof(MWM_HEADER), 1, f);

  if (edit_mode) {
    fread(song->position_table, 1, MAX_POSITION + 1, f);
  } else {
    fread(song->position_table, 1, song->header.song_length + 1, f);
  }

  uint8_t max_pos = 0;
  for (int i = 0; i <= song->header.song_length; i++) {
    if (song->position_table[i] > max_pos)
      max_pos = song->position_table[i];
  }
  song->max_pattern = max_pos;

  // Read pattern offsets (2 bytes each for MSX)
  uint16_t *pattern_offsets = malloc(sizeof(uint16_t) * (max_pos + 1));
  fread(pattern_offsets, 2, max_pos + 1, f);

  // Read patterns in chunks
  song->patterns = calloc(max_pos + 1, sizeof(uint8_t *));

  MWM_PATTERN_HEADER ph;
  int pattern_count = 0;
  while (pattern_count <= max_pos) {
    if (fread(&ph, sizeof(MWM_PATTERN_HEADER), 1, f) != 1)
      break;
    if (ph.nr_of_patterns == 0)
      break;

    uint8_t *chunk_data = malloc(ph.size);
    fread(chunk_data, 1, ph.size, f);

    // The offsets we read earlier are absolute addresses.
    // We need to find the base address of the first pattern in this chunk.
    // In LICKIT, the first offset is often 0x8000 or something similar.
    uint16_t base_offset = pattern_offsets[pattern_count];

    for (int j = 0; j < ph.nr_of_patterns && (pattern_count + j) <= max_pos;
         j++) {
      uint16_t current_offset = pattern_offsets[pattern_count + j];
      int relative_offset = current_offset - base_offset;
      if (relative_offset >= 0 && relative_offset < ph.size) {
        // Find the length of this pattern by looking at the next offset
        // or the end of the chunk
        int pattern_len;
        if (j < ph.nr_of_patterns - 1) {
          pattern_len = pattern_offsets[pattern_count + j + 1] - current_offset;
        } else {
          pattern_len = ph.size - relative_offset;
        }
        if (pattern_len > 0) {
          song->patterns[pattern_count + j] = malloc(pattern_len);
          memcpy(song->patterns[pattern_count + j],
                 chunk_data + relative_offset, pattern_len);
        }
      }
    }
    pattern_count += ph.nr_of_patterns;
    free(chunk_data);
  }

  // XLFO
  char xlfo_sig[4];
  if (fread(xlfo_sig, 1, 4, f) == 4) {
    if (strncmp(xlfo_sig, "XLFO", 4) == 0) {
      fread(song->xlfo, 1, 18, f);
      song->has_xlfo = true;
    }
  }

  free(pattern_offsets);
  fclose(f);
  return true;
}

void free_mwm(MwmSong *song) {
  if (song->patterns) {
    for (int i = 0; i <= song->max_pattern; i++) {
      if (song->patterns[i])
        free(song->patterns[i]);
    }
    free(song->patterns);
  }
}

bool load_mwk(const char *filename, MwkKit *kit, uint8_t *opl4_ram) {
  FILE *f = fopen(filename, "rb");
  if (!f)
    return false;

  char signature[6];
  if (fread(signature, 1, 6, f) != 6) {
    fclose(f);
    return false;
  }

  bool edit_mode = false;
  if (strncmp(signature, "MBMS\x10\x0D", 6) != 0) {
    if (strncmp(signature, "MBMS\x10\x0C", 6) != 0) {
      fclose(f);
      return false;
    }
    edit_mode = true;
  }

  uint8_t size_bytes[3];
  fread(size_bytes, 1, 3, f);
  kit->total_sample_size =
      size_bytes[0] | (size_bytes[1] << 8) | (size_bytes[2] << 16);
  fread(&kit->nr_of_waves, 1, 1, f);
  fread(kit->own_tone_info, 1, MAX_OWN_TONES, f);
  fread(kit->own_patches, sizeof(OWN_PATCH), kit->nr_of_waves, f);

  if (edit_mode) {
    fseek(f, kit->nr_of_waves * 16, SEEK_CUR);
  }

  uint32_t header_address = 0;
  uint32_t sample_address = 0x200300; // Relative to OPL4 memory root

  for (int i = 0; i < MAX_OWN_TONES; i++) {
    if (kit->own_tone_info[i] & 0x01) {
      uint8_t sample_header[13];
      if (edit_mode)
        fseek(f, 16, SEEK_CUR);
      fread(sample_header, 1, 13, f);

      uint8_t *ram_header = &opl4_ram[header_address];
      // Byte 0: Bit 7-6 = Bits/sample, Bit 5-0 = Address bits 21-16
      ram_header[0] =
          ((sample_address >> 16) & 0x3F) | (kit->own_tone_info[i] & 0xC0);
      ram_header[1] = (sample_address >> 8) & 0xFF;
      ram_header[2] = sample_address & 0xFF;

      // Copy remaining 9 bytes of header
      ram_header[3] = sample_header[2];
      ram_header[4] = sample_header[3];
      ram_header[5] = sample_header[4];
      ram_header[6] = sample_header[5];
      ram_header[7] = sample_header[6];
      ram_header[8] = sample_header[7];
      ram_header[9] = sample_header[8];
      ram_header[10] = sample_header[9];
      ram_header[11] = sample_header[10];

      // Load sample data into RAM at offset relative to 0x200000
      uint32_t ram_offset = sample_address - 0x200000;
      uint16_t sample_len = sample_header[11] | (sample_header[12] << 8);
      fread(&opl4_ram[ram_offset], 1, sample_len, f);
      sample_address += sample_len;
    }
    header_address += 12;
  }

  fclose(f);
  return true;
}
