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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "waveforms.h"

#define RT_LENGTH 64
#define PS_LENGTH 8
#define GROUP_LENGTH 4
#define HIST_NOT_REUSED 0
#define HIST_REUSED     1
#define HIST_VARYING    2

struct {
    uint16_t pi;
    int ta;
    char ps[PS_LENGTH];
    char rt[RT_LENGTH];
    uint8_t pty;
} rds_params = { 0 };
/* Here, the first member of the struct must be a scalar to avoid a
   warning on -Wmissing-braces with GCC < 4.8.3 
   (bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119)
*/

/* The RDS error-detection code generator polynomial is
   x^10 + x^8 + x^7 + x^5 + x^4 + x^3 + x^0
*/
#define POLY 0x1B9
#define POLY_DEG 10
#define MSB_BIT 0x8000
#define BLOCK_SIZE 16

#define BITS_PER_GROUP (GROUP_LENGTH * (BLOCK_SIZE+POLY_DEG))
#define SAMPLES_PER_BIT 192
#define FILTER_SIZE (sizeof(waveform_biphase)/sizeof(float))
#define SAMPLE_BUFFER_SIZE (SAMPLES_PER_BIT + FILTER_SIZE)


char *rdsh_filename = NULL; // RDS-history filename
int varying_ps = 1;
int suppress_write = 0;

uint16_t offset_words[] = {0x0FC, 0x198, 0x168, 0x1B4};
// We don't handle offset word C' here for the sake of simplicity

// AF (alternative frequencies)
uint8_t af_pool[25]; // AF method A (max of 25)
int af_count = 0;

// PTY
uint16_t pty_mask = 0x1F << 5;

/* Classical CRC computation */
uint16_t crc(uint16_t block) {
    uint16_t crc = 0;
    
    for(int j=0; j<BLOCK_SIZE; j++) {
        int bit = (block & MSB_BIT) != 0;
        block <<= 1;

        int msb = (crc >> (POLY_DEG-1)) & 1;
        crc <<= 1;
        if((msb ^ bit) != 0) {
            crc = crc ^ POLY;
        }
    }
    
    return crc;
}

/* Possibly generates a CT (clock time) group if the minute has just changed
   Returns 1 if the CT group was generated, 0 otherwise
*/
int get_rds_ct_group(uint16_t *blocks) {
    static int latest_minutes = -1;

    // Check time
    time_t now;
    struct tm *utc;
    
    now = time (NULL);
    utc = gmtime (&now);

    if(utc->tm_min != latest_minutes) {
        // Generate CT group
        latest_minutes = utc->tm_min;
        
        int l = utc->tm_mon <= 1 ? 1 : 0;
        int mjd = 14956 + utc->tm_mday + 
                        (int)((utc->tm_year - l) * 365.25) +
                        (int)((utc->tm_mon + 2 + l*12) * 30.6001);
        
        blocks[1] = 0x4400 | (mjd>>15);
        blocks[2] = (mjd<<1) | (utc->tm_hour>>4);
        blocks[3] = (utc->tm_hour & 0xF)<<12 | utc->tm_min<<6;
        
        utc = localtime(&now);
        
        int offset = utc->tm_gmtoff / (30 * 60);
        blocks[3] |= abs(offset);
        if(offset < 0) blocks[3] |= 0x20;
        
        //printf("Generated CT: %04X %04X %04X\n", blocks[1], blocks[2], blocks[3]);
        return 1;
    } else return 0;
}

/* Creates an RDS group. This generates sequences of the form 0A, 0A, 0A, 0A, 2A, etc.
   The pattern is of length 5, the variable 'state' keeps track of where we are in the
   pattern. 'ps_state' and 'rt_state' keep track of where we are in the PS (0A) sequence
   or RT (2A) sequence, respectively.
*/
void get_rds_group(int *buffer) {
    static int state = 0;
    static int ps_state = 0;
    static int rt_state = 0;
    static int af_state = 0;
    uint16_t blocks[GROUP_LENGTH] = {rds_params.pi, 0, 0, 0};
    
    // Generate block content
    if(! get_rds_ct_group(blocks)) { // CT (clock time) has priority on other group types
        if(state < 4) {
            blocks[1] = 0x0400 | ps_state;
            if(rds_params.ta) blocks[1] |= 0x0010;
            if (af_count > 0)
            {
                if (af_state == 0) {
                    blocks[2] = 0xE000 | (af_count << 8) | af_pool[0];
                    af_state--;
                } else if (af_state + 1 <= af_count - 1) {
                    blocks[2] = (af_pool[af_state] << 8) | af_pool[af_state+1];
                } else if (af_state == af_count - 1) {
                    blocks[2] = (af_pool[af_state] << 8) | 0xCD;
                }
                af_state += 2;
                if(af_state > af_count - 1) af_state = 0;
                // printf("0A block2: %04X\n", blocks[2]);
            } else {
                blocks[2] = 0xCDCD; // no AF
            }
            blocks[3] = rds_params.ps[ps_state*2]<<8 | rds_params.ps[ps_state*2+1];
            ps_state++;
            if(ps_state >= 4) ps_state = 0;
        } else { // state == 4 (counting from 0)
            blocks[1] = 0x2400 | rt_state;
            blocks[2] = rds_params.rt[rt_state*4+0]<<8 | rds_params.rt[rt_state*4+1];
            blocks[3] = rds_params.rt[rt_state*4+2]<<8 | rds_params.rt[rt_state*4+3];
            rt_state++;
            if(rt_state >= 16) rt_state = 0;
        }
    
        state++;
        if(state >= 5) state = 0;
    }
    blocks[1] |= rds_params.pty << 5; // Adding PTY
    // printf("block1: %04X\n", blocks[1]);
    
    // Calculate the checkword for each block and emit the bits
    for(int i=0; i<GROUP_LENGTH; i++) {
        uint16_t block = blocks[i];
        uint16_t check = crc(block) ^ offset_words[i];
        for(int j=0; j<BLOCK_SIZE; j++) {
            *buffer++ = ((block & (1<<(BLOCK_SIZE-1))) != 0);
            block <<= 1;
        }
        for(int j=0; j<POLY_DEG; j++) {
            *buffer++= ((check & (1<<(POLY_DEG-1))) != 0);
            check <<= 1;
        }
    }
}

/* Get a number of RDS samples. This generates the envelope of the waveform using
   pre-generated elementary waveform samples, and then it amplitude-modulates the 
   envelope with a 57 kHz carrier, which is very efficient as 57 kHz is 4 times the
   sample frequency we are working at (228 kHz).
 */
void get_rds_samples(float *buffer, int count) {
    static int bit_buffer[BITS_PER_GROUP];
    static int bit_pos = BITS_PER_GROUP;
    static float sample_buffer[SAMPLE_BUFFER_SIZE] = {0};
    
    static int prev_output = 0;
    static int cur_output = 0;
    static int cur_bit = 0;
    static int sample_count = SAMPLES_PER_BIT;
    static int inverting = 0;
    static int phase = 0;

    static int in_sample_index = 0;
    static int out_sample_index = SAMPLE_BUFFER_SIZE-1;
        
    for(int i=0; i<count; i++) {
        if(sample_count >= SAMPLES_PER_BIT) {
            if(bit_pos >= BITS_PER_GROUP) {
                get_rds_group(bit_buffer);
                bit_pos = 0;
            }
            
            // do differential encoding
            cur_bit = bit_buffer[bit_pos];
            prev_output = cur_output;
            cur_output = prev_output ^ cur_bit;
            
            inverting = (cur_output == 1);

            float *src = waveform_biphase;
            int idx = in_sample_index;

            for(int j=0; j<FILTER_SIZE; j++) {
                float val = (*src++);
                if(inverting) val = -val;
                sample_buffer[idx++] += val;
                if(idx >= SAMPLE_BUFFER_SIZE) idx = 0;
            }

            in_sample_index += SAMPLES_PER_BIT;
            if(in_sample_index >= SAMPLE_BUFFER_SIZE) in_sample_index -= SAMPLE_BUFFER_SIZE;
            
            bit_pos++;
            sample_count = 0;
        }
        
        float sample = sample_buffer[out_sample_index];
        sample_buffer[out_sample_index] = 0;
        out_sample_index++;
        if(out_sample_index >= SAMPLE_BUFFER_SIZE) out_sample_index = 0;
        
        
        // modulate at 57 kHz
        // use phase for this
        switch(phase) {
            case 0:
            case 2: sample = 0; break;
            case 1: break;
            case 3: sample = -sample; break;
        }
        phase++;
        if(phase >= 4) phase = 0;
        
        *buffer++ = sample;
        sample_count++;
    }
}

void bind_rds_history(char *filename) {
    rdsh_filename = filename;
}

void write_rds_history() {
    if (rdsh_filename == NULL || suppress_write)
    {
        return;
    }

    int historyfd = open(rdsh_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    char buf[10];

    // PI
    write(historyfd, "PI ", 3);
    snprintf(buf, sizeof(buf), "0x%04X\n", rds_params.pi);
    write(historyfd, buf, strlen(buf));

    if (varying_ps) write(historyfd, "PSVAR ON\n", 9);

    // PS
    write(historyfd, "PS ", 3);
    write(historyfd, rds_params.ps, PS_LENGTH);
    write(historyfd, "\n", 1);

    // RT
    write(historyfd, "RT ", 3);
    write(historyfd, rds_params.rt, RT_LENGTH);
    write(historyfd, "\n", 1);

    // PTY
    snprintf(buf, sizeof(buf), "PTY %d\n", rds_params.pty);
    write(historyfd, buf, strlen(buf));

    // TA
    if (rds_params.ta) write(historyfd, "TA ON\n", 6);

    // AFs
    if (af_count > 0)
    {
        write(historyfd, "AF ", 3);
        for (int i = 0; i < af_count; i++) {
            if (i == af_count-1) {
                snprintf(buf, sizeof(buf), "%d", af_pool[i]);
            } else {
                snprintf(buf, sizeof(buf), "%d;", af_pool[i]);
            }
            write(historyfd, buf, strlen(buf));
        }
        write(historyfd, "\n", 1);
    }
    
    close(historyfd);
}

void disable_varying_ps() {
    varying_ps = 0;
    write_rds_history();
}

void set_rds_pi(uint16_t pi_code) {
    rds_params.pi = pi_code;
    write_rds_history();
}

void set_rds_rt(char *rt) {
    strncpy(rds_params.rt, rt, 64);
    for(int i=0; i<64; i++) {
        if(rds_params.rt[i] == 0) rds_params.rt[i] = 32;
    }
    write_rds_history();
}

void set_rds_ps(char *ps) {
    strncpy(rds_params.ps, ps, 8);
    for(int i=0; i<8; i++) {
        if(rds_params.ps[i] == 0) rds_params.ps[i] = 32;
    }
    write_rds_history();
}

void set_rds_ta(int ta) {
    rds_params.ta = ta;
    write_rds_history();
}

void set_rds_pty(uint8_t pty) {
    rds_params.pty = pty;
    write_rds_history();
}

void add_rds_af(uint8_t af) { // Binary number according to AF code table
    if (af_count <= 24) {
        af_pool[af_count] = af;
        af_count++;
        if (!suppress_write) {
            write_rds_history();
        }
    } else if (af == 0) {
        fprintf(stderr, "Error: Invalid AF.\n");
    } else {
        fprintf(stderr, "Error: Cannot add more AFs. Maximum is 25.\n");
    }
}

void clear_rds_af() {
    af_count = 0;
    write_rds_history();
}

uint8_t mhz_to_binary(int freq) {
    uint8_t binary = (freq - 87500000) / 100000;
    if (binary < 1 || binary > 204)
    {
        return 0; // This AF is invalid - will fail to add
    }
    // printf("BIN: %d\n", binary);
    return binary;
}

int reuse_rds_history(int dbus_mediainfo) {
    if (rdsh_filename == NULL || access(rdsh_filename, F_OK ) != 0)
    {
        return HIST_NOT_REUSED;
    }

    int return_value = HIST_REUSED;
    FILE *rdshistory = fopen (rdsh_filename, "r");
    char res[100] = {0};
    suppress_write = 1;
    
    while (fgets(res, 100, rdshistory))
    {
        char *arg = res+3;
        if(arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
        if(res[0] == 'P' && res[1] == 'S' && res[2] == 'V') {
            return_value = HIST_VARYING;
        }
        else if(res[0] == 'P' && res[1] == 'S' && return_value != HIST_VARYING) {
            arg[8] = 0;
            varying_ps = 0;
            set_rds_ps(arg);
        }
        else if(res[0] == 'R' && res[1] == 'T') {
            if (!dbus_mediainfo)
            {
                arg[64] = 0;
                set_rds_rt(arg);
            }
        }
        else if(res[0] == 'T' && res[1] == 'A') {
            int ta = ( strcmp(arg, "ON") == 0 );
            set_rds_ta(ta);
        }
        else if(res[0] == 'P' && res[1] == 'T' && res[2] == 'Y') {
            uint8_t pty = (uint8_t) (atoi(arg+1));
            if (pty > 31) pty = 31;
            set_rds_pty(pty);
        }
        else if(res[0] == 'P' && res[1] == 'I') {
            uint16_t pi = (uint16_t) strtol(arg, NULL, 16);
            set_rds_pi(pi);
        }
        else if(res[0] == 'A' && res[1] == 'F') {
            char *af = strtok(arg, ";");
            while (af != NULL)
            {
                add_rds_af((uint8_t)atoi(af));
                af = strtok(NULL, arg);
            }
        }
    }
    suppress_write = 0;
    write_rds_history();
    return return_value;
}
