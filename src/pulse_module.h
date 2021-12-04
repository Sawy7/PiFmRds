#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pulse/pulseaudio.h>
#include <sndfile.h>

void pulse_module();
void context_state_cb(pa_context *c, void *userdata);
void sink_ready_cb(pa_context *c, uint32_t idx, void *userdata);
void list_sinks_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata);
void default_sink_cb(pa_context *c, int success, void *userdata);
void list_inputs_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata);

void pulse_unload();
void context_unload_cb(pa_context *c, void *userdata);
void sink_unload_cb(pa_context *c, int success, void *userdata);