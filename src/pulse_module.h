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
*/

#ifndef PULSE_H
#define PULSE_H

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

#endif /* PULSE_H */