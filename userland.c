// piEncoder userland example code
// Copyright (C) 2014 Nash Kaminski
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of  MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
#include <sys/types.h>
#include "piEncoder.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char ** argv){
	enc_out_t data;
	double dTdt;
	int ofd = open("/dev/encoder", O_RDONLY);
	if(ofd < 0){
		perror("Device open failed");
		exit(1);
	}
	while(1){
		read(ofd,&data,sizeof(data));
		dTdt=(double)data.ticks/(data.dt / 1E9);
		printf("deltaTicks: %lu, dt: %lu, dT/dt: %f counts/sec\n",data.ticks,data.dt,dTdt);
		usleep(100000);
	}
	close(ofd);
}
