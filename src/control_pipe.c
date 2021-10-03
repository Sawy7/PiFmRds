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
    
    control_pipe.c: handles command written to a non-blocking control pipe,
    in order to change RDS PS and RT at runtime.
*/


#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "rds.h"
#include "control_pipe.h"

#define CTL_BUFFER_SIZE 100

FILE *f_ctl;
char *rdsh_filename = NULL;

/*
 * Opens a file (pipe) to be used to control the RDS coder, in non-blocking mode.
 */
int open_control_pipe(char *filename) {
	int fd = open(filename, O_RDWR);

    // If the pipe doesnt exist - we create it
    if(fd == -1) {
        mode_t oldmask = umask(0000);
        mkfifo(filename, 0666);
        umask(oldmask);
        fd = open(filename, O_RDWR);
    }

	int flags;
	flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	if( fcntl(fd, F_SETFL, flags) == -1 ) return -1;

	f_ctl = fdopen(fd, "r");
	if(f_ctl == NULL) return -1;

	return 0;
}


/*
 * Polls the control file (pipe), non-blockingly, and if a command is received,
 * processes it and updates the RDS data.
 */
int poll_control_pipe() {
	static char buf[CTL_BUFFER_SIZE];

    char *res = fgets(buf, CTL_BUFFER_SIZE, f_ctl);
    if(res == NULL) return -1;
    if(strlen(res) > 3 && res[2] == ' ') {
        // save rds history to file (mainly for gui reference)
        write_rds_history(res);
        char *arg = res+3;
        if(arg[strlen(arg)-1] == '\n') arg[strlen(arg)-1] = 0;
        if(res[0] == 'P' && res[1] == 'S') {
            arg[8] = 0;
            set_rds_ps(arg);
            printf("PS set to: \"%s\"\n", arg);
            return CONTROL_PIPE_PS_SET;
        }
        if(res[0] == 'R' && res[1] == 'T') {
            arg[64] = 0;
            set_rds_rt(arg);
            printf("RT set to: \"%s\"\n", arg);
            return CONTROL_PIPE_RT_SET;
        }
        if(res[0] == 'T' && res[1] == 'A') {
            int ta = ( strcmp(arg, "ON") == 0 );
            set_rds_ta(ta);
            printf("Set TA to ");
            if(ta) printf("ON\n"); else printf("OFF\n");
            return CONTROL_PIPE_TA_SET;
        }
    }
    
    return -1;
}

/*
 * Closes the control pipe.
 */
int close_control_pipe() {
    if(f_ctl) return fclose(f_ctl);
    else return 0;
}

void create_rds_history(char *filename, char *ps, char *rt)
{
    rdsh_filename = filename;
    
    FILE* rdshistory = fopen(filename, "w");
    if (ps == NULL)
    {
        fputs("PS <Varying>", rdshistory);
    }
    else
    {
        fputs("PS ", rdshistory);
        fputs(ps, rdshistory);
    }
    fputs("\n", rdshistory);

    fputs("RT ", rdshistory);
    fputs(rt, rdshistory);
    fputs("\n", rdshistory);

    fputs("TA\n", rdshistory);
    fclose(rdshistory);
}

void write_rds_history(char *res)
{
    FILE *rdshistory = fopen(rdsh_filename, "r");
    char temp_path[strlen(rdsh_filename)+4];
    strcpy(temp_path, rdsh_filename);
    strcat(temp_path, ".tmp");
    FILE *rdstemp = fopen(temp_path, "w");
    char buf[CTL_BUFFER_SIZE];
    while (1)
    {
        if (fgets(buf, CTL_BUFFER_SIZE, rdshistory) != NULL)
        {
            if (buf[0] == res[0] && buf[1] == res[1])
            {
                fputs(res, rdstemp);
            }
            else
            {
                fputs(buf, rdstemp);
            }
        }
        else
        {
            break;
        }
    }
    fclose(rdshistory);
    fclose(rdstemp);
    remove(rdsh_filename);
    rename(temp_path, rdsh_filename);
}