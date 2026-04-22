#include "mwm_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool read_exact(FILE *f, void *dst, size_t bytes) {
  return fread(dst, 1, bytes, f) == bytes;
}

static uint16_t read_u16_le(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

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

  uint8_t full_position_table[MAX_POSITION + 1];
  memset(full_position_table, 0, sizeof(full_position_table));

  if (edit_mode) {
    if (!read_exact(f, full_position_table, MAX_POSITION + 1) ||
        !read_exact(f, &song->header, sizeof(MWM_HEADER))) {
      fclose(f);
      return false;
    }
  } else {
    if (!read_exact(f, &song->header, sizeof(MWM_HEADER))) {
      fclose(f);
      return false;
    }
    if (!read_exact(f, full_position_table, song->header.song_length + 1)) {
      fclose(f);
      return false;
    }
  }

  memcpy(song->position_table, full_position_table, song->header.song_length + 1);

  uint8_t max_pos = 0;
  for (int i = 0; i <= song->header.song_length; i++) {
    if (song->position_table[i] > max_pos)
      max_pos = song->position_table[i];
  }
  song->max_pattern = max_pos;

  uint16_t *pattern_offsets = (uint16_t *)malloc(sizeof(uint16_t) * (max_pos + 1));
  if (!pattern_offsets) {
    fclose(f);
    return false;
  }
  for (int i = 0; i <= max_pos; i++) {
    uint8_t raw[2];
    if (!read_exact(f, raw, sizeof(raw))) {
      free(pattern_offsets);
      fclose(f);
      return false;
    }
    pattern_offsets[i] = read_u16_le(raw);
  }

  song->patterns = calloc(max_pos + 1, sizeof(uint8_t *));
  if (!song->patterns) {
    free(pattern_offsets);
    fclose(f);
    return false;
  }

  MWM_PATTERN_HEADER ph;
  int pattern_count = 0;
  while (pattern_count <= max_pos) {
    if (!read_exact(f, &ph, sizeof(ph)))
      break;
    if (ph.nr_of_patterns == 0)
      break;

    uint8_t *chunk_data = (uint8_t *)malloc(ph.size);
    if (!chunk_data || !read_exact(f, chunk_data, ph.size)) {
      free(chunk_data);
      free(pattern_offsets);
      fclose(f);
      return false;
    }

    uint16_t base_offset = pattern_offsets[pattern_count];

    for (int j = 0; j < ph.nr_of_patterns && (pattern_count + j) <= max_pos;
         j++) {
      uint16_t current_offset = pattern_offsets[pattern_count + j];
      int relative_offset = current_offset - base_offset;
      if (relative_offset >= 0 && relative_offset < ph.size) {
        int pattern_len;
        if (j < ph.nr_of_patterns - 1) {
          pattern_len = pattern_offsets[pattern_count + j + 1] - current_offset;
        } else {
          pattern_len = ph.size - relative_offset;
        }
        if (pattern_len > 0) {
          song->patterns[pattern_count + j] = (uint8_t *)malloc(pattern_len);
          if (song->patterns[pattern_count + j]) {
            memcpy(song->patterns[pattern_count + j], chunk_data + relative_offset,
                   (size_t)pattern_len);
          }
        }
      }
    }

    pattern_count += ph.nr_of_patterns;
    free(chunk_data);
  }

  free(pattern_offsets);

  char xlfo_sig[4];
  if (read_exact(f, xlfo_sig, sizeof(xlfo_sig))) {
    if (strncmp(xlfo_sig, "XLFO", 4) == 0) {
      read_exact(f, song->xlfo, 18);
      song->has_xlfo = true;
    }
  }

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
  if (!read_exact(f, size_bytes, sizeof(size_bytes))) {
    fclose(f);
    return false;
  }
  kit->total_sample_size =
      size_bytes[0] | (size_bytes[1] << 8) | (size_bytes[2] << 16);
  if (!read_exact(f, &kit->nr_of_waves, 1) ||
      !read_exact(f, kit->own_tone_info, MAX_OWN_TONES)) {
    fclose(f);
    return false;
  }
  if (kit->nr_of_waves > MAX_OWN_PATCHES) {
    fclose(f);
    return false;
  }
  if (!read_exact(f, kit->own_patches, sizeof(OWN_PATCH) * kit->nr_of_waves)) {
    fclose(f);
    return false;
  }

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
      if (!read_exact(f, sample_header, sizeof(sample_header))) {
        fclose(f);
        return false;
      }

      uint8_t *ram_header = &opl4_ram[header_address];
      if (kit->own_tone_info[i] & 0x20) {
        ram_header[0] = sample_header[12] | (kit->own_tone_info[i] & 0xC0);
      } else {
        // Byte 0: Bit 7-6 = Bits/sample, Bit 5-0 = Address bits 21-16
        ram_header[0] =
            ((sample_address >> 16) & 0x3F) | (kit->own_tone_info[i] & 0xC0);
      }
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

      if (!(kit->own_tone_info[i] & 0x20)) {
        // Load sample data into RAM at offset relative to 0x200000
        uint32_t ram_offset = sample_address - 0x200000;
        uint16_t sample_len = sample_header[11] | (sample_header[12] << 8);
        if (!read_exact(f, &opl4_ram[ram_offset], sample_len)) {
          fclose(f);
          return false;
        }
        sample_address += sample_len;
      }
    }
    header_address += 12;
  }

  fclose(f);
  return true;
}
