/*
    PiFmRds - FM/RDS transmitter for the Raspberry Pi
    Copyright (C) 2014 Christophe Jacquet, F8FTK
    Copyright (C) 2021 Jan Němec
    
    See https://github.com/ChristopheJacquet/PiFmRds

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

#ifndef RDS_H
#define RDS_H

#include <stdint.h>
#include "control_pipe.h"

extern void get_rds_samples(float *buffer, int count);
extern void bind_rds_history(char *filename);
extern void write_rds_history();
extern void disable_varying_ps();
extern void set_rds_pi(uint16_t pi_code);
extern void set_rds_rt(char *rt);
extern void set_rds_ps(char *ps);
extern void set_rds_ta(int ta);
extern void set_rds_pty(uint8_t pty);
extern void add_rds_af(uint8_t af);
extern void clear_rds_af();
extern uint8_t mhz_to_binary(int freq);
extern int reuse_rds_history(int dbus_mediainfo);
extern void manage_rds_startparams(struct rds_data_s *rds_data);
extern void set_history_write(int suppress);

#endif /* RDS_H */