/* Copyright (C) 2003,2004 Andi Kleen, SuSE Labs.
   Allocate memory with policy for testing.

   numactl is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; version
   2.

   numactl is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should find a copy of v2 of the GNU General Public License somewhere
   on your Linux system; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <string.h>
#include <stdbool.h>
#include "numa.h"
#include "numaif.h"
#include "util.h"
#include <sys/syscall.h>

#define terr(x) perror(x)

enum {
	UNIT = 2*1024*1024,
};

#ifndef MADV_NOHUGEPAGE
#define MADV_NOHUGEPAGE 15
#endif

#define MAX_LINE_LENGTH 500
#define GB_SIZE 1073741824
#define KB_SIZE 1024
#define PAGE_SIZE 4096

#define SYS_user_kmalloc 449
#define SYS_user_kfree 450

int repeat = 1;
long length;
float percentage;

void usage(void)
{
	printf("memhog [-fFILE] [-rNUM] size[kmg] [policy [nodeset]]\n");
	printf("-f mmap is backed by FILE\n");
	printf("-rNUM repeat memset NUM times\n");
	printf("-H disable transparent hugepages\n");
	print_policies();
	exit(1);
}

unsigned long min(unsigned long a, unsigned long b) {
	return (a > b) ? b : a;
}

static bool check_for_pattern(FILE *fp, const char *pattern, char *buf) {
	while (fgets(buf, MAX_LINE_LENGTH, fp) != NULL) {
		if (!strncmp(buf, pattern, strlen(pattern)))
			return true;
	}
	return false;
}

static unsigned long get_mem() {
	unsigned long total_mem = 0;
	const char *field = "MemTotal:", *mem_path = "/proc/meminfo";
	char pattern[100];
	FILE *fp;
	char buffer[MAX_LINE_LENGTH];

	fp = fopen(mem_path, "r");
	if (!fp) {
		printf("%s: Failed to open file %s\n", __func__, mem_path);
		exit(EXIT_FAILURE);
	}

	if (!check_for_pattern(fp, field, buffer)) goto err_out;

	strcpy(pattern, field);
	strcat(pattern, "%10ld kB");
	if (sscanf(buffer, pattern, &total_mem) != 1) {
		printf("Reading /proc/meminfo error\n");
		exit(EXIT_FAILURE);
	}

err_out:
	fclose(fp);
	return total_mem;
}

static unsigned long get_numa_mem() {
	unsigned long free_mem = 0;
	float val1, val2, val3;
	const char *field = "MemFree", *command = "numastat -m";
	FILE *pipe;
	char buffer[1000];
	char pattern[1000];

	pipe = popen(command, "r");
	if (!pipe) {
		printf("popen failed!\n");
		exit(EXIT_FAILURE);
	}

	strcpy(pattern, field);
	strcat(pattern, " %f %f %f");
	while (!feof(pipe)) {
		if (fgets(buffer, 128, pipe) != NULL) {
			if (!strncmp(buffer, field, strlen(field))) break; 
		}
	}

	pclose(pipe);

	if (sscanf(buffer, pattern, &val1, &val2, &val3) != 3) {
		printf("Could not match pattern!\n");
		exit(EXIT_FAILURE);
	}

	free_mem = (unsigned long) (val2-KB_SIZE/4)*KB_SIZE*KB_SIZE;

	return free_mem;
}

void fragment(void *map)
{
	long i;
        unsigned long num_fragments = 0, frac_fragments;
	printf("Fragmenting memory...\n");
	for (i = 0;  i < length; i += UNIT) {
		long left = length - i;
		if (left >= UNIT) {
			left = UNIT;
			num_fragments++;
		}
		if ((i % GB_SIZE) == 0) {
			putchar('.');
			fflush(stdout);
		}
		memset(map + i, 0xff, left);
		munmap(map + i, min(PAGE_SIZE, left));
	}
	putchar('\n');

	frac_fragments = (unsigned long) num_fragments*percentage;
	printf("%ld fragments, allocating kernel memory %ld fragments...\n", num_fragments, frac_fragments);
	
	struct page **pgs = (struct page **) malloc(frac_fragments*sizeof(struct page *));
	for (i = 0;  i < frac_fragments; i ++) {
		long left = length - i*UNIT;
		if ((i*UNIT % GB_SIZE) == 0) {
			putchar('.');
			fflush(stdout);
		}
		if (left >= PAGE_SIZE) {
			pgs[i] = (struct page *) syscall(SYS_user_kmalloc, 0);
		}	
	}
	putchar('\n');

	//while (true) {}
	printf("Enter character: ");
	char c = getchar();
	printf("Character entered: ");
	putchar(c);
	printf("\n");
	
	for (i = 0; i < frac_fragments; i++) {
		syscall(SYS_user_kfree, pgs[i], 0);
	}
}

int main(int ac, char **av)
{
	char *map;
	struct bitmask *nodes, *gnodes;
	int policy, gpolicy;
	int ret = 0;
	int loose = 0;
	int i;
	int fd = -1;
	bool disable_hugepage = false;
	unsigned long total_mem;

	nodes = numa_allocate_nodemask();
	gnodes = numa_allocate_nodemask();

	while (av[1] && av[1][0] == '-') {
		switch (av[1][1]) {
		case 'f':
			fd = open(av[1]+2, O_RDWR);
			if (fd < 0)
				perror(av[1]+2);
			break;
		case 'r':
			repeat = atoi(av[1] + 2);
			break;
		case 'H':
			disable_hugepage = true;
			break;
		default:
			usage();
		}
		av++;
	}

	if (!av[1]) usage();

	total_mem = get_mem()*KB_SIZE;
	length = get_numa_mem();
	//length = atof(av[1])/100.0 * total_mem;
	//length = memsize(av[1]);
	printf("total mem = %lu, length = %lu\n", total_mem, length);
	percentage = atof(av[1])/100.0;

	if (av[2] && numa_available() < 0) {
		printf("Kernel doesn't support NUMA policy\n");
	} else
		loose = 1;
	policy = parse_policy(av[2], av[3]);
	if (policy != MPOL_DEFAULT)
		nodes = numa_parse_nodestring(av[3]);
        if (!nodes) {
		printf ("<%s> is invalid\n", av[3]);
		exit(1);
	}

	if (fd >= 0)
		map = mmap(NULL,length,PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	else
		map = mmap(NULL, length, PROT_READ|PROT_WRITE,
				   MAP_PRIVATE|MAP_ANONYMOUS,
				   0, 0);
	if (map == (char*)-1)
		err("mmap");

	if (mbind(map, length, policy, nodes->maskp, nodes->size, 0) < 0)
		terr("mbind");

	if (disable_hugepage)
		madvise(map, length, MADV_NOHUGEPAGE);

	gpolicy = -1;
	if (get_mempolicy(&gpolicy, gnodes->maskp, gnodes->size, map, MPOL_F_ADDR) < 0)
		terr("get_mempolicy");
	if (!loose && policy != gpolicy) {
		ret = 1;
		printf("policy %d gpolicy %d\n", policy, gpolicy);
	}
	if (!loose && !numa_bitmask_equal(gnodes, nodes)) {
		printf("nodes differ %lx, %lx!\n",
			gnodes->maskp[0], nodes->maskp[0]);
		ret = 1;
	}

	for (i = 0; i < repeat; i++)
		fragment(map);
	exit(ret);
}
