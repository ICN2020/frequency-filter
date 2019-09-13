/**
 * Copyright (c) 2019 Osaka University
 *
 * This software is released under the MIT License.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @author Junji Takemasa
 */

/* Parts of this code excluding "Filter:" are:
 *
 * Copyright (c) 2016, National Institute of Information and Communications
 * Technology (NICT). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the NICT nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NICT AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE NICT OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * filter.c
 */

/*
	filter.c is an implementation of Filter with FIFO cache eviction algorithm. 
	Filter is a simple frequency-based cache admission algorithm, and it is designed so as to achieve both high speed and high cache hit rate on ICN software routers.
*/

/****************************************************************************************
 Include Files
 ****************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <csmgrd/csmgrd_plugin.h>
#include "cache_replace_lib.h"
#include "lib/siphash24.h"

/****************************************************************************************
 Macros
 ****************************************************************************************/



/****************************************************************************************
 Structures Declaration
 ****************************************************************************************/

/***** structure for listing content entries *****/
typedef struct _FifoT_Entry {
	unsigned char 	key[CsmgrdC_Key_Max];	/* key of content entry 					*/
	int 			key_len;				/* length of key 							*/
    int             valid;
} FifoT_Entry;

/****************************************************************************************
 State Variables
 ****************************************************************************************/

static int cache_cap = 0;					/* Maximum number of entries that can be 	*/
											/* listed (it is the same value as the 		*/
											/* maximum value of the cache table) 		*/

/* pointers of functions which stores and removes the content entry into/from the cache */
/* table (implementation of the functions are in a plugin which uses this library) 		*/
static int (*store_api)(CsmgrdT_Content_Entry*);
static void (*remove_api)(unsigned char*, int);

static int              fifo_hand;          /* position of hand                         */
static int              cache_count;        /* number of cache entries                  */
static FifoT_Entry*     cache_entry_list;   /* list for cache entry                     */

/************Filter: Data structures of Filter is declared.*********/
#define THRESHOLD 2

uint64_t *history; //fifo ring
size_t history_tail;
size_t history_cap;
size_t history_count;

uint32_t *counter; //counting bloom fitler with the min-count method
size_t counter_cap;
/******************************************************************/


/****************************************************************************************
 Static Function Declaration
 ****************************************************************************************/

static void fifo_rotate_until_find_empty();
static void fifo_store_entry(CsmgrdT_Content_Entry* entry, int index);
static void fifo_remove_entry(int index, int is_removed);
static void fifo_move_entry(int from_index, int to_index);
static void fifo_rotate_hand();
static void fifo_rotate_hand_back();

/****************************************************************************************
 ****************************************************************************************/

/*--------------------------------------------------------------------------------------
	Init API
----------------------------------------------------------------------------------------*/

int 							/* If the error occurs, this value is a negative value	*/
init (
	int capacity, 							/* Maximum number of entries that can be 	*/
											/* listed (it is the same value as the 		*/
											/* maximum value of the cache table) 		*/
	int (*store)(CsmgrdT_Content_Entry*), 	/* store a content entry API 				*/
	void (*remove)(unsigned char*, int)		/* remove a content entry API 				*/
) {
    fifo_hand = 0;
    cache_count = 0;
    
	/* Records the capacity of cache		*/
	if (capacity < 1) {
		fprintf (stderr, "[FIFO LIB] Invalid Cacacity\n");
		return (-1);
	}
	cache_cap = capacity;

	/* Records store and remove APIs 		*/
	if ((store == NULL) || (remove == NULL)) {
		fprintf (stderr, "[FIFO LIB] Not specified store or remove API\n");
		return (-1);
	}
	store_api 	= store;
	remove_api 	= remove;
    
	/* Creates the memory pool 				*/
    cache_entry_list = (FifoT_Entry*) calloc(cache_cap, sizeof(FifoT_Entry));
    memset(cache_entry_list, 0, sizeof(FifoT_Entry) * cache_cap);
    
    /* Creates lookup table */
    crlib_lookup_table_init(capacity);

	/************Filter: Init Filter's Data Structure.*********/
    //counter	
    counter_cap = capacity * 10;
    counter = (uint32_t*) malloc(counter_cap*sizeof(uint32_t));
    if (counter == NULL){
    	fprintf (stderr, "Filter: init of Filter's counter fails.\n");
	return (-1);
    }
    //history
    history_cap = capacity * 2;
    history = (uint64_t*) malloc(history_cap*sizeof(uint64_t));
    if (history == NULL){
    	fprintf (stderr, "Filter: init of Filter's history fails.\n");
	return (-1);
    }
    history_tail = 0;
    history_count = 0;

    printf("Filter: Filter's init completes with %zd histries, %zd counters and the threshold %d\n", history_cap, counter_cap, THRESHOLD);
	/********************************************************/
	return (0);
}

/*--------------------------------------------------------------------------------------
	Destroy API
----------------------------------------------------------------------------------------*/
void
destroy (
	void
) {
    fifo_hand = 0;
    cache_count = 0;
	cache_cap 		= 0;
	store_api 		= NULL;
	remove_api 		= NULL;
    free(cache_entry_list);
    /*******Filter: free regions of counter and history******/
    free(counter);
    free(history);
    crlib_lookup_table_destroy();
	/********************************************************/
}

/*--------------------------------------------------------------------------------------
	Insert API
----------------------------------------------------------------------------------------*/
void
insert (
	CsmgrdT_Content_Entry* entry			/* content entry 							*/
) {
    if (cache_count < cache_cap) {
        /* when cache is not full, add entry */
        fifo_rotate_until_find_empty();
    } else {
	/**************Filter: Admission****************/
	uint64_t hash = siphash(entry->name,entry->name_len);
	size_t hash_idx = hash % counter_cap;
	if (counter[hash_idx] < 2){//content shoud not be inserted.
		return;
	}
	/***********************************************/
        /* when cache is full, replace entry */
    	fifo_remove_entry(fifo_hand, 0);
    }
    fifo_store_entry(entry, fifo_hand);
    fifo_rotate_hand();
    // printf("Filter: Content is cached at the CS.\n");
}

/*--------------------------------------------------------------------------------------
	Erase API
----------------------------------------------------------------------------------------*/
void
erase (
	unsigned char* key, 					/* key of content entry removed from cache 	*/
											/* table									*/
	int key_len								/* length of the key 						*/
) {
    int index = crlib_lookup_table_search(key, key_len);
    if (index < 0) {
        fprintf(stderr, "[FIFO LIB] failed to erace\n");
        return;
    }
    fifo_remove_entry(index, 1);
    if (cache_count > 0) {
        if (!cache_entry_list[fifo_hand].valid) fifo_rotate_hand_back();
        fifo_move_entry(fifo_hand, index);
    }
}

/*--------------------------------------------------------------------------------------
	Hit API
----------------------------------------------------------------------------------------*/
void
hit (
	unsigned char* key, 					/* key of the content entry hits in the 	*/
											/* cache table 								*/
	int key_len								/* length of the key 						*/
) {
	/**********************Filter: update couter*************************/
	// printf("Filter: Interest hit: [%s]-%d.\n",key,key_len);
 	/*Filter: Update history and counter*/
 	if (history_count >= history_cap){//pop the oldest entry and decrement its counter
	 	uint64_t old_hash = history[history_tail];
 		size_t old_counter_idx = old_hash % counter_cap;
 		counter[old_counter_idx]--;
 		history_count--;
 	}
 	//insert the new entry and increment its counter
 	uint64_t new_hash = siphash(key,(uint64_t)key_len);
	size_t new_counter_idx = new_hash % counter_cap;
	counter[new_counter_idx]++;
	history[history_tail] = new_hash;
 	history_tail = (history_tail +1) % history_cap;
 	history_count++;
	return;
	/******************************************************************/
}

/*--------------------------------------------------------------------------------------
	Miss API
----------------------------------------------------------------------------------------*/
void
miss (
	unsigned char* key, 					/* key of the content entry fails to hit 	*/
											/* in the cache table						*/
	int key_len								/* length of the key 						*/
) {
	/**********************Filter: update couter*************************/
 	// printf("Filter: Interest miss: [%s]-%d.\n",key,key_len);
 	if (history_count >= history_cap){//pop the oldest entry and decrement its counter
	 	uint64_t old_hash = history[history_tail];
 		size_t old_counter_idx = old_hash % counter_cap;
 		counter[old_counter_idx]--;
 		history_count--;
 	}
 	//insert the new entry and increment its counter
 	uint64_t new_hash = siphash(key,(uint64_t)key_len);
	size_t new_counter_idx = new_hash % counter_cap;
	counter[new_counter_idx]++;
	history[history_tail] = new_hash;
 	history_tail = (history_tail +1) % history_cap;
 	history_count++;
	return;
	/******************************************************************/
}

/*--------------------------------------------------------------------------------------
	Status API
----------------------------------------------------------------------------------------*/
void
status (
	void* arg								/* state information						*/
) {
	// printf("Filter: Filter's history: %zd cache: %d\n",history_count, cache_count);
	return;
}


/*--------------------------------------------------------------------------------------
	Static Functions
----------------------------------------------------------------------------------------*/
static void fifo_rotate_until_find_empty() {
    while (cache_entry_list[fifo_hand].valid) {
        fifo_rotate_hand();
    }
}

static void fifo_store_entry(
	CsmgrdT_Content_Entry* entry,
    int index
) {
    unsigned char 	key[CsmgrdC_Key_Max];
    int 			key_len;
    FifoT_Entry*   rsentry;
    
    key_len = csmgrd_name_chunknum_concatenate (
                    entry->name, entry->name_len, entry->chnk_num, key);
    rsentry = &cache_entry_list[index];
    rsentry->key_len = key_len;
    memcpy(rsentry->key, key, key_len);
    rsentry->valid = 1;
    crlib_lookup_table_add(rsentry->key, rsentry->key_len, index);
    (*store_api)(entry);
    cache_count++;
}

static void fifo_remove_entry(
    int index,
    int is_removed
) {
    FifoT_Entry* rsentry;
    rsentry = &cache_entry_list[index];
    crlib_lookup_table_remove(rsentry->key, rsentry->key_len);
    if (!is_removed) (*remove_api)(rsentry->key, rsentry->key_len);
    memset(rsentry, 0, sizeof(FifoT_Entry));
    cache_count--;
}

static void fifo_move_entry(int from_index, int to_index) {
    if (from_index == to_index) return;
    
    FifoT_Entry *from_entry = &cache_entry_list[from_index];
    FifoT_Entry *to_entry = &cache_entry_list[to_index];
    
    crlib_lookup_table_remove(from_entry->key, from_entry->key_len);
    memcpy(to_entry, from_entry, sizeof(FifoT_Entry));
    memset(from_entry, 0, sizeof(FifoT_Entry));
    crlib_lookup_table_add(to_entry->key, to_entry->key_len, to_index);
}

static void fifo_rotate_hand() {
    fifo_hand++;
    if (fifo_hand >= cache_cap) fifo_hand = 0;
}

static void fifo_rotate_hand_back() {
    fifo_hand--;
    if (fifo_hand < 0) fifo_hand = cache_cap - 1;
}
