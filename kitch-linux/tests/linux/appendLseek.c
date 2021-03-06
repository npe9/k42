/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Testing open O_APPEND and lseek
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s  [-d <d>] <file> <n>\n"
	    "open <file> and write append <n> \"lines\" of "
	    "10 bytes each(n>0), using d file descriptors", prog);
}

static int
open_fds(int number_fds, int **fds, char *file, char *prog)
{
    int i;
    int fd;
    
    *fds = (int*) malloc(sizeof(int)*number_fds);
    if (*fds == 0) {
	fprintf(stderr, "%s: allocation of fds array (size %d) failed\n",
		prog, number_fds);
	return 1;
    }

    fd = (*fds)[0] = open (file, O_CREAT | O_APPEND | O_WRONLY, 0640);
    
    for (i=1; i < number_fds; i++) {
        (*fds)[i] = dup(fd);
	if ((*fds)[i] == -1) {
	    fprintf(stderr, "%s: %d-th open(%s) failed: %s\n", 
		    prog, i, file, strerror(errno));
	    return (1);
	}
    }

    return 0;
}

static int
check_file(int fd, char *file, char *prog, int *first)
{
    int ret;
    struct stat stat_buf;
    int last;
    char line[10];
    
    // find last element in the file. Is the file empty so far?
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: fstat failed: %s\n", 
		prog, strerror(errno));
	return 1;
    }
    
    if (stat_buf.st_size%10 !=0) {
	fprintf(stderr, "%s: file %s has not the expected format"
		"(it has not been generated by previous"
		"execution of %s\n", prog, file, prog);
	return 1;
    }
    
    if (stat_buf.st_size == 0) {
	*first = 0;
    } else {
	lseek(fd, -10, SEEK_END);
	ret = read(fd, line, 10);
	if (ret != 10) {
	    fprintf(stderr, "read for last number in the file failed"
		    "(ret=%d)\n", ret);
	    return 1;
	}
	// sanity check
	last = atoi(line);
	if (last != stat_buf.st_size/10 - 1) {
	    fprintf(stderr, "%s: file %s has not the expected format"
		    "(it has not been generated by previous "
		    "execution of %s(last=%d)\n", prog, file, prog, last);
	    return 1;
	}
	*first = last+1;
    }

    return 0;
}

static int
write_file(int number_fds, int *fds, int n, int first, int *size, char *file,
	   char *prog)
{
    char *buf, *ptr;
    int i, ret;
    char line[10];
    struct stat stat_buf;
    int fd_id = 0;
    
    // fill up buffer to be written; let's write the whole stuff in a single
    // call
    buf = (char*) malloc(n*10);
    if (buf==NULL) {
	fprintf(stderr, "malloc failed\n");
	return 1;
    }
    ptr = buf;
    for (i=first; i<first+n; i++) {
	sprintf(line, "%9d\n", i);
	memcpy(ptr, line, strlen(line));
	ptr += 10;
    }

    ret = write(fds[fd_id++], buf, n*10);
    if (ret == -1) {
	fprintf(stderr, "%s: write(%s) failed:%s\n", 
		prog, file, strerror(errno));
	return (1);
    }

    ret = fstat(fds[fd_id%number_fds], &stat_buf);
    if (ret == -1) {
	    fprintf(stderr, "%s: fstat failed: %s\n", 
		    prog, strerror(errno));
    }
    *size = stat_buf.st_size;
#if 0    
    fprintf(stdout, "%s: Added info for file. Final size for file is %d\n",
	    prog, *size);
#endif
    return 0;
}

static int
test_lseek(int number_fds, int *fds, int size, char *file, char *prog)
{
    int ntests, number;
    int position=0, last, last_position, jump;
    int i, ret;
    char line[10];
    int fd_id = 0;
    int fd;
    
    ntests = 10;
    if (ntests > size/10) ntests = size/10; 
    for (i=0; i < ntests; i++) {
	position = rand() % size;
	position -= position%10;
#if 0	
	fprintf(stdout, "%s: testing lseek to position %d\n", prog,
		position);
#endif
	fd = fds[fd_id % number_fds];
	fd_id++;
	ret = lseek(fd, position, SEEK_SET);
	if (ret == -1) {
	    fprintf(stderr, "%s: lseek to position %d of file %s failed:%s\n",
		    prog, position, file, strerror(errno));
	    return 1;
	}
	ret = read(fd, line, 10);
	if (ret != 10) {
	    fprintf(stderr, "%s: read of number at position %d of file %s"
		    "returned %d\n", prog, position, file, ret);
	    return 1;
	}
	// check if got expected number
	number = atoi(line);
	if (number != position/10) {
	    fprintf(stderr, "%s: read %d, expected %d\n", prog, number,
		    position/10);
	    return 1;
	}
    }

    // Testing lseek SEEK_CUR
    last_position = position+10;
    if (last_position == size) {
	// jump == 0 is not valid
	jump = rand() % (size-1) + 10;
	jump -= jump%10;
	jump *= -1;
    } else {
	jump = rand() % (size/2);
	jump -= jump%10;
	if (last_position >= size/2) jump *= -1;
    }

    fd = fds[fd_id % number_fds];
    fd_id++;
    ret = lseek(fd, jump, SEEK_CUR);
    if (ret == -1) {
	fprintf(stderr, "%s: lseek SEEK_CUR %d of file %s failed: %s\n",
		prog, jump, file, strerror(errno));
	return 1;
    }
    ret = read(fd, line, 10);
    if (ret != 10) {
	fprintf(stderr, "%s: read of number at position %d of file %s"
		"returned %d\n", prog, last_position+jump, file, ret);
	return 1;
    }

    last = atoi(line);
    if ((last_position+jump)/10 != last) {
	fprintf(stderr, "%s: after lseek SEEK_CUR read %d, expected %d"
		" (jump %d)\n", prog, last, (last_position+jump)/10, jump);
	return 1;
    }
    
    return 0;
}

static void
close_fds (int number_fds, int *fds)
{
    int i;
    for (i=0; i <number_fds; i++) {
	close(fds[i]);
    }
}

int
main(int argc, char *argv[])
{

    int n, first, size;
    int number_fds = 1, *fds;
    extern int optind;
    int c;
    const char *optlet = "d";
    char *file;
    
    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'd':
	    // dup option
	    if (optind == argc) {
		usage(argv[0]);
		return 1;
	    } else {
		number_fds = atoi(argv[optind++]);
		if (number_fds <= 0) {
		    fprintf(stderr, "%s: invalid argument for -d:%d\n",
			    argv[0], number_fds);
		    return 1;
		}
	    }
	    break;
	case '?':
	default:
	    usage(argv[0]);
	    return (1);
	}
    }

    if (optind >= argc-1) {
	usage(argv[0]);
	return (1);
    }

    file = argv[optind++];
    n = atoi(argv[optind]);
    if (n<=0 || n > 999999999) {
	fprintf(stderr, "number argument has to be in [0, 999999999]\n");
	return 1;
    }

    if (open_fds(number_fds, &fds, file, argv[0])) return 1;


    if (check_file(fds[0], file, argv[0], &first)) return 1;

    if (write_file(number_fds, fds, n, first, &size, file, argv[0])) return 1;

    if (test_lseek(number_fds, fds, size, file, argv[0])) return 1;
    
    fprintf(stdout, "Test of append and lseek succeeded\n");

    close_fds(number_fds, fds);

    return 0;
}
