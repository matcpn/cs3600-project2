/*
 * CS3600, Spring 2014
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This file contains all of the basic functions that you will need 
 * to implement for this project.  Please see the project handout
 * for more details on any particular function, and ask on Piazza if
 * you get stuck.
 */

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

#include <time.h>
#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

// GLOBAL VARIABLES
vcb myRealVCB;
dirent allTheDirEnts[100];
fatent *fatTable;

/*
 * Initialize filesystem. Read in file system metadata and initialize
 * memory structures. If there are inconsistencies, now would also be
 * a good time to deal with that. 
 *
 * HINT: You don't need to deal with the 'conn' parameter AND you may
 * just return NULL.
 *
 */
static void* vfs_mount(struct fuse_conn_info *conn) {
	fprintf(stderr, "vfs_mount called\n");

	// Do not touch or move this code; connects the disk
	dconnect();

	/* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
					 AND LOAD ANY DATA STRUCTURES INTO MEMORY */

	// Temporary BLOCKSIZE-d location
	char tmp[BLOCKSIZE];
	memset(tmp, 0, BLOCKSIZE);

	// Read the VCB from disk
	dread(0, tmp);

	// Copy into VCB structure
	memcpy(&myRealVCB, tmp, sizeof(vcb));

	// Check the magic number
	if (myRealVCB.magic != 17) {
		fprintf(stderr, "This is not the right filesystem, give up.");
	}

	// Read the DirEnts from Disk
	char deTmp[BLOCKSIZE];
	for(int i = 0; i < 100; i++) {
		memset(deTmp, 0, BLOCKSIZE);
		dread(i + 1, deTmp);
		memcpy(&allTheDirEnts[i], deTmp, sizeof(dirent)); // Copy all dirents into their array so we can access them
	}

	// Create the FAT Table
	fatTable = (fatent *) malloc(myRealVCB.fat_length * BLOCKSIZE);
	for(int i = 0; i < myRealVCB.fat_length; i++) {
		char buf[BLOCKSIZE];
		memset(buf, 0, BLOCKSIZE);
		dread(myRealVCB.fat_start + i, buf);
		memcpy(fatTable + 128 * i, buf, BLOCKSIZE);
	}

	return NULL;
}

/*
 * Called when your file system is unmounted.
 
 */
static void vfs_unmount (void *private_data) {
	fprintf(stderr, "vfs_unmount called\n");

	/* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
					 ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
					 KEEP DATA CACHED THAT'S NOT ON DISK */
	
	// Write the DEs to Disk, should be its own function
	for(int i = 0; i < 100; i++) {
		char buffer[BLOCKSIZE];
		memset(buffer, 0, BLOCKSIZE);
		memcpy(buffer, &allTheDirEnts[i], sizeof(dirent));
		dwrite(i + 1, buffer);
	}
	// Save the FatTable to Disk
	for(int i = 0; i < myRealVCB.fat_length; i++) {
		char buffer[BLOCKSIZE];
		memcpy(buffer, fatTable + 128 * i, BLOCKSIZE);
		dwrite(myRealVCB.fat_start + i, buffer);
	}

	free(fatTable);
	// Do not touch or move this code; unconnects the disk
	dunconnect();
}

/* 
 *
 * Given an absolute path to a file/directory (i.e., /foo ---all
 * paths will start with the root directory of the CS3600 file
 * system, "/"), you need to return the file attributes that is
 * similar stat system call.
 *
 * HINT: You must implement stbuf->stmode, stbuf->st_size, and
 * stbuf->st_blocks correctly.
 *
 */
static int vfs_getattr(const char *path, struct stat *stbuf) {
	fprintf(stderr, "vfs_getattr called\n");

	// Do not mess with this code 
	stbuf->st_nlink = 1; // hard links
	stbuf->st_rdev  = 0;
	stbuf->st_blksize = BLOCKSIZE;

	/* 3600: YOU MUST UNCOMMENT BELOW AND IMPLEMENT THIS CORRECTLY */
	
	
	// Root directory since it's info is in the VCB
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = 0777 | S_IFDIR;
		stbuf->st_uid = getuid();
		stbuf->st_gid =  getgid();
		return 0;
	}
		
	// Any other file with a dirent
	for(int i = 0; i < 100; i++) {
		// If the name matches and is valid (has things written to it)
		if ((strcmp(path, allTheDirEnts[i].name) == 0) && (allTheDirEnts[i].valid == 1)) {
			// Found the file
			stbuf->st_uid = allTheDirEnts[i].user;
			stbuf->st_gid = allTheDirEnts[i].group;
			stbuf->st_mode = allTheDirEnts[i].mode;

			stbuf->st_atime = allTheDirEnts[i].access_time.tv_sec;
			stbuf->st_mtime = allTheDirEnts[i].modify_time.tv_sec;
			stbuf->st_ctime = allTheDirEnts[i].create_time.tv_sec;
			stbuf->st_size = allTheDirEnts[i].size;

			stbuf->st_blocks = ((allTheDirEnts[i].size / BLOCKSIZE) + 1);
			return 0;
		}
	}		

	return -ENOENT;
}

/*
 * Given an absolute path to a directory (which may or may not end in
 * '/'), vfs_mkdir will create a new directory named dirname in that
 * directory, and will create it with the specified initial mode.
 *
 * HINT: Don't forget to create . and .. while creating a
 * directory.
 */
/*
 * NOTE: YOU CAN IGNORE THIS METHOD, UNLESS YOU ARE COMPLETING THE 
 *       EXTRA CREDIT PORTION OF THE PROJECT.  IF SO, YOU SHOULD
 *       UN-COMMENT THIS METHOD.
static int vfs_mkdir(const char *path, mode_t mode) {

	return -1;
} */

/** Read directory
 *
 * Given an absolute path to a directory, vfs_readdir will return 
 * all the files and directories in that directory.
 *
 * HINT:
 * Use the filler parameter to fill in, look at fusexmp.c to see an example
 * Prototype below
 *
 * Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *                                 const struct stat *stbuf, off_t off);
 *			   
 * Your solution should not need to touch fi
 *
 */
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
											 off_t offset, struct fuse_file_info *fi)
{
		// Only do anything if in root since we're not supporting more directories
		if (strcmp(path, "/") == 0) {
				// Generic stat object to do work with

				// Loop through dirents and get valid stat info
				filler(buf, ".", 0, 0);
				filler(buf, "..", 0, 0);
				for(int i = offset; i < 100; i++) {
					// If the dirent is valid (has things written to it)
					struct stat stat_struct;
					if (allTheDirEnts[i].valid == 1) {
						// Copy things to the stat
						stat_struct.st_uid = allTheDirEnts[i].user;
						stat_struct.st_gid = allTheDirEnts[i].group;
						stat_struct.st_mode = allTheDirEnts[i].mode;
						filler(buf, allTheDirEnts[i].name, &stat_struct, 0);
					}
				}
		}
		else {
			return -1;
		}

		return 0;
		
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
		// Allocate and initialize a new file entry -> 0
			// Make a new DE
			// Fill in name, set valid to 1, set uid,gid,mode
			// access/modify/create to now
			// IMPORTANT first_block just the number of the fatent that starts this
			// Fatent needs to be instantiated.
			// Size is still 0 right now.
			// Make sure we write the dirent

		// Check that the path is valid OR file already exists -> -1
		if ((validPath(path) == 1) || (findDEBlock(path) < 0)) {
			// Create a new DE
			dirent dir;

			// Assign dir things
			dir.valid = 1;
			dir.size = 0;
			dir.user = getuid();
			dir.group = getgid();
			dir.mode = mode;

			// Assign times
			clock_gettime(CLOCK_REALTIME, dir.access_time);
			clock_gettime(CLOCK_REALTIME, dir.modify_time);
			clock_gettime(CLOCK_REALTIME, dir.create_time);

			// Create a new Fatent
			fatent fe = findNextAvailableFatent();
		}
		else {
			// The path wasn't valid
			return -1;
		}

		return 0;
}

/*
 * The function vfs_read provides the ability to read data from 
 * an absolute path 'path,' which should specify an existing file.
 * It will attempt to read 'size' bytes starting at the specified
 * offset (offset) from the specified file (path)
 * on your filesystem into the memory address 'buf'. The return 
 * value is the amount of bytes actually read; if the file is 
 * smaller than size, vfs_read will simply return the most amount
 * of bytes it could read. 
 *
 * HINT: You should be able to ignore 'fi'
 *
 */
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
										struct fuse_file_info *fi)
{

		return 0;
}

/*
 * The function vfs_write will attempt to write 'size' bytes from 
 * memory address 'buf' into a file specified by an absolute 'path'.
 * It should do so starting at the specified offset 'offset'.  If
 * offset is beyond the current size of the file, you should pad the
 * file with 0s until you reach the appropriate length.
 *
 * You should return the number of bytes written.
 *
 * HINT: Ignore 'fi'
 */
static int vfs_write(const char *path, const char *buf, size_t size,
										 off_t offset, struct fuse_file_info *fi)
{

	/* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
					 MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */

	return 0;
}

/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{

	/* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
					 AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */

		return 0;
}

/*
 * The function rename will rename a file or directory named by the
 * string 'oldpath' and rename it to the file name specified by 'newpath'.
 *
 * HINT: Renaming could also be moving in disguise
 *
 */
static int vfs_rename(const char *from, const char *to)
{

		return 0;
}


/*
 * This function will change the permissions on the file
 * to be mode.  This should only update the file's mode.  
 * Only the permission bits of mode should be examined 
 * (basically, the last 16 bits).  You should do something like
 * 
 * fcb->mode = (mode & 0x0000ffff);
 *
 */
static int vfs_chmod(const char *file, mode_t mode)
{

		return 0;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{

		return 0;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{

		return 0;
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{

	/* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
					 BE AVAILABLE FOR OTHER FILES TO USE. */

		return 0;
}
int findNextAvailableFatent() {
	for(int i = 0; i < )
}

int findDEBlock(char *path) {
	// Finds the DE block # given the path
	for(int i = 0; i < 100; i++) {
		if (strcmp(allTheDirEnts[i].name, path)) {
			return i;
		}
	}

	return -1;
}

int validPath(char *path) {
	// Count the /'s
	// If there's more than 1, give up.
	int pathLength = strlen(path);
	int counter = 0;
	for(int i = 0; i < pathLength; i++) {
		counter += strcmp(path[i], "/");
	}

	return counter;
}

/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     .mkdir	 = vfs_mkdir,
 */
static struct fuse_operations vfs_oper = {
		.init    = vfs_mount,
		.destroy = vfs_unmount,
		.getattr = vfs_getattr,
		.readdir = vfs_readdir,
		.create	 = vfs_create,
		.read	 = vfs_read,
		.write	 = vfs_write,
		.unlink	 = vfs_delete,
		.rename	 = vfs_rename,
		.chmod	 = vfs_chmod,
		.chown	 = vfs_chown,
		.utimens	 = vfs_utimens,
		.truncate	 = vfs_truncate,
};

int main(int argc, char *argv[]) {
		/* Do not modify this function */
		umask(0);
		if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
			printf("Usage: ./3600fs -s -d <dir>\n");
			exit(-1);
		}
		return fuse_main(argc, argv, &vfs_oper, NULL);
}

