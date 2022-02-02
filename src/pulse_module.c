/*
    PiFmRds - FM/RDS transmitter for the Raspberry Pi
    Copyright (C) 2021 Jan Němec
    
    See https://github.com/ChristopheJacquet/PiFmRds
    
    rds_wav.c is a test program that writes a RDS baseband signal to a WAV
    file. It requires libsndfile.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    pulse_module.c: creates and manages a virtual sink for pulseaudio integration.
*/

#include "pulse_module.h"

pa_mainloop *pa_ml;
pa_mainloop_api *pa_mlapi;
pa_context *context;
int ret;
uint32_t module_idx;

void pulse_module()
{
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    context = pa_context_new(pa_mlapi, "pifmrds");

    // This function connects to the pulse server
    pa_context_connect(context, NULL, 0, NULL);

    // This function defines a callback so the server will tell us its state.
    pa_context_set_state_callback(context, context_state_cb, NULL);

    if (pa_mainloop_run(pa_ml, &ret) < 0) {
        printf("pa_mainloop_run() failed.");
        exit(1);
    }
}

void context_state_cb(pa_context *c, void *userdata)
{
    if (pa_context_get_state(c) == PA_CONTEXT_READY)
    {
        pa_context_load_module(c, "module-pipe-sink", "file=/tmp/pifmfifo sink_name=pifmrds rate=44100 format=s16le sink_properties=device.description=PiFmRds", sink_ready_cb, userdata);
    }
}

void sink_ready_cb(pa_context *c, uint32_t idx, void *userdata)
{
    if (idx == PA_INVALID_INDEX) {
        fprintf(stderr, "Error: invalid index in sink_ready_cb().\n") ;
        return;
    }
    module_idx = idx;
    printf("Sink created!\n");

    // // This is just to write file header (sndfile complains on read otherwise)
    // SF_INFO sf_info;
    // sf_info.samplerate = 44100;
    // sf_info.channels = 2;
    // sf_info.format = SF_FORMAT_IRCAM | SF_FORMAT_PCM_16;

    // SNDFILE *inf;
    // int modulefd = open("/tmp/pifmfifo", O_WRONLY);

    // if(! (inf = sf_open_fd(modulefd, SFM_WRITE, &sf_info, 0))) {
    //     fprintf(stderr, "Error: could not open write pipe; %s.\n", sf_strerror (inf));
    // } else {
    //     printf("Using write pipe.\n");
    // }
    // sf_close(inf);
    // close(modulefd);

    pa_context_get_sink_info_list(c, list_sinks_cb, (void*)idx);
}

void list_sinks_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    if (eol > 0) {
        return;
    }

    if (i->owner_module == (uint32_t)userdata)
    {
        pa_context_set_default_sink(c, i->name, default_sink_cb, NULL);
        pa_context_get_sink_input_info_list(c, list_inputs_cb, (void*)i->index);
    }
}

void default_sink_cb(pa_context *c, int success, void *userdata)
{
    if (success == 0)
        printf("Set sink module as default.\n");    
}

void list_inputs_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata)
{
    if (eol > 0) {
        pa_mainloop_quit(pa_ml, ret);
        return; // this may be pointless, but better safe...
    }

    pa_context_move_sink_input_by_index(c, i->index, (uint32_t)userdata, NULL, NULL);
}

void pulse_unload()
{
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    context = pa_context_new(pa_mlapi, "pifmrds");

    // This function connects to the pulse server
    pa_context_connect(context, NULL, 0, NULL);

    // This function defines a callback so the server will tell us its state.
    pa_context_set_state_callback(context, context_unload_cb, NULL);

    if (pa_mainloop_run(pa_ml, &ret) < 0) {
        printf("pa_mainloop_run() failed.");
        exit(1);
    }
}

void context_unload_cb(pa_context *c, void *userdata)
{
    if (pa_context_get_state(c) == PA_CONTEXT_READY)
    {
        pa_context_unload_module(context, module_idx, sink_unload_cb, NULL);
    }
}

void sink_unload_cb(pa_context *c, int success, void *userdata)
{
    printf("\nSink module unloaded successfully!\n");
    pa_mainloop_quit(pa_ml, ret);
}