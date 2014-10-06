/*
 * CS3600, Spring 2014
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This program is intended to format your disk file, and should be executed
 * BEFORE any attempt is made to mount your file system.  It will not, however
 * be called before every mount (you will call it manually when you format 
 * your disk file).
 */

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "3600fs.h"
#include "disk.h"

#define DE_START 1 // Starts after VCB
#define DE_LENGTH 100 // Number of file entries

// VCB Data Structure
typedef struct vcb_s {
	// Magic number to identify our disk
	int magic;

	// description of the disk layout
	int blocksize;
	int de_start;
	int de_length;
	int fat_start;
	int fat_length;
	int db_start;

	//metadata for the root directory
	uid_t user;
	gid_t group;
	mode_t mode;
	struct timespec access_time;
	struct timespec modify_time;
	struct timespec create_time;
} vcb;

// Dirent Data Structure
typedef struct dirent_s {
	unsigned int valid;
	unsigned int first_block;
	unsigned int size;

	uid_t user;
	uid_t group;
	mode_t mode;
	struct timespec access_time;
	struct timespec modify_time;
	struct timespec create_time;

	char name[440]; // Makes the total size 512 bytes
} dirent;

void myformat(int size) {
	// Do not touch or move this function
	dcreate_connect();
	/*
	3600: FILL IN CODE HERE.  YOU SHOULD INITIALIZE ANY ON-DISK
	STRUCTURES TO THEIR INITIAL VALUE, AS YOU ARE FORMATTING
	A BLANK DISK.  YOUR DISK SHOULD BE size BLOCKS IN SIZE. */

	// Set up your vcb
	vcb myvcb;
	myvcb.magic = 17;
	myvcb.blocksize = BLOCKSIZE;
	myvcb.de_start = DE_START;
	myvcb.de_length = DE_LENGTH;
	myvcb.fat_start = (DE_LENGTH + 1);

	// myvcb.fat_length
	// myvcb.db_start

	char tmp[BLOCKSIZE];
	memset(tmp, 0, BLOCKSIZE);
	memcpy(tmp, &myvcb, sizeof(vcb));

	/* 3600: AN EXAMPLE OF READING/WRITING TO THE DISK IS BELOW - YOU'LL
					 WANT TO REPLACE THE CODE BELOW WITH SOMETHING MEANINGFUL. */

	// first, create a zero-ed out array of memory  
	char *temp = (char *) malloc(BLOCKSIZE);
	memset(temp, 0, BLOCKSIZE);

	// now, write that to every block
	for (int i=0; i<size; i++) 
		if (dwrite(i, temp) < 0) 
			perror("Error while writing to disk");

	// voila! we now have a disk containing all zeros

	// Do not touch or move this function
	dunconnect();
}

int main(int argc, char** argv) {
	// Do not touch this function
	if (argc != 2) {
		printf("Invalid number of arguments \n");
		printf("usage: %s diskSizeInBlockSize\n", argv[0]);
		return 1;
	}

	unsigned long size = atoi(argv[1]);
	printf("Formatting the disk with size %lu \n", size);
	myformat(size);
}
