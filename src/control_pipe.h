/*
    PiFmRds - FM/RDS transmitter for the Raspberry Pi
    Copyright (C) 2014 Christophe Jacquet, F8FTK
    
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

#ifndef CTL_H
#define CTL_H

#define CONTROL_PIPE_PS_SET     1
#define CONTROL_PIPE_RT_SET     2
#define CONTROL_PIPE_TA_SET     3
#define CONTROL_PIPE_AF_ADDED   4
#define CONTROL_PIPE_AF_CLEARED 5
#define CONTROL_PIPE_PI_CHANGED 6

struct rds_data_s
{
    // Programme ID
    uint16_t pi;
    // Programme Service Name
    char *ps;
    // Variable PS
    int ps_var;
    // Radio Text
    char *rt;
    // Programme Type
    uint8_t pty;
    // Alternative frequencies
    uint8_t af_pool[25];
    int af_count;
    // Start-rds (in arguments)
    int rds_set;
};

extern int open_control_pipe(char *filename);
extern int close_control_pipe();
extern int poll_control_pipe(int dbus_mediainfo);

// void create_rds_history(char *filename, struct rds_data_s *rds_data);
// void write_rds_history(char *res);

#endif /* CTL_H */