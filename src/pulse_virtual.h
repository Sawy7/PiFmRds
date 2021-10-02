/*
    PiFmRds - FM/RDS transmitter for the Raspberry Pi
    Copyright (C) 2021 Christophe Jacquet, F8FTK, Jan Nemec
    
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pulse/pulseaudio.h>
#include <sndfile.h>
#include <pthread.h>

// From pulsecore/macro.h
#define pa_memzero(x,l) (memset((x), 0, (l)))
#define pa_zero(x) (pa_memzero(&(x), sizeof(x)))

#define	SIGPA 64

void* pulse_virtual(int *pipe);

void context_state_cb(pa_context *c, void *userdata);
void sink_ready_cb(pa_context *c, uint32_t idx, void *userdata);
void sinkinfo_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata);
void stream_state_cb(pa_stream *s, void *userdata);
void stream_read_cb(pa_stream *s, size_t length, void *userdata);
void sink_unload_cb(pa_context *c, int success, void *userdata);
void setup_cleanup();
void cleanup_handler();