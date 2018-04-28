/*
	Chad Coates
	ECE 373
	Homework #8
	June 4, 2017

	This is the reader/writer for the acme_pci driver
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct ring_info{
	int head;
	int tail;
	uint32_t icr;
	uint32_t rh;
	uint32_t rl;
	uint32_t len;
	uint32_t led;
};

int acme_error(int);
int reader(int,char *);
int writer(int,uint32_t,char *);

int main(int argc, char *argv[]){
	char *file="/dev/ece_led";
	int fd=open(file,O_RDWR);

	if(fd<0){
		fprintf(stderr,"open failed: %s\n",file);
		exit(1);
	}

	for(;;){
		reader(fd,file);
		sleep(2);
	}

	close(fd);
	exit(0);
}

int acme_error(int fd){
	fprintf(stderr,"Error, cowardly refusing to proceed!\n");
	close(fd);
	exit(1);
}

int reader(int fd,char *file){
	struct ring_info info;
	if(read(fd,&info,sizeof(struct ring_info))<0){
		fprintf(stderr,"read failed!: %s\n",file);
		exit(1);
	}
	fprintf(stderr, "LED: 0x%08x, Head: %i, Tail: %i\n",info.led,info.head,info.tail);
	return 0;
}

