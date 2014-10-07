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

void myformat(int size) {
	// Do not touch or move this function
	dcreate_connect();
	/*
	3600: FILL IN CODE HERE.  YOU SHOULD INITIALIZE ANY ON-DISK
	STRUCTURES TO THEIR INITIAL VALUE, AS YOU ARE FORMATTING
	A BLANK DISK.  YOUR DISK SHOULD BE size BLOCKS IN SIZE. */

	// Set up VCB
	vcb myvcb;
	myvcb.magic = 17;
	myvcb.blocksize = BLOCKSIZE;
	myvcb.de_start = DE_START;
	myvcb.de_length = DE_LENGTH;
	myvcb.fat_start = (DE_LENGTH + 1); // Block after DE

	int numberOfBlocks = size - (1 + DE_LENGTH);
	myvcb.fat_length = numberOfBlocks / 128 + 1;
	myvcb.db_start = DE_LENGTH + myvcb.fat_length + 1; // Block after FAT

	char vcbTmp[BLOCKSIZE];
	memset(vcbTmp, 0, BLOCKSIZE);
	memcpy(vcbTmp, &myvcb, sizeof(vcb));

	// Write the VCB to Disk
	dwrite(0, vcbTmp);

	/* Next, write the DEs */

	// Create new DE
	dirent newDirectoryEntry;
	newDirectoryEntry.valid = 0;
	char deTmp[BLOCKSIZE];
	memset(deTmp, 0, BLOCKSIZE);
	memcpy(deTmp, &newDirectoryEntry, sizeof(dirent));

	// Write the Directory Entries to Disk
	for(int i = 1; i < (DE_START + DE_LENGTH); i++) {
		dwrite(i, deTmp);
	}

	/* Thirdly, write the FAT Table */
	char fatTmp[BLOCKSIZE];
	memset(fatTmp, 0, BLOCKSIZE);

	// Write FAT Table to Disk
	for(int i = 0; i < myvcb.fat_length; i++) {
		dwrite(myvcb.fat_start + i, fatTmp);
	}

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
