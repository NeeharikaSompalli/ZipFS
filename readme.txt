

Execution of the program:

1. Compile the code using the command with a executable "zipfs"

gcc  -Wall ZipFs_nsompal.c `pkg-config fuse libzip --cflags --libs` -o zipfs 

2. Mount the executable onto a temporary directory "tmp1" which will unzip the file ESAHW1.zip

./zipfs ESAHW1.zip tmp1/

3. Run the commands on the mounted directory

Ex: ls -l tmp1/

4. As this is read only file system write, remove commands doesn't work on the mounted directory.

Dependencies:

libzip, libfuse




