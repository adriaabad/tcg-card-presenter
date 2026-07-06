#include "tcg-card-source.h"

#include <callback/proc.h>
#include <graphics/image-file.h>
#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

#define SETTING_SEARCH_QUERY "search_query"

enum card_phase {
	CARD_PHASE_HIDDEN,
	CARD_PHASE_ENTERING,
	CARD_PHASE_HOLDING,
	CARD_PHASE_EXITING,
};

struct card_transform {
	float opacity;
	float scale;
	float x;
	float y;
};

struct tcg_card_source {
	obs_source_t *source;
	char *card_file;
	char *enter_animation;
	char *exit_animation;
	int enter_ms;
	int hold_ms;
	int exit_ms;
	enum card_phase phase;
	float phase_ms;
	bool pending_show;
	gs_image_file_t image;
	gs_effect_t *effect;
	gs_eparam_t *image_param;
	gs_eparam_t *opacity_param;
};

static const char *tcg_text(const char *key)
{
	return obs_module_text(key);
}

static bool str_equal(const char *left, const char *right)
{
	const char *a = left ? left : "";
	const char *b = right ? right : "";
	return strcmp(a, b) == 0;
}

static float clamp01(float value)
{
	if (value < 0.0f)
		return 0.0f;
	if (value > 1.0f)
		return 1.0f;
	return value;
}

static float ease_out_cubic(float value)
{
	const float inverse = 1.0f - clamp01(value);
	return 1.0f - inverse * inverse * inverse;
}

static int read_ms(obs_data_t *settings, const char *name, int fallback, int min, int max)
{
	const int value = (int)obs_data_get_int(settings, name);
	if (value < min)
		return fallback;
	if (value > max)
		return max;
	return value;
}

static const char *read_animation(obs_data_t *settings, const char *name,
				  const char *fallback)
{
	const char *value = obs_data_get_string(settings, name);

	if (str_equal(value, TCG_CARD_ANIMATION_FADE) ||
	    str_equal(value, TCG_CARD_ANIMATION_SLIDE_LEFT) ||
	    str_equal(value, TCG_CARD_ANIMATION_SLIDE_RIGHT) ||
	    str_equal(value, TCG_CARD_ANIMATION_SLIDE_UP) ||
	    str_equal(value, TCG_CARD_ANIMATION_SLIDE_DOWN) ||
	    str_equal(value, TCG_CARD_ANIMATION_ZOOM) ||
	    str_equal(value, TCG_CARD_ANIMATION_CUT)) {
		return value;
	}

	return fallback;
}

static void set_phase(struct tcg_card_source *context, enum card_phase phase)
{
	context->phase = phase;
	context->phase_ms = 0.0f;
}

static void show_card(struct tcg_card_source *context)
{
	if (!context)
		return;

	if (!context->image.texture) {
		context->pending_show = true;
		return;
	}

	context->pending_show = false;
	set_phase(context, context->enter_ms > 0 ? CARD_PHASE_ENTERING : CARD_PHASE_HOLDING);
}

static void hide_card(struct tcg_card_source *context)
{
	if (!context || context->phase == CARD_PHASE_HIDDEN)
		return;

	set_phase(context, context->exit_ms > 0 ? CARD_PHASE_EXITING : CARD_PHASE_HIDDEN);
}

static bool show_button_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	(void)props;
	(void)property;
	show_card(data);
	return false;
}

static bool hide_button_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	(void)props;
	(void)property;
	hide_card(data);
	return false;
}

static void proc_show_card(void *data, calldata_t *params)
{
	(void)params;
	show_card(data);
}

static void proc_hide_card(void *data, calldata_t *params)
{
	(void)params;
	hide_card(data);
}

static void free_image(struct tcg_card_source *context)
{
	obs_enter_graphics();
	gs_image_file_free(&context->image);
	obs_leave_graphics();
}

static void load_image(struct tcg_card_source *context, const char *path)
{
	free_image(context);

	if (!path || !path[0])
		return;

	obs_enter_graphics();
	gs_image_file_init(&context->image, path);
	gs_image_file_init_texture(&context->image);
	obs_leave_graphics();

	if (!context->image.texture) {
		blog(LOG_WARNING, "[tcg-card-presenter] could not load image: %s", path);
	}
}

static void load_effect(struct tcg_card_source *context)
{
	char *effect_path = obs_module_file("effects/tcg-card.effect");
	char *errors = NULL;

	if (!effect_path)
		return;

	obs_enter_graphics();
	context->effect = gs_effect_create_from_file(effect_path, &errors);
	obs_leave_graphics();

	if (!context->effect) {
		blog(LOG_WARNING, "[tcg-card-presenter] could not load effect: %s",
		     errors ? errors : "unknown error");
		bfree(effect_path);
		bfree(errors);
		return;
	}

	context->image_param = gs_effect_get_param_by_name(context->effect, "image");
	context->opacity_param = gs_effect_get_param_by_name(context->effect, "opacity");

	bfree(effect_path);
	bfree(errors);
}

static const char *card_source_get_name(void *unused)
{
	(void)unused;
	return tcg_text("TCGCardPresenter");
}

static void card_source_update(void *data, obs_data_t *settings)
{
	struct tcg_card_source *context = data;
	const char *next_file = obs_data_get_string(settings, TCG_CARD_SETTING_CARD_FILE);
	const char *next_enter = read_animation(settings, TCG_CARD_SETTING_ENTER_ANIMATION,
						TCG_CARD_ANIMATION_FADE);
	const char *next_exit = read_animation(settings, TCG_CARD_SETTING_EXIT_ANIMATION,
					       TCG_CARD_ANIMATION_FADE);

	context->enter_ms = read_ms(settings, TCG_CARD_SETTING_ENTER_MS, 320, 0, 10000);
	context->hold_ms = read_ms(settings, TCG_CARD_SETTING_HOLD_MS, 3500, 0, 120000);
	context->exit_ms = read_ms(settings, TCG_CARD_SETTING_EXIT_MS, 240, 0, 10000);

	if (!str_equal(context->card_file, next_file)) {
		bfree(context->card_file);
		context->card_file = bstrdup(next_file);
		load_image(context, context->card_file);
		set_phase(context, CARD_PHASE_HIDDEN);

		if (context->pending_show && context->image.texture) {
			show_card(context);
		}
	}

	if (!str_equal(context->enter_animation, next_enter)) {
		bfree(context->enter_animation);
		context->enter_animation = bstrdup(next_enter);
	}

	if (!str_equal(context->exit_animation, next_exit)) {
		bfree(context->exit_animation);
		context->exit_animation = bstrdup(next_exit);
	}
}

static void *card_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct tcg_card_source *context = bzalloc(sizeof(*context));
	context->source = source;
	context->enter_animation = bstrdup(TCG_CARD_ANIMATION_FADE);
	context->exit_animation = bstrdup(TCG_CARD_ANIMATION_FADE);
	context->enter_ms = 320;
	context->hold_ms = 3500;
	context->exit_ms = 240;
	context->phase = CARD_PHASE_HIDDEN;

	load_effect(context);
	card_source_update(context, settings);

	proc_handler_t *handler = obs_source_get_proc_handler(source);
	proc_handler_add(handler, "void show_card()", proc_show_card, context);
	proc_handler_add(handler, "void hide_card()", proc_hide_card, context);

	return context;
}

static void card_source_destroy(void *data)
{
	struct tcg_card_source *context = data;

	if (!context)
		return;

	free_image(context);

	if (context->effect) {
		obs_enter_graphics();
		gs_effect_destroy(context->effect);
		obs_leave_graphics();
	}

	bfree(context->card_file);
	bfree(context->enter_animation);
	bfree(context->exit_animation);
	bfree(context);
}

static uint32_t card_source_get_width(void *data)
{
	struct tcg_card_source *context = data;
	return context && context->image.cx ? context->image.cx : 488;
}

static uint32_t card_source_get_height(void *data)
{
	struct tcg_card_source *context = data;
	return context && context->image.cy ? context->image.cy : 680;
}

static void add_animation_option(obs_property_t *property, const char *label_key,
				 const char *value)
{
	obs_property_list_add_string(property, tcg_text(label_key), value);
}

static obs_properties_t *card_source_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_property_t *query = obs_properties_add_text(
		props, SETTING_SEARCH_QUERY, tcg_text("SearchQuery"), OBS_TEXT_DEFAULT);
	obs_property_set_long_description(query, tcg_text("SearchQuery.Description"));

	obs_properties_add_path(props, TCG_CARD_SETTING_CARD_FILE, tcg_text("CardImage"), OBS_PATH_FILE,
				"Image files (*.png *.jpg *.jpeg *.webp *.gif);;All files (*.*)",
				NULL);

	obs_property_t *enter_ms = obs_properties_add_int_slider(
		props, TCG_CARD_SETTING_ENTER_MS, tcg_text("EnterDuration"), 0, 10000, 50);
	obs_property_int_set_suffix(enter_ms, " ms");

	obs_property_t *hold_ms = obs_properties_add_int_slider(
		props, TCG_CARD_SETTING_HOLD_MS, tcg_text("VisibleDuration"), 0, 120000, 250);
	obs_property_int_set_suffix(hold_ms, " ms");

	obs_property_t *exit_ms = obs_properties_add_int_slider(
		props, TCG_CARD_SETTING_EXIT_MS, tcg_text("ExitDuration"), 0, 10000, 50);
	obs_property_int_set_suffix(exit_ms, " ms");

	obs_property_t *enter_animation = obs_properties_add_list(
		props, TCG_CARD_SETTING_ENTER_ANIMATION, tcg_text("EnterAnimation"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	add_animation_option(enter_animation, "AnimationFade", TCG_CARD_ANIMATION_FADE);
	add_animation_option(enter_animation, "AnimationSlideLeft", TCG_CARD_ANIMATION_SLIDE_LEFT);
	add_animation_option(enter_animation, "AnimationSlideRight", TCG_CARD_ANIMATION_SLIDE_RIGHT);
	add_animation_option(enter_animation, "AnimationSlideUp", TCG_CARD_ANIMATION_SLIDE_UP);
	add_animation_option(enter_animation, "AnimationSlideDown", TCG_CARD_ANIMATION_SLIDE_DOWN);
	add_animation_option(enter_animation, "AnimationZoom", TCG_CARD_ANIMATION_ZOOM);
	add_animation_option(enter_animation, "AnimationCut", TCG_CARD_ANIMATION_CUT);

	obs_property_t *exit_animation = obs_properties_add_list(
		props, TCG_CARD_SETTING_EXIT_ANIMATION, tcg_text("ExitAnimation"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	add_animation_option(exit_animation, "AnimationFade", TCG_CARD_ANIMATION_FADE);
	add_animation_option(exit_animation, "AnimationSlideLeft", TCG_CARD_ANIMATION_SLIDE_LEFT);
	add_animation_option(exit_animation, "AnimationSlideRight", TCG_CARD_ANIMATION_SLIDE_RIGHT);
	add_animation_option(exit_animation, "AnimationSlideUp", TCG_CARD_ANIMATION_SLIDE_UP);
	add_animation_option(exit_animation, "AnimationSlideDown", TCG_CARD_ANIMATION_SLIDE_DOWN);
	add_animation_option(exit_animation, "AnimationZoom", TCG_CARD_ANIMATION_ZOOM);
	add_animation_option(exit_animation, "AnimationCut", TCG_CARD_ANIMATION_CUT);

	obs_properties_add_button2(props, "show_card", tcg_text("ShowCard"), show_button_clicked,
				   data);
	obs_properties_add_button2(props, "hide_card", tcg_text("HideCard"), hide_button_clicked,
				   data);

	return props;
}

static void card_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, TCG_CARD_SETTING_CARD_FILE, "");
	obs_data_set_default_string(settings, SETTING_SEARCH_QUERY, "");
	obs_data_set_default_int(settings, TCG_CARD_SETTING_ENTER_MS, 320);
	obs_data_set_default_int(settings, TCG_CARD_SETTING_HOLD_MS, 3500);
	obs_data_set_default_int(settings, TCG_CARD_SETTING_EXIT_MS, 240);
	obs_data_set_default_string(settings, TCG_CARD_SETTING_ENTER_ANIMATION,
				    TCG_CARD_ANIMATION_FADE);
	obs_data_set_default_string(settings, TCG_CARD_SETTING_EXIT_ANIMATION,
				    TCG_CARD_ANIMATION_FADE);
}

static float progress_for_phase(struct tcg_card_source *context, int duration_ms)
{
	if (duration_ms <= 0)
		return 1.0f;

	return clamp01(context->phase_ms / (float)duration_ms);
}

static void apply_named_animation(struct card_transform *transform, const char *animation,
				  float amount, float width, float height)
{
	if (str_equal(animation, TCG_CARD_ANIMATION_CUT)) {
		transform->opacity = amount > 0.0f ? 1.0f : 0.0f;
		return;
	}

	if (str_equal(animation, TCG_CARD_ANIMATION_FADE)) {
		transform->opacity = amount;
		return;
	}

	if (str_equal(animation, TCG_CARD_ANIMATION_ZOOM)) {
		transform->opacity = amount;
		transform->scale = 0.86f + 0.14f * amount;
		return;
	}

	transform->opacity = amount;

	if (str_equal(animation, TCG_CARD_ANIMATION_SLIDE_LEFT))
		transform->x = -width * (1.0f - amount);
	else if (str_equal(animation, TCG_CARD_ANIMATION_SLIDE_RIGHT))
		transform->x = width * (1.0f - amount);
	else if (str_equal(animation, TCG_CARD_ANIMATION_SLIDE_UP))
		transform->y = -height * (1.0f - amount);
	else if (str_equal(animation, TCG_CARD_ANIMATION_SLIDE_DOWN))
		transform->y = height * (1.0f - amount);
}

static struct card_transform render_transform(struct tcg_card_source *context)
{
	struct card_transform transform = {
		.opacity = 1.0f,
		.scale = 1.0f,
		.x = 0.0f,
		.y = 0.0f,
	};

	const float width = (float)card_source_get_width(context);
	const float height = (float)card_source_get_height(context);

	if (context->phase == CARD_PHASE_ENTERING) {
		const float progress = ease_out_cubic(progress_for_phase(context, context->enter_ms));
		apply_named_animation(&transform, context->enter_animation, progress, width, height);
	} else if (context->phase == CARD_PHASE_EXITING) {
		const float progress = ease_out_cubic(progress_for_phase(context, context->exit_ms));
		apply_named_animation(&transform, context->exit_animation, 1.0f - progress, width,
				      height);
	}

	return transform;
}

static void advance_phase(struct tcg_card_source *context)
{
	if (context->phase == CARD_PHASE_ENTERING && context->phase_ms >= context->enter_ms) {
		set_phase(context, CARD_PHASE_HOLDING);
		return;
	}

	if (context->phase == CARD_PHASE_HOLDING && context->hold_ms > 0 &&
	    context->phase_ms >= context->hold_ms) {
		set_phase(context, context->exit_ms > 0 ? CARD_PHASE_EXITING : CARD_PHASE_HIDDEN);
		return;
	}

	if (context->phase == CARD_PHASE_EXITING && context->phase_ms >= context->exit_ms) {
		set_phase(context, CARD_PHASE_HIDDEN);
	}
}

static void card_source_tick(void *data, float seconds)
{
	struct tcg_card_source *context = data;
	const uint64_t elapsed_ns = (uint64_t)(seconds * 1000000000.0f);

	if (context->image.texture && gs_image_file_tick(&context->image, elapsed_ns)) {
		obs_enter_graphics();
		gs_image_file_update_texture(&context->image);
		obs_leave_graphics();
	}

	if (context->pending_show && context->image.texture) {
		show_card(context);
	}

	if (context->phase == CARD_PHASE_HIDDEN)
		return;

	context->phase_ms += seconds * 1000.0f;
	advance_phase(context);
}

static void card_source_render(void *data, gs_effect_t *unused_effect)
{
	struct tcg_card_source *context = data;
	struct card_transform transform;
	const uint32_t width = card_source_get_width(context);
	const uint32_t height = card_source_get_height(context);
	gs_effect_t *effect = context->effect ? context->effect : obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image_param;
	const char *technique = context->effect ? "Draw" : "Draw";
	(void)unused_effect;

	if (!context->image.texture || context->phase == CARD_PHASE_HIDDEN || !effect)
		return;

	transform = render_transform(context);
	if (transform.opacity <= 0.001f)
		return;

	image_param = context->image_param ? context->image_param
					   : gs_effect_get_param_by_name(effect, "image");
	if (image_param)
		gs_effect_set_texture(image_param, context->image.texture);

	if (context->opacity_param)
		gs_effect_set_float(context->opacity_param, transform.opacity);

	gs_matrix_push();
	gs_matrix_translate3f(transform.x + ((float)width * (1.0f - transform.scale) * 0.5f),
			      transform.y + ((float)height * (1.0f - transform.scale) * 0.5f),
			      0.0f);
	gs_matrix_scale3f(transform.scale, transform.scale, 1.0f);

	while (gs_effect_loop(effect, technique)) {
		gs_draw_sprite(context->image.texture, 0, width, height);
	}

	gs_matrix_pop();
}

static struct obs_source_info tcg_card_source_info = {
	.id = TCG_CARD_SOURCE_ID,
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = card_source_get_name,
	.create = card_source_create,
	.destroy = card_source_destroy,
	.get_width = card_source_get_width,
	.get_height = card_source_get_height,
	.get_defaults = card_source_defaults,
	.get_properties = card_source_properties,
	.update = card_source_update,
	.video_tick = card_source_tick,
	.video_render = card_source_render,
};

void tcg_card_source_register(void)
{
	obs_register_source(&tcg_card_source_info);
}
