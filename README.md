Project 2 - Filesystem

We chose to implement a FAT filesystem, which uses a table to point to and organize files on the disk in order to quickly jump around the files.
We chose to do this implementation mostly because we are both short on time this semester and we're told from friends that INODE isn't fun

Our high level approach was first to write down all the actions that were required for the functions we needed to write.
For example, in vfs_create, we wrote down everything that we needed to do to create an empty file and we rewrote it as comments in instructions, then we just coded out the instructions we wrote down

The challenges we faced was that writing everything out became a lot of things, especailly in cases like write because there are a lot of different things to do when you want to write a file

We are proud that we met the minimum requirements for the project because we know its one of the harder projects for the class.