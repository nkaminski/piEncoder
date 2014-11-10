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
