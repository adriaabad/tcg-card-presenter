#include <obs-module.h>
#include <util/platform.h>

#if __has_include(<plugin-support.h>)
#include <plugin-support.h>
#else
#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.1.0"
#endif
#endif

#include "tcg-card-dock.h"
#include "tcg-card-source.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("tcg-card-presenter", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Native OBS source for showing TCG card images with simple transitions.";
}

bool obs_module_load(void)
{
	tcg_card_source_register();
	tcg_card_dock_register();
	blog(LOG_INFO, "[tcg-card-presenter] loaded version %s", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	tcg_card_dock_unregister();
	blog(LOG_INFO, "[tcg-card-presenter] unloaded");
}
