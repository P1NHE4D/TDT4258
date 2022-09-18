#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef enum {
    dm, fa
} cache_map_t;
typedef enum {
    uc, sc
} cache_org_t;
typedef enum {
    instruction, data
} access_t;

typedef struct {
    uint32_t address;
    access_t accesstype;
} mem_access_t;

typedef struct {
    uint64_t accesses;
    uint64_t hits;
    // You can declare additional statistics if
    // you like, however you are now allowed to
    // remove the accesses or hits
} cache_stat_t;

typedef struct {
    uint64_t *tags;
    uint64_t *valid_tags;
    uint64_t fifo_pointer;
    uint64_t block_count;
} cache_t;

// DECLARE CACHES AND COUNTERS FOR THE STATS HERE

uint32_t cache_size;
uint32_t block_size = 64;
cache_map_t cache_mapping;
cache_org_t cache_org;

// USE THIS FOR YOUR CACHE STATISTICS
cache_stat_t cache_statistics;

/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE *ptr_file) {
    char buf[1000];
    char *token;
    char *string = buf;
    mem_access_t access;

    if (fgets(buf, 1000, ptr_file) != NULL) {
        /* Get the access type */
        token = strsep(&string, " \n");
        if (strcmp(token, "I") == 0) {
            access.accesstype = instruction;
        } else if (strcmp(token, "D") == 0) {
            access.accesstype = data;
        } else {
            printf("Unkown access type\n");
            exit(0);
        }

        /* Get the access type */
        token = strsep(&string, " \n");
        access.address = (uint32_t) strtol(token, NULL, 16);

        return access;
    }

    /* If there are no more entries in the file,
     * return an address 0 that will terminate the infinite loop in main
     */
    access.address = 0;
    return access;
}

/**
 * Initialises a cache based on the block count
 *
 * The data blocks are ignored since they were not required by the assignment
 * @param block_count number of blocks
 * @return a cache with <block_count> blocks
 */
cache_t *init_cache(uint64_t block_count) {
    cache_t *cache = malloc(sizeof(cache_t));
    cache->tags = malloc(block_count * sizeof(uint64_t));
    cache->valid_tags = malloc(block_count * sizeof(uint64_t));
    cache->fifo_pointer = 0;
    cache->block_count = block_count;
    for (uint64_t i = 0; i < block_count; i++) {
        cache->valid_tags[i] = 0;
        cache->tags[i] = 0;
    }
    return cache;
}

int main(int argc, char **argv) {
    // Reset statistics:
    memset(&cache_statistics, 0, sizeof(cache_stat_t));

    /* Read command-line parameters and initialize:
     * cache_size, cache_mapping and cache_org variables
     */
    /* IMPORTANT: *IF* YOU ADD COMMAND LINE PARAMETERS (you really don't need to),
     * MAKE SURE TO ADD THEM IN THE END AND CHOOSE SENSIBLE DEFAULTS SUCH THAT WE
     * CAN RUN THE RESULTING BINARY WITHOUT HAVING TO SUPPLY MORE PARAMETERS THAN
     * SPECIFIED IN THE UNMODIFIED FILE (cache_size, cache_mapping and cache_org)
     */
    if (argc != 4) { /* argc should be 2 for correct execution */
        printf(
                "Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] "
                "[cache organization: uc|sc]\n");
        exit(0);
    } else {
        /* argv[0] is program name, parameters start with argv[1] */

        /* Set cache size */
        cache_size = atoi(argv[1]);

        /* Set Cache Mapping */
        if (strcmp(argv[2], "dm") == 0) {
            cache_mapping = dm;
        } else if (strcmp(argv[2], "fa") == 0) {
            cache_mapping = fa;
        } else {
            printf("Unknown cache mapping\n");
            exit(0);
        }

        /* Set Cache Organization */
        if (strcmp(argv[3], "uc") == 0) {
            cache_org = uc;
        } else if (strcmp(argv[3], "sc") == 0) {
            cache_org = sc;
        } else {
            printf("Unknown cache organization\n");
            exit(0);
        }
    }

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file = fopen("mem_trace.txt", "r");
    if (!ptr_file) {
        printf("Unable to open the trace file\n");
        exit(1);
    }

    /* initialise cache and allocate memory */
    cache_t *cache;
    cache_t *instruction_cache;
    cache_t *data_cache;
    if (cache_org == uc) {
        cache = init_cache(cache_size / block_size);
    }
    else {
        instruction_cache = init_cache((cache_size / 2) / block_size);
        data_cache = init_cache((cache_size / 2) / block_size);
    }

    /* Loop until whole trace file has been read */
    mem_access_t access;
    uint64_t tag;
    while (1) {
        access = read_transaction(ptr_file);
        // If no transactions left, break out of loop
        if (access.address == 0) break;
        printf("%d %x\n", access.accesstype, access.address);
        /* Do a cache access */

        // ADD YOUR CODE HERE

        /* if split cache, cache is used as a pointer to the individual caches based on the instruction */
        if (cache_org == sc) {
            if (access.accesstype == instruction) {
                cache = instruction_cache;
            } else {
                cache = data_cache;
            }
        }

        /* cache access */
        if (cache_mapping == dm) {
            /* get the set (block) position in the cache */
            uint64_t set_pos = (access.address >> 6) & (cache->block_count - 1);

            /* get the tag */
            uint64_t index_bit_count = (uint64_t) log2l(cache->block_count);
            tag = access.address >> (6 + index_bit_count);

            /* check if the cache at set_pos has the same tag and is valid => cache hit */
            if (cache->tags[set_pos] == tag && cache->valid_tags[set_pos]) {
                cache_statistics.hits++;
            } else {
                /* tags are not the same or set is not valid => cache miss */
                cache->tags[set_pos] = tag;
                cache->valid_tags[set_pos] = 1;
            }
            cache_statistics.accesses++;
        } else {
            /* get the tag (corresponds to the address shifted
             * right by 6 bits since we don`t have any index bits */
            tag = access.address >> 6;

            uint8_t cache_full = 1;
            for (uint64_t i = 0; i < cache->block_count; i++) {
                /* check if tag stored in cache is the same as the current tag and if
                 * the block is valid => cache hit; otherwise continue searching*/
                if (cache->tags[i] == tag && cache->valid_tags[i]) {
                    cache_statistics.hits++;
                    cache_full = 0;
                    break;
                }
                /* if a cache block is invalid, all the following cache blocks are invalid
                 * Hence, there is no block in the cache with the same tag => cache miss
                 * Still, the cache is not full, thus, we don´t have to evict any data */
                if (!cache->valid_tags) {
                    cache->tags[i] = tag;
                    cache->valid_tags[i] = 1;
                    cache_full = 0;
                    break;
                }
            }
            /* cache is full. Evict entry at which the fifo pointer is pointing. Increment the
             * pointer afterwards. => cache miss */
            if (cache_full) {
                cache->tags[cache->fifo_pointer] = tag;
                cache->valid_tags[cache->fifo_pointer] = 1;
                cache->fifo_pointer = (cache->fifo_pointer + 1) % cache->block_count;
            }
            cache_statistics.accesses++;
        }
    }

    /* free caches */
    if (cache_org == uc) {
        free(cache);
    } else {
        free(instruction_cache);
        free(data_cache);
    }

    /* Print the statistics */
    // DO NOT CHANGE THE FOLLOWING LINES!
    printf("\nCache Statistics\n");
    printf("-----------------\n\n");
    printf("Accesses: %ld\n", cache_statistics.accesses);
    printf("Hits:     %ld\n", cache_statistics.hits);
    printf("Hit Rate: %.4f\n",
           (double) cache_statistics.hits / cache_statistics.accesses);
    // DO NOT CHANGE UNTIL HERE
    // You can extend the memory statistic printing if you like!

    /* Close the trace file */
    fclose(ptr_file);
}
