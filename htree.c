#include <stdio.h>     
#include <stdlib.h>   
#include <stdint.h>  
#include <inttypes.h>  
#include <errno.h>     // for EINTR
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/mman.h>
#include <string.h>
#include "common.h"
#include "common_threads.h"


// Print out the usage of the program and exit.
void Usage(char*);
uint32_t jenkins_one_at_a_time_hash(const uint8_t* , size_t );
void *tree(void *arg);
uint8_t* file;
int n;
int m;
int extraThread;

struct info{
	int height;
	int i;
};

#define BSIZE 4096
uint32_t* htable;

int 
main(int argc, char** argv) 
{
  int32_t fd;
  uint32_t nblocks;

  // input checking 
  if (argc != 3)
    Usage(argv[0]);

  // open input file
  fd = open(argv[1], O_RDWR);
  if (fd == -1) {
    perror("open failed");
    exit(EXIT_FAILURE);
  }
  // use fstat to get file size
	struct stat statbuf;

	if (fstat(fd, &statbuf) == -1){
              printf("failed to fstat");
	      exit(EXIT_FAILURE);
	}
	uint32_t fileSize = statbuf.st_size;
  // calculate nblocks 
	nblocks = fileSize / 4096;

  printf("no. of blocks = %u \n", nblocks);

  double start = GetTime();

  // calculate hash of the input file
	struct info vars;
	void* rHash = malloc(sizeof(uint32_t));
	vars.height = atoi(argv[2]);
	vars.i = 0;
	file = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd , 0);
	n = nblocks;
	m = 1 << (vars.height + 1);
	extraThread = 1 << vars.height;
	uint32_t hash;

	if(vars.height >= 0){
		pthread_t p1;
		Pthread_create(&p1, NULL, tree, &vars);
		Pthread_join(p1, &rHash);
		printf("hash value = %s \n", (char *)rHash);
	}
	else if(vars.height == -1){
		hash = jenkins_one_at_a_time_hash(&file[0],fileSize);
		printf("hash value = %u \n", hash);
	}
	else{
		printf("Invalid height");
		exit(EXIT_FAILURE);
	}

  double end = GetTime();
  printf("num Threads = %d\n", 1 << (vars.height + 1));
  printf("blocks per Thread = %u\n", n/m);
  printf("time taken = %f \n", (end - start));
  close(fd);
  return EXIT_SUCCESS;
}

uint32_t 
jenkins_one_at_a_time_hash(const uint8_t* key, size_t length) {
  size_t i = 0;
  uint32_t hash = 0;
  while (i != length) {
    hash += key[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}


void 
Usage(char* s) {
  fprintf(stderr, "Usage: %s filename height \n", s);
  exit(EXIT_FAILURE);
}

void* tree(void* arg) {
        struct info vars = *(struct info *) arg;
	uint32_t iHash;
	uint64_t temp = (uint64_t)n / m * BSIZE;
	void* cHash = malloc(sizeof(uint32_t));
	void* lHash = malloc(sizeof(uint32_t));
	void* rHash = malloc(sizeof(uint32_t));
	int i = vars.i;
	if(vars.height > 0){ // internal node functions
		vars.height--;
		
		pthread_t lchild; // create children
		pthread_t rchild;

		vars.i = 2 * i + 1; // increment i value for left children
		Pthread_create(&lchild, NULL, tree, &vars);
		Pthread_join(lchild, &lHash);

		vars.i = 2 * i + 2; // increment i value for right children
		Pthread_create(&rchild, NULL, tree, &vars);
		Pthread_join(rchild, &rHash);

		iHash = jenkins_one_at_a_time_hash(&file[temp * i], temp); //hash current block

		sprintf(cHash, "%u", iHash);
		strcat(cHash,lHash); // current hash + lhash
		strcat(cHash,rHash); // current hash + lhash + rhash
		iHash = jenkins_one_at_a_time_hash(cHash, strlen(cHash)); // rehash new  hash
		sprintf(cHash,"%u",iHash);
		
		if((i % 2) == 0){
		        strcpy(rHash, cHash);
			pthread_exit(rHash); // send right child up tree
		}
		else{
			strcpy(lHash, cHash);
			pthread_exit(lHash); // send left child up tree
		}

	}
	else if(vars.height == 0 && i == extraThread - 1){ //left most thread creates extra thread at h+1
		pthread_t lchild;
		
		vars.i = 2 * i + 1; // increment i value for extra thread
		Pthread_create(&lchild, NULL, tree, &vars);
		Pthread_join(lchild, &lHash);

		iHash = jenkins_one_at_a_time_hash(&file[temp * i], temp); // hash current block

		sprintf(cHash, "%u", iHash);
		strcat(cHash, lHash);// current hash + left child 
		iHash = jenkins_one_at_a_time_hash(cHash, strlen(cHash)); // rehash new hash
		sprintf(cHash,"%u",iHash);
		strcpy(lHash, cHash);
		pthread_exit(lHash); //send left most child up to parent
		}
	else if(vars.height == 0){ //all other leafs
		iHash = jenkins_one_at_a_time_hash(&file[(i * n / m) * BSIZE], (n / m) * BSIZE); //hash current block
		
		if((i % 2) == 0){
			sprintf(rHash,"%u",iHash);
			pthread_exit(rHash);//send right child up tree
		}
		else{
			sprintf(lHash,"%u",iHash);
			pthread_exit(lHash); // send left child up tree
		}
	}
	return NULL;
}
