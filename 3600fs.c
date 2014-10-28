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
int numberOfFatEnts;
int firstDataBlock;

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

	numberOfFatEnts = myRealVCB.fat_length;
	firstDataBlock = myRealVCB.db_start;

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
						filler(buf, (allTheDirEnts[i].name + 1), &stat_struct, 0);
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
		if(findDEBlock(path) > 0) {
			return -EEXIST;
		}
		// Check that the path is valid
		if (validPath(path) == 1) {
			int dir = findNextAvailableDirEnt();

			// Assign dir things
			allTheDirEnts[dir].valid = 1;
			allTheDirEnts[dir].size = 0;
			allTheDirEnts[dir].user = getuid();
			allTheDirEnts[dir].group = getgid();
			allTheDirEnts[dir].mode = mode;
			strcpy(allTheDirEnts[dir].name, path);

			// Assign times
			clock_gettime(CLOCK_REALTIME, &allTheDirEnts[dir].access_time);
			clock_gettime(CLOCK_REALTIME, &allTheDirEnts[dir].modify_time);
			clock_gettime(CLOCK_REALTIME, &allTheDirEnts[dir].create_time);

			// Create a new Fatent and get it's #
			int fe = findNextAvailableFatent();

			if(fe == -1) {
				return -ENOSPC; // Out of space
			}

			// Assign first_block of the dirent
			allTheDirEnts[dir].first_block = fe;
			// Write the dirent
			char deTmp[BLOCKSIZE];
			memset(deTmp, 0, BLOCKSIZE);
			memcpy(deTmp, &allTheDirEnts[dir], sizeof(dirent));
			dwrite(dir + 1, deTmp); // Add 1 so it's 1-100, 0 is VCB
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
	for (int i = 0; i < 100; i++) {
	  if (strcmp(allTheDirEnts[i].name, path) == 0 && allTheDirEnts[i].valid == 1) {
	    //Dont try to read something that starts past the end of the file please
	    if (offset >= allTheDirEnts[i].size) {
	    	return -1;
	    }
	    //Just for convenience
	    int fileLength = allTheDirEnts[i].size;
	    //apparently size is unsigned and the compiler doesn't like that. 
	    int signedSize = size;

	    //Find out which block the byte is in
	    int startingBlock = offset / BLOCKSIZE;

		//check if size is going to read past the end of the file. If it is, make sure it doesn't do that.
		if (fileLength < offset + signedSize) {
			signedSize = fileLength - offset;
		}

		//Find out where the read ends (after adjusting the size of the read)
		int endingBlock = (offset + signedSize)/BLOCKSIZE;

		//We can finally start doing things
		//Step 1 is to find the block in the fat table
		int blockNumber = 0;
		int currentBlock = allTheDirEnts[i].first_block;
		while (!fatTable[currentBlock].eof) {
			//If you've found the starting block, thats great, you're done
			if (blockNumber == startingBlock) {
				break;
			}
			//Otherwise keep going until you find it
			currentBlock = fatTable[currentBlock].next;
			blockNumber++;
		}

		int numberOfBytesRead = 0; //just keep track of the number of bytes we've read
		int numberOfBytesRemaining = signedSize; //and keep track of how many bytes are left

		//check if we're going to be reading more than one block
		if (offset % BLOCKSIZE + signedSize > BLOCKSIZE) {
			//so lets start reading blocks
			char tmp[BLOCKSIZE];
			memset(tmp, 0, BLOCKSIZE);
			dread(firstDataBlock + currentBlock, tmp);
			memcpy(buf, tmp + offset % BLOCKSIZE, BLOCKSIZE - offset % BLOCKSIZE);

			//update the number of bytes we've read
			numberOfBytesRead = BLOCKSIZE - offset % BLOCKSIZE;
			//change the current block to be the next one and increment the counter
			currentBlock = fatTable[currentBlock].next;
			blockNumber++;
			//update the number of bytes left to read
			numberOfBytesRemaining = numberOfBytesRemaining - numberOfBytesRead;

			//now that we've read the first block, the rest are a bit easier, since you just read the entire block (unless its the last one)
			while(blockNumber <= endingBlock) {
				if (fatTable[currentBlock].used != 1) {
					//we cant read from a block that isn't used, so dont tell us to
					return -1;
				}

				if (numberOfBytesRemaining >= BLOCKSIZE) {
					//if you're reading more than blocksize, then you can just read all of currentblock and iterate again
					memset(tmp, 0, BLOCKSIZE);
					dread(firstDataBlock + currentBlock, tmp);
					memcpy( buf + numberOfBytesRead, tmp, BLOCKSIZE);
					//and dont forget to update the important stuff
					numberOfBytesRead += BLOCKSIZE;
					numberOfBytesRemaining -= BLOCKSIZE;
				}
				else { //we're in the last block
					memset(tmp, 0, BLOCKSIZE);
					dread(firstDataBlock + currentBlock, tmp);
					memcpy(buf + numberOfBytesRead, tmp, numberOfBytesRemaining); //key step here
					numberOfBytesRead += numberOfBytesRemaining;
					numberOfBytesRemaining = 0; //dont really need to update this but its pretty satisfying
					break;
				}
				//update the things that keep the while loop going
				currentBlock = fatTable[currentBlock].next;
				blockNumber++;
			}

			return numberOfBytesRead;
		}
		else { //only have to read one block (yay)
			char tmp[BLOCKSIZE];
			memset(tmp, 0, BLOCKSIZE);
			dread(firstDataBlock + currentBlock, tmp);
			memcpy(buf, tmp + offset % BLOCKSIZE, numberOfBytesRemaining);
			return numberOfBytesRemaining;
		}
	  } //ending to the if statement that checks to see if we have the right dirent
	}// ending to the for loop that goes through all the dirents
	return -ENOENT;
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
	/**
	 * Loop through them and write the other data blocks and fatTable entries
	 * Make sure to set fatTable entry.next to allocatedBlock[i+1]
	 * use memset,memcpy and dwrite to write the last one cause it might not fit the
	 * last BLOCK completely
	 * Return size of the new file (offset + sizeOfBuffer)
	 * ********* END originalSize == 0
	 */
		// Find the Dirent
		int dirEnt = findDEBlock(path);

		// Find the FatEnt
		int firstFatEnt = allTheDirEnts[dirEnt].first_block;

		// Renamed them for my mental health
	  const char *buffer = buf;
	  int sizeOfBuffer = size;
	  int originalSize = allTheDirEnts[dirEnt].size;

	  // // They always use 1 block
	  // if(originalSize == 0) {
	  // 	int currentNumberOfBlocks = 1; // Got 1 block from create
	  // }
	  // else {
	  // 	int currentNumberOfBlocks = originalSize / BLOCKSIZE;
	  // }

	  // Start doing the real work here.
	  if(originalSize == 0) { // Fresh file
	  	if(sizeOfBuffer <= BLOCKSIZE) { // Write to this block, happily
	  		char tempDataBlock[BLOCKSIZE];
	  		memset(tempDataBlock, 0, BLOCKSIZE);
	  		memcpy(tempDataBlock + offset, buffer, sizeOfBuffer);
	  		dwrite(firstDataBlock + firstFatEnt, tempDataBlock); // firstDataBlock# + fatEnt#
	  		// Update the fatEnt
	  		fatTable[firstFatEnt].used = 1;
	  		fatTable[firstFatEnt].eof = 1;
	  		// Update the dirEnt size
	  		allTheDirEnts[dirEnt].size = offset + sizeOfBuffer;
	  		return sizeOfBuffer; 
	  	}
	  	else { // sizeOfBuffer > Blocksize, and we cry.
	  		// Figure out how many blocks we need
	  		int necessaryBlocks = (sizeOfBuffer - BLOCKSIZE - 1) / BLOCKSIZE + 1;
	  		// We already have the first block allocated.
	  		// Allocate an array of these fatEnts cause we're lazy-ish
	  		int *fatEntryIndeces = (int *) calloc(necessaryBlocks, sizeof(int));
	  		// Get the necessary FatEnts
	  		for(int i = 0; i < necessaryBlocks; i++) {
	  			int fatEntry = findNextAvailableFatent();
	  			if(fatEntry == -1) {
	  				return -ENOSPC;
	  			}
	  			fatEntryIndeces[i] = fatEntry;
	  		}
	  		// Now we have an array of the fatEnts we can use [int, int, int]
	  		// Write the first block
	  		dwrite(firstDataBlock + firstFatEnt, (char *)buffer);
	  		// Update fatEnt
	  		fatTable[firstFatEnt].eof = 0;
	  		fatTable[firstFatEnt].next = fatEntryIndeces[0];

	  		// All the blocks up until the last one cause it may not fill an entire block
	  		for(int a = 0; a < necessaryBlocks; a++) {
	  			// If its not the last one
	  			if ( a < (necessaryBlocks-1)) {
	  				fatTable[fatEntryIndeces[a]].next = fatEntryIndeces[a+1];
	  				fatTable[fatEntryIndeces[a]].eof = 0;
	  				dwrite(firstDataBlock + fatEntryIndeces[a], (char *)(buffer + BLOCKSIZE + (a * BLOCKSIZE)));
	  			}
	  			else { // It is the last one woo, finally.
	  				// Set the last Fatent, finally.
	  				fatTable[fatEntryIndeces[a]].next = 0;
	  				fatTable[fatEntryIndeces[a]].eof = 1;
	  				// Don't want to copy garbage so at this point the leftovers
	  				// Could be less than BLOCKSIZE... stupid.
	  				int leftoverBufferSize = sizeOfBuffer - BLOCKSIZE - (a * BLOCKSIZE);
	  				char tempDataBlock[BLOCKSIZE];
	  				memset(tempDataBlock, 0, BLOCKSIZE);
	  				memcpy(tempDataBlock, buffer + BLOCKSIZE + (a * BLOCKSIZE), leftoverBufferSize);
	  				dwrite(firstDataBlock + fatEntryIndeces[a], tempDataBlock);
	  			}
	  		}
	  		// We're done, we're friggin done
	  		// Update the dirEnt size
	  		allTheDirEnts[dirEnt].size = offset + sizeOfBuffer;
	  		free(fatEntryIndeces);
	  		return sizeOfBuffer; 
	  	}
	  }
	  else if (originalSize < (sizeOfBuffer + offset)) { // It's not a fresh file, kill us now.
	  /** 
			* else if originalSize < sizeOfBuffer + offset These are in bytes...ugh
			* Find out how many blocks the original uses
			* Find out how many buffer+offset need
			* If I need more, allocate the data blocks and update their fatents...ugh
			* 
	  	*/
			int numberOfBlocksWeHave = originalSize / BLOCKSIZE + 1;
			int numberOfBlocksWeNeed = (offset + sizeOfBuffer) / BLOCKSIZE + 1;
			if(numberOfBlocksWeNeed > numberOfBlocksWeHave) {  // ------------------------ This if branch is super questionable -----------------------------/
				// We need to allocate new blocks
				int numberOfNewBlocksWeNeed = numberOfBlocksWeNeed - numberOfBlocksWeHave;
				// Find the eof, fatEnt index
				int currentFatEnt = firstFatEnt;
				int blockNumberCurrentlyAt = 0;
				while(!fatTable[currentFatEnt].eof) {
					if(blockNumberCurrentlyAt == numberOfNewBlocksWeNeed) {
						break;
					}
					blockNumberCurrentlyAt++;
					currentFatEnt = fatTable[currentFatEnt].next;
				}
				// At this point, current is the old last one.

				// Allocate space for new fatents
				int *fatEntryIndeces = (int *) calloc(numberOfNewBlocksWeNeed, sizeof(int));
	  		// Get the necessary FatEnts
	  		int fatEntry;
	  		for(int i = 0; i < numberOfNewBlocksWeNeed; i++) {
	  			fatEntry = findNextAvailableFatent();
	  			if(fatEntry == -1) {
	  				return -ENOSPC;
	  			}
	  			fatEntryIndeces[i] = fatEntry;
	  		}

	  		fatTable[currentFatEnt].eof = 0; // It's no longer the last fatent
	  		fatTable[currentFatEnt].next = fatEntryIndeces[0];

	  		// All the blocks up until the last one cause it may not fill an entire block
	  		for(int a = 0; a < numberOfNewBlocksWeNeed; a++) {
	  			// If its not the last one
	  			if ( a < (numberOfNewBlocksWeNeed-1)) {
	  				fatTable[fatEntryIndeces[a]].next = fatEntryIndeces[a+1];
	  				fatTable[fatEntryIndeces[a]].eof = 0;
	  				dwrite(firstDataBlock + fatEntryIndeces[a], (char *) (buffer + BLOCKSIZE + (a * BLOCKSIZE)));
	  			}
	  			else { // It is the last one woo, finally.
	  				// Set the last Fatent, finally.
	  				fatTable[fatEntryIndeces[a]].next = 0;
	  				fatTable[fatEntryIndeces[a]].eof = 1;
	  				// Don't want to copy garbage so at this point the leftovers
	  				// Could be less than BLOCKSIZE... stupid.
	  				int leftoverBufferSize = sizeOfBuffer - BLOCKSIZE - (a * BLOCKSIZE);
	  				char tempDataBlock[BLOCKSIZE];
	  				memset(tempDataBlock, 0, BLOCKSIZE);
	  				memcpy(tempDataBlock, buffer + BLOCKSIZE + (a * BLOCKSIZE), leftoverBufferSize);
	  				dwrite(firstDataBlock + fatEntryIndeces[a], tempDataBlock);
	  			}
	  		}
				// If there's a space between offset and originalSize, then pad with 0s
				if(offset > originalSize) {
					// Padd with 0s
				}

				// Do the writes
				/** BEGIN "DO THE WRITES" */
	  		// Find the block that we need to start writing at using the offset
				int blockToStartWritingAt = offset / BLOCKSIZE;
	  		if (blockToStartWritingAt == ((offset + sizeOfBuffer) / BLOCKSIZE)) { // If we're writing to the last block, hint: we should be.
	  			char tempDataBlock[BLOCKSIZE];
	  			dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  			memcpy(tempDataBlock + (offset % BLOCKSIZE), buffer, sizeOfBuffer);
	  			dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  		}
	  		else { // If we need to write to more than one block
	  			char tempDataBlock[BLOCKSIZE];
	  			// Copy the beginning of the offset block to the offset, cause we needs it.
	  			// How much data is that OH WAIT, WHAT? MATH??
	  			int weakBeginningDataAmount = BLOCKSIZE - (offset % BLOCKSIZE);
	  			dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  			memcpy(tempDataBlock + (offset % BLOCKSIZE), buffer, weakBeginningDataAmount);
	  			dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  			blockNumberCurrentlyAt++; // Go to the next block.
	  			currentFatEnt = fatTable[currentFatEnt].next; // Really, go to the next block.

	  			int theLastBlockNumber = (offset + sizeOfBuffer) / BLOCKSIZE;
	  			int anotherCounter;
	  			// Do your best to write the blocks in the middle
	  			for(anotherCounter = 0; blockNumberCurrentlyAt < theLastBlockNumber; blockNumberCurrentlyAt++, currentFatEnt = fatTable[currentFatEnt].next, anotherCounter++) {
	  				dwrite(firstDataBlock + currentFatEnt, (char *) buffer + (anotherCounter + 1) * BLOCKSIZE - offset % BLOCKSIZE);
	  			}

	  			dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  			weakBeginningDataAmount =  (offset + sizeOfBuffer) % BLOCKSIZE;
	  			memset(tempDataBlock, 0, BLOCKSIZE);
	  			memcpy(tempDataBlock, buffer + (anotherCounter + 1) * BLOCKSIZE - offset % BLOCKSIZE, weakBeginningDataAmount);
	  			dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  			fatTable[currentFatEnt].eof = 1;
	  		}
				// Return how much you wrote.
				return sizeOfBuffer + (offset - originalSize); 
			}
			else { // ------------------------ This else branch is questionable -----------------------------/
			 	/** BEGIN "DO THE WRITES" */
	  		// Find the block that we need to start writing at using the offset
				int blockToStartWritingAt = offset / BLOCKSIZE + 1;

	  		// Find the fatent with eof (while loops for dayz)
				int currentFatEnt = firstFatEnt;
				int blockNumberCurrentlyAt = 0;
				while(!fatTable[currentFatEnt].eof) {
					if(blockNumberCurrentlyAt == blockToStartWritingAt) {
						break;
					}
					blockNumberCurrentlyAt++;
					currentFatEnt = fatTable[currentFatEnt].next;
				}
	  		// At this point currentFatEnt should hold the fatEnt index I want.
	  		// blockNumberCurrentlyAt should be equal to blockToStartWritingAt

	  		if (blockToStartWritingAt == ((offset + sizeOfBuffer) / BLOCKSIZE + 1)) { // If we're writing to the last block
	  			char tempDataBlock[BLOCKSIZE];
	  			dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  			memcpy(tempDataBlock + (offset % BLOCKSIZE), buffer, sizeOfBuffer);
	  			dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  			// It's finally written, christ...
	  		}
	  		else { // If we need to write to more than one block
	  			char tempDataBlock[BLOCKSIZE];
	  			// Copy the beginning of the offset block to the offset, cause we needs it.
	  			// How much data is that OH WAIT, WHAT? MATH??
	  			int weakBeginningDataAmount = BLOCKSIZE - (offset % BLOCKSIZE);
	  			dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  			memcpy(tempDataBlock + (offset % BLOCKSIZE), buffer, weakBeginningDataAmount);
	  			dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  			blockNumberCurrentlyAt++; // Go to the next block.
	  			currentFatEnt = fatTable[currentFatEnt].next; // Really, go to the next block.

	  			int theLastBlockNumber = (offset + sizeOfBuffer) / BLOCKSIZE;
	  			int anotherCounter;
	  			// Do your best to write the blocks in the middle
	  			for(anotherCounter = 0; blockNumberCurrentlyAt < theLastBlockNumber; blockNumberCurrentlyAt++, currentFatEnt = fatTable[currentFatEnt].next, anotherCounter++) {
	  				dwrite(firstDataBlock + currentFatEnt, (char *) buffer + (anotherCounter + 1) * BLOCKSIZE - offset % BLOCKSIZE);
	  			}

	  			dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  			weakBeginningDataAmount =  (offset + sizeOfBuffer) % BLOCKSIZE;
	  			memset(tempDataBlock, 0, BLOCKSIZE);
	  			memcpy(tempDataBlock, buffer + (anotherCounter + 1) * BLOCKSIZE - offset % BLOCKSIZE, weakBeginningDataAmount);
	  			dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  			fatTable[currentFatEnt].eof = 1;
	  		}

	  		allTheDirEnts[dirEnt].size = offset + sizeOfBuffer;
	  		return sizeOfBuffer; 
	  		/** END OF THE "DO THE WRITES" BLOCK*/

			}

	  }
	  else {
	  	/** BEGIN "DO THE WRITES" */
	  	// Find the block that we need to start writing at using the offset
	  	int blockToStartWritingAt = offset / BLOCKSIZE + 1;
	  	
	  	// Find the fatent with eof (while loops for dayz)
	  	int currentFatEnt = firstFatEnt;
	  	int blockNumberCurrentlyAt = 0;
	  	while(!fatTable[currentFatEnt].eof) {
	  		if(blockNumberCurrentlyAt == blockToStartWritingAt) {
	  			break;
	  		}
	  		blockNumberCurrentlyAt++;
	  		currentFatEnt = fatTable[currentFatEnt].next;
	  	}
	  	// At this point currentFatEnt should hold the fatEnt index I want.
	  	// blockNumberCurrentlyAt should be equal to blockToStartWritingAt

	  	if (blockToStartWritingAt == ((offset + sizeOfBuffer) / BLOCKSIZE + 1)) { // If we're writing to the last block
	  		char tempDataBlock[BLOCKSIZE];
	  		dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  		memcpy(tempDataBlock + (offset % BLOCKSIZE), buffer, sizeOfBuffer);
	  		dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  		// It's finally written, christ...

	  	}
	  	else { // If we need to write to more than one block
	  		char tempDataBlock[BLOCKSIZE];
	  		// Copy the beginning of the offset block to the offset, cause we needs it.
	  		// How much data is that OH WAIT, WHAT? MATH??
	  		int weakBeginningDataAmount = BLOCKSIZE - (offset % BLOCKSIZE);
	  		dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  		memcpy(tempDataBlock + (offset % BLOCKSIZE), buffer, weakBeginningDataAmount);
	  		dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  		blockNumberCurrentlyAt++; // Go to the next block.
	  		currentFatEnt = fatTable[currentFatEnt].next; // Really, go to the next block.

	  		int theLastBlockNumber = (offset + sizeOfBuffer) / BLOCKSIZE;
	  		int anotherCounter;
	  		// Do your best to write the blocks in the middle
	  		for(anotherCounter = 0; blockNumberCurrentlyAt < theLastBlockNumber; blockNumberCurrentlyAt++, currentFatEnt = fatTable[currentFatEnt].next, anotherCounter++) {
	  			dwrite(firstDataBlock + currentFatEnt, (char *) buffer + (anotherCounter + 1) * BLOCKSIZE - offset % BLOCKSIZE);
	  		}

	  		dread(firstDataBlock + currentFatEnt, tempDataBlock);
	  		weakBeginningDataAmount =  (offset + sizeOfBuffer) % BLOCKSIZE;
	  		memset(tempDataBlock, 0, BLOCKSIZE);
	  		memcpy(tempDataBlock, buffer + (anotherCounter + 1) * BLOCKSIZE - offset % BLOCKSIZE, weakBeginningDataAmount);
	  		dwrite(firstDataBlock + currentFatEnt, tempDataBlock);
	  		fatTable[currentFatEnt].eof = 1;
	  	}

	  	allTheDirEnts[dirEnt].size = offset + sizeOfBuffer;
	  	return sizeOfBuffer; 
	  	/** END OF THE "DO THE WRITES" BLOCK*/
	  }

	  return -ENOENT;
}

/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{

	for (int i = 0; i < 100; i++) {
	    if (strcmp(allTheDirEnts[i].name, path) == 0 && allTheDirEnts[i].valid == 1) {
	      	allTheDirEnts[i].valid = 0;
	    	allTheDirEnts[i].name[0] = '\0';

	      	while (!fatTable[i].eof) {
	      		fatTable[i].used = 0;
	      		i = fatTable[i].next;
	      	}
	      	fatTable[i].used = 0;
	      	
	      	//write the dirent
	      	char tmp[BLOCKSIZE];
	      	memset(tmp, 0, BLOCKSIZE);
	      	memcpy(tmp, &allTheDirEnts[i], sizeof(dirent));
	      	dwrite(i + 1, tmp);

	      	return 0;
	    }
  }
  
  // if control reaches here, that means the file did not exist
  return -ENOENT;
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
	int dirent = findDEBlock(from);
	if (dirent != -1) {
		strcpy(allTheDirEnts[dirent].name, to);
		clock_gettime(CLOCK_REALTIME, &allTheDirEnts[dirent].modify_time);
		return 0;
	}
	return -ENOENT;
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
	int dirent = findDEBlock(file);
	if (dirent != -1) {
		allTheDirEnts[dirent].mode = (mode & 0x0000ffff);
		clock_gettime(CLOCK_REALTIME, &allTheDirEnts[dirent].modify_time);
		return 0;
	}
	return -ENOENT;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{
	int dirent = findDEBlock(file);
	if (dirent != -1) {
		allTheDirEnts[dirent].user = uid;
		allTheDirEnts[dirent].group = gid;
		clock_gettime(CLOCK_REALTIME, &allTheDirEnts[dirent].modify_time);
		return 0;
	}
		
	return -ENOENT;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
	int dirent = findDEBlock(file);
	if (dirent != -1) {
		allTheDirEnts[dirent].access_time = ts[0];
		allTheDirEnts[dirent].modify_time = ts[1];
		return 0;
	}
		
	return -ENOENT;
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
	int dirent = findDEBlock(file);
	if (dirent != -1) {
		if (offset > allTheDirEnts[dirent].size) {
			return -1;
		}
		int offsetBlockNum = offset / BLOCKSIZE;
		int lastDataBlock = allTheDirEnts[dirent].size / BLOCKSIZE;
		if (offsetBlockNum == lastDataBlock) {
			allTheDirEnts[dirent].size = offset;
			return 0;
		}
		else {
			int currentFatEnt = allTheDirEnts[dirent].first_block;
			int a = 0;
			while (!fatTable[currentFatEnt].eof) {
				if (fatTable[currentFatEnt].used == 0) {
					return -1;
				}

				if (a == offsetBlockNum) {
					break;
				}

				currentFatEnt = fatTable[currentFatEnt].next;
				a++;
			}

			if (fatTable[currentFatEnt].eof == 1) {
				return -1;
			}

			fatTable[currentFatEnt].eof = 1;
			currentFatEnt = fatTable[currentFatEnt].next;
			while (!fatTable[currentFatEnt].eof) {
				fatTable[currentFatEnt].used = 0;
				currentFatEnt = fatTable[currentFatEnt].next;
			}
			fatTable[currentFatEnt].used = 0;
			fatTable[currentFatEnt].eof = 0;
			allTheDirEnts[dirent].size = offset;
			return 0;
		}
	}
	return -ENOENT;
}
int findNextAvailableFatent() {
	for(int i = 0; i < numberOfFatEnts; i++) {
		if(fatTable[i].used == 0) {
			fatTable[i].used = 1;
			fatTable[i].eof = 1;
			return i;
		}
	}

	return -1; // If it's full, which is terrible.
}

// 
int findNextAvailableDirEnt() {
	for(int i = 0; i < 100; i++) {
		if(allTheDirEnts[i].valid == 0) { // 0 means free
			return i; // Returns 0-99
		}
	}

	return -1;
}

int findDEBlock(const char* path) {
	// Finds the DE block # given the path
	for(int i = 0; i < 100; i++) {
		if (strcmp(allTheDirEnts[i].name, path) == 0) {
			return i;
		}
	}

	return -1;
}

int validPath(const char* path) {
	// Count the /'s
	// If there's more than 1, give up.
	int pathLength = strlen(path);
	int counter = 0;
	for(int i = 0; i < pathLength; i++) {
		if (path[i] == '/')
			counter++;
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

