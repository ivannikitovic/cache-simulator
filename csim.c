/* * * * * * * * * * * * * * * * * * *
 * csim.c - cache memory simulator   *
 * made by Ivan Nikitovic, in@bu.edu *
 * * * * * * * * * * * * * * * * * * */


#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>

typedef unsigned long int size_64;

typedef struct {
    bool valid; // valid bit
    size_64 tag; // size is fixed at 64bits
} Line;

typedef Line* Set;
typedef Set* Cache;

size_64** LRU; // least recently used 2d array
Cache *cache; // cache pointer
int s, E, b;
int S; // number of sets
bool v = 0; // verbose tag
int hit, miss, evicts;

void printSummary(int hits, int misses, int evictions);
void print_help_msg(void);
void initialize_cache();
void destroy_cache();
void print_cache();
int load(size_64 address);
int store(size_64 address);
int modify(size_64 address);
void parse_address(size_64 *tag, size_64 *set,
                   size_64 *offset, size_64 address);
void initialize_cache_struct();
Line *find_line(size_64 address);
void initialize_lru();
void destroy_lru();
void print_lru();
void place_in_lru(size_64 tag, size_64 set);
void evict(size_64 tag, size_64 set);

int main(int argc, char* argv[]) {
    char c; // stores command-line argument
    FILE *fp; // file pointer
    char buffer[32]; // line buffer
    hit = 0;
    miss = 0;
    evicts = 0;

    // input loop assigns values to s, E, b, S
    while( (c=getopt(argc,argv,"hvs:E:b:t:")) != -1){
        switch(c){
        case 'h': // help message
            print_help_msg();
            exit(0);
        case 'v': // verbose tag
            v = 1;
            break;
        case 's':
            s = atoi(optarg);
            S = 1 << s;
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            fp = fopen(optarg, "r");
            break;
        case '?':
        default:
            print_help_msg();
            exit(1);
        }
    }

    if (argc < 9 || s == 0 || b == 0 || E == 0) { // are there enough arguments
                                                  // are they valid?
        printf("%s Missing required command line argument\n", argv[0]);
        print_help_msg();
        exit(1);
    }
    
    initialize_cache(); // initialize cache
    initialize_lru(); // initialize least recently used array

    unsigned int address;
    char *token;
    char *rest;
    char command;

    while (fgets(buffer, 32, fp)) { // grabs line
        command = *strtok(buffer, " ");
        token = strtok(NULL, " ,"); // grabs address
        address = strtol(token, &rest, 16); // parses address as hex and converts to int

        if (command == 'L') load(address);
        if (command == 'S') store(address);
        if (command == 'M') modify(address);
    }

    printSummary(hit, miss, evicts);
    destroy_cache(); // free cache
    destroy_lru(); // free LRU queue
    return 0;
}

/*
 * find_line - attempts to mach tag in set given by address
 */
Line *find_line(size_64 address) {
    size_64 location[3];
    parse_address(&location[0], &location[1], &location[2], address);

    for (int i = 0; i < E; i++) { // iterate over lines in set to find tag
        if (cache[location[1]][i]->tag == location[0]) { // are tags same?
            if (cache[location[1]][i]->valid == 1) { // is line valid?
                // valid line found
                return cache[location[1]][i];
            }
        }
    } // if here, no tag found

    return 0x0;
}

/*
 * place - helper function for load and store: 
 *         places address in cache
 */
Line *place(size_64 address) {
    size_64 location[3];
    parse_address(&location[0], &location[1], &location[2], address);

    for (int i = 0; i < E; i++) { // iterate over lines in set to find empty
        if (cache[location[1]][i]->valid == 0) { // is line empty?
                cache[location[1]][i]->tag = location[0]; // copy tag
                cache[location[1]][i]->valid = 1; // validate line
                place_in_lru(location[0], location[1]); // place in LRU queue
                return cache[location[1]][i]; // return location
        }
    } // if here, no empty lines found

    evict(location[0], location[1]); // evict and replace

    return 0x0;
}

/*
 * place_in_lru - places most recently used tag into LRU queue at beginning
 */
void place_in_lru(size_64 tag,
                  size_64 set) {
    int t;
    for (t = E - 1; t >= 0; t--) { // start at second to last to find queue end
        if (LRU[set][t] == tag) {
            break; // found end
        }
    }

    if (t == -1)  // special case, empty
        t = E - 1;
    
    for (int i = t - 1; i >= 0; i --) { // start at second to end of queue
        LRU[set][i+1] = LRU[set][i]; // shift queue
    }

    LRU[set][0] = tag; // insert at beginning
}

/*
 * evict - takes tag and set as input
 *         finds least recently used line
 *         evicts and replaces
 */
void evict(size_64 tag, size_64 set) {
    size_64 lru_tag = LRU[set][E-1]; // grabs LRU tag

    for (int i = 0; i < E; i++) { // iterates over set to find tag
        if (cache[set][i]->tag == lru_tag) { // found
            cache[set][i]->tag = tag; // overwrite tag
            cache[set][i]->valid = 1; // validate
            place_in_lru(tag, set); // place in LRU queue
        }
    }
    if (v == 1) printf("eviction ");
    evicts++;
}

/*
 * load - simulates cache load command
 */
int load(size_64 address) {
    size_64 location[3];
    parse_address(&location[0], &location[1], &location[2], address);
    Line *line = find_line(address); // find line
    if (line == 0x0) { // miss, place or evict
        if (v == 1) printf("miss ");
        miss++;
        place(address);
    } else { // hit, place in LRU queue
        if (v == 1) printf("hit ");
        place_in_lru(location[0], location[1]);
        hit++;
    }
    if (v == 1) printf("\n");
    return 0;
}

/*
 * store - since we are not worried about blocks for this assignment,
 *         load simulates store fully for this purpose
 */
int store(size_64 address) {
    load(address);
    return 0;
}

/*
 * modify - simulate cache modify command
 */
int modify(size_64 address) {
    load(address);
    store(address);
    return 0;
}

/*
 * parse_address - parses address and stores into pointers:
 *                 tag: location[0]
 *                 set: location[1]
 *                 offset: location[2]
 */
void parse_address(size_64 *tag,
                   size_64 *set,
                   size_64 *offset,
                   size_64 address) {
    *offset = address % (1 << b); // calculates block offset bitwise
    *set = (address >> b) % (1 << s); // calculates set bitwise
    *tag = (address >> b) >> s; // calculates tag bitwise
}

/*
 * initialize_cache - allocates cache
 */
void initialize_cache() {
    cache = malloc(S * sizeof(Cache)); // allocate cache container
    for (int i = 0; i < S; i++) { // iterate over sets
        Set *current_set = malloc(E * sizeof(Set)); // allocate set
        for (int j = 0; j < E; j++) { // iterate over lines
            Line *current_line = malloc(sizeof(Line)); // allocate line
            current_line->valid = 0; // initialize valid bit
            current_line->tag = 0; // initialize tag
            current_set[j] = current_line; // assign line
        }
        cache[i] = current_set; // assign set
    }
}

/*
 * destroy_cache - frees cache completely
 */
void destroy_cache() {
    for (int i = 0; i < S; i++) { // iterate sets
        for (int j = 0; j < E; j++) { // iterate lines
            free(cache[i][j]); // free line
        }
        free(cache[i]); // free set
    }
    free(cache); // free main cache
}

/*
 * initialize_lru - LRU queue modeled as 2D array of sets * number of lines
 *                  last entry, LRU[set][E-1] is least recently used
 */
void initialize_lru() {
    LRU = malloc(S * sizeof(size_64*)); // allocate sets
    for (int i = 0; i < S; i++) { // iterate sets
         LRU[i] = malloc(E * sizeof(size_64)); // allocate line tags
         for (int j = 0; j < E; j++) { // iterate lines
             LRU[i][j] = -1; // if tag = -1, LRU entry is empty
         }
    }
}

/*
 * destroy_lru - frees LRU completely
 */
void destroy_lru() {
    for (int i = 0; i < S; i++) { // iterate sets
         free(LRU[i]); // free set
    }    
    free(LRU); // free container
}

/*
 * print_lru - debug functions, prints LRU queue
 */
void print_lru() {
    for (int i = 0; i < S; i++) {
        printf("Set %d: ", i);
        for (int j = 0; j < E; j++) {
            printf("%lu ", LRU[i][j]);
        }
        printf("\n");
    }
}

/*
 * print_cache - debug function, prints cache
 */
void print_cache() {
    for (int i = 0; i < S; i++) {
        printf("Set %d: ", i);
        for (int j = 0; j < E; j++) {
            printf("Line %d valid: %d \n", j, cache[i][j]->valid);
            printf("       ");
            printf("         tag: %lu \n", cache[i][j]->tag);
            printf("       ");
        }
        printf("\n");
    }
}

/* 
 * printSummary - Summarize the cache simulation statistics 
 */
void printSummary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

/*
 * print_help_msg - prints help message to user
 */
void print_help_msg(void) {
    printf("\
Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n\
Options:\n\
  -h         Print this help message.\n\
  -v         Optional verbose flag.\n\
  -s <num>   Number of set index bits.\n\
  -E <num>   Number of lines per set.\n\
  -b <num>   Number of block offset bits.\n\
  -t <file>  Trace file.\n\
\n\
Examples:\n\
  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n\
  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}