#ifndef LIBMOONSOUND_H
#define LIBMOONSOUND_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MSContext MSContext;

MSContext *ms_create(void);
void ms_destroy(MSContext *ctx);

int ms_load_mwm_file(MSContext *ctx, const char *mwm_path);
int ms_load_mwk_file(MSContext *ctx, const char *mwk_path);
int ms_load_rom_file(MSContext *ctx, const char *rom_path);
int ms_load_waves_file(MSContext *ctx, const char *waves_path);

void ms_set_seconds_limit(MSContext *ctx, int seconds);
void ms_clear_seconds_limit(MSContext *ctx);
void ms_set_loop_count(MSContext *ctx, int loops);

bool ms_supports_loop(MSContext *ctx);
bool ms_requires_mwk(MSContext *ctx);

uint32_t ms_calculate_length_samples(MSContext *ctx, int loops);
uint32_t ms_get_total_samples(MSContext *ctx);

int ms_prepare(MSContext *ctx);
uint32_t ms_render(MSContext *ctx, int16_t *out_interleaved,
                   uint32_t frames);
void ms_stop(MSContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
