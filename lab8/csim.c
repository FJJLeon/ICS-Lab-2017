/*
 *
 * Student Name: Junjie Fang
 * Student ID: 516030910006
 *
 */
#include <stdio.h>
#include "cachelab.h"
#include <string.h>
#include <getopt.h>
#include <stdlib.h> 
#include <unistd.h>

/* global cache parameter */
#define num_m 64 		// number of physical address bits
#define maxNameLen 20	// max filename length
int num_s;				// number of set index bits 
int num_E;				// number of lines per sets
int num_b;				// number of block offset bits
int num_t;				// number of tag bits, m-(s+b)
char filename[maxNameLen]; 
int currlineNum = 0;

/* Optional verbose flag */
int verbose = 0;

/* cache run condition */
int hits = 0, misses = 0, evictions = 0;

/* get tag or set bits */
#define GET_TAG(addr) (((addr) >> (num_s + num_b)) & ((1 << num_t) - 1))
#define GET_SET(addr) (((addr) >> (num_b)) & ((1 << num_s) - 1))

/* struct for cache */
typedef struct line // each line inside sets
{
	int valid;		// the valid bit must be set
	long tag;		// the tag bits must match that in address
	//long block;		// cache sim actually won't touch memory
	int lastAccessNum;	// the file line num of last access this block
}line_type;

typedef struct set
{
	line_type *lines;
}set_type;

typedef struct c
{
	set_type *sets;
	int s, S, E, b, t;
}cache_type;

cache_type cache;


/* print usage info */
void usage()
{
	printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
	printf("Options:\n");
	printf("  -h         Print this help message.\n");
	printf("  -v         Optional verbose flag.\n");
  	printf("  -s <num>   Number of set index bits.\n");
  	printf("  -E <num>   Number of lines per set.\n");
  	printf("  -b <num>   Number of block offset bits.\n");
  	printf("  -t <file>  Trace file.\n");
  	printf("\nExamples:\n");
  	printf("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
  	printf("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");

  	exit(0);
}

/* read command line */
void read_cmd(int argc, char * const argv[], char *filename) 
{
	int opt;
	opterr = 1;
	num_s = -1;
	num_E = -1;
	num_b = -1;

	while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
		switch (opt) {
			case 'h':
				usage();
				break;
			case 'v':
				verbose = 1;
				break;
			case 's':
				num_s = atoi(optarg);
				break;
			case 'E':
				num_E = atoi(optarg);
				break;
			case 'b':
				num_b = atoi(optarg);
				break;
			case 't':
				strncpy(filename, optarg, maxNameLen) ;
				break;
			default:
				usage();
				break;
		}
	}

	// cmd parameter is enough but value is not
	if (num_s == -1 || num_E == -1 || num_b == -1 || *filename == 0 ) {
		printf("./csim: Missing required command line argument\n");
		usage();
	}
}

/* initialize the cache */
void init()
{
	cache.s = num_s;
	cache.S = 1 << num_s;
	cache.E = num_E;
	cache.b = num_b;
	cache.t = num_m - (num_s + num_b);
	num_t = cache.t;

	cache.sets = (set_type *)malloc(cache.S * sizeof(set_type));
	
	int i;
	for (i=0; i < cache.S; i++){
		cache.sets[i].lines = (line_type *)malloc(cache.E * sizeof(line_type));
		int j;
		for (j=0; j<cache.E; j++){
			cache.sets[i].lines[j].valid = 0;
			cache.sets[i].lines[j].lastAccessNum = 0;
		}
	}
}

/* free the cache */
void destory()
{
	int i;
	for (i=0; i<cache.S; i++)
		free(cache.sets[i].lines);
	free(cache.sets);
}

void check()
{
	for (int i=0; i<cache.S; i++){
		for (int j=0; j<cache.E; j++){
			line_type tmpLine = cache.sets[i].lines[j];

			printf("set %d, line %d is %s valid, tag is %ld, access at %d\n",\
					i, j, tmpLine.valid ? " ": "not", tmpLine.tag, tmpLine.lastAccessNum);
		}
	}
}

void handleLine(char op, long addr, int size){
	int tmpSet = GET_SET(addr);
	long tmpTag = GET_TAG(addr);
	
	int empty_line = -1;
	int min_lineNum = 99999999;
	int evict_line = -1;

	line_type *tmpLine;
	// search for hit and handle LRU
	int i; 
	for (i=0; i<cache.E; i++){
		tmpLine = &cache.sets[tmpSet].lines[i];

		if (tmpLine->valid && tmpLine->tag == tmpTag){
			hits++;
			tmpLine->lastAccessNum = currlineNum;
			if (verbose) 
				printf(" hit");
			return;
		}

		// handle LRU replace strategy
		if (empty_line == -1){
			if (!tmpLine->valid)
				empty_line = i;
			else if (tmpLine->lastAccessNum < min_lineNum){
				min_lineNum = tmpLine->lastAccessNum;
				evict_line = i;
			}
		} 
	}

	// no hit
	misses++;

	if (verbose) 
		printf(" miss");

	if (empty_line != -1){// empty line exist
		tmpLine = &cache.sets[tmpSet].lines[empty_line];
		tmpLine->valid = 1;
		tmpLine->tag = tmpTag;
		tmpLine->lastAccessNum = currlineNum;
	}
	else{// no empty line exist, need evict
		evictions++;
		if (verbose) 
			printf(" eviction");
		tmpLine = &cache.sets[tmpSet].lines[evict_line];
		tmpLine->tag = tmpTag;
		tmpLine->lastAccessNum = currlineNum;
	}

}

void read_file(char *filename) 
{
	FILE *file = fopen(filename, "r");
	char op;
	long addr;
	int size;

	//check();
	while (fscanf(file, "%c", &op) != -1){
		if (op != 'I'){
			if (op == '\n') // in the first line of dave.trace, there is a \n at end?
				fscanf(file, "%c", &op);
			fscanf(file, "%c %lx,%d", &op, &addr, &size);
			fgetc(file);
		}
		else{ // op is 'I'
			fscanf(file, "%lx,%d", &addr, &size);
			fgetc(file);
			continue;
		} 

		if (verbose && (op == 'S' || op == 'L' || op == 'M')){
			printf("%c %lx,%d", op, addr, size);
		}

		currlineNum++;
		handleLine(op, addr, size);
		if (op == 'M')// 'M' = 'L' + 'S' 
			handleLine(op, addr, size);

		if (verbose && (op == 'S' || op == 'L' || op == 'M')){
			printf("\n");
		}
		//check();
	}

	fclose(file);

}
int main(int argc, char * const argv[])
{
	read_cmd(argc, argv, filename);
	printf("name: %s\n",filename);

	init();

	read_file(filename);

    printSummary(hits, misses, evictions);

    destory();
    return 0;
}
