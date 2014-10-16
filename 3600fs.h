/*
 * CS3600, Spring 2014
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600FS_H__
#define __3600FS_H__

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
	unsigned int valid; // 1 means taken, 0 means free
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

// Fatent Data Structure
typedef struct fatent_s {
	unsigned int used:1;
	unsigned int eof:1;
	unsigned int next:30;
} fatent;

int findNextAvailableFatent();

int findNextAvailableDirEnt();

int findDEBlock(const char* path);

int validPath(const char* path);

#endif
