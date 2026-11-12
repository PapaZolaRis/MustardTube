/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MT_GAME_MIXER_H
#define MT_GAME_MIXER_H
#include <sys/types.h>
/* All calls are demand-driven, synchronous and never change a device sink. */
/* Adjust returns 0=changed, 1=already at limit, <0=unsafe/unavailable. */
int mt_game_adjust(const char *app, pid_t radio_pid, int delta);
/* Radio-session GAME preference, 0..100%. Only a uniquely identified
 * game output is written; missing games simply retain the preference. */
int mt_game_mix_level(const char *app);
int mt_game_mix_set(const char *app, int percentage);
/* Called by the EXISTING event helper on a slow cadence. Returns 0 if applied,
 * 1 when no game yet, 2 when a previous game must be recovered first. */
int mt_game_mix_sync(const char *app, pid_t radio_pid);
/* Returns 2 if old game exited; its identity-verified recovery remains pending. */
int mt_game_restore(const char *app);
int mt_game_recover_once(const char *app);
int mt_game_recover_bounded(const char *app,int (*cancelled)(void));
int mt_game_diagnostic(const char *app, pid_t radio_pid);
int mt_game_session_status(const char *app);
#endif
