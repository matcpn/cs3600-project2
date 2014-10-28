Project 2 - Filesystem

We chose to implement a FAT filesystem, which uses a table to point to and organize files on the disk in order to quickly jump around the files.
We chose to do this implementation mostly because we are both short on time this semester and we're told from friends that INODE isn't fun

Final submission
To test our Filesystem, we ran it against the test script provided. When a test would fail, we would mount the filesystem and try to do the same command the test would do manually, while monitoring the fuse output. If we weren't able to figure it out then, we would take a break and think about it. Usually this allowed us to take a step back and look at the action at every angle to try and figure out what was happening internally.

A challenge we faced while writing the function vfs_write was that the helper function that was written to resolve paths to their dirents was written incorrectly. It would return the incorrect value and then we would segfault while trying to access the incorrect value of data.

We fixed this by re-reading the documentation for strcmp and eventually fixing the method, and that was the source of our error.

The biggest challenge we faced was the large writes we made, being different from the tests in large orders of magnitude.
We have found that our vfs_write function was for some reason writing the data for the last block to all the blocks of the file repeatedly. We really had no idea how to fix this and ended up taking the slip day, not fixing it and then giving up.

After that, our next biggest challenge was writing the truncate function.

We believe that in our vfs_read function we were using the dread fuse function incorrectly as we were not getting the data that we really were writing to the file and going for. The truncation was fixed by moving forward and double-checking that the write function worked, turns out there was an off-by-one error in the writing to multiple blocks.