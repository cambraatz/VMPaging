/*/////////////////////////////////////////////////////////////////////
File: pager-lru.c
Author:       Andy Sayler
    http://www.andysayler.com
Adopted From: Dr. Alva Couch
    http://www.cs.tufts.edu/~couch/

Project: CSCI 3753 Programming Assignment 4
Create Date: Unknown
Adoption Date: 2012/04/03

Modifier: Cameron Braatz
Modify Date: 2024/12/10

Description:
This file contains a least recently used paging implementation, that 
tracks timestamps (in ticks) for all paging operations. This allows the
algorithm to iteratively find the page that has sat in memory unused
for the longest time, targeting the result for eviction.

The block comments loosely define the pieces of logic seen in the README
file's paging diagrams. Refer to these diagrams to visualize what is 
happening in this code. This paging implementation can be called with 
'./test-lru' after compilation.

*//////////////////////////////////////////////////////////////////////

#include <stdio.h> 
#include <stdlib.h>

#include "simulator.h"

/*/////////////////////////////////////////////////////////////////////
pageit(Pentry q[MAXPROCESSES])

    This paging function handles the logic behind the least recently 
    used (LRU) paging algorithm. It tracks the timestamps of operations
    to maintain the ability to effectively target less active pages for
    eviction.

    It initializes our timsestamp table to a zero matrix to be filled 
    as our algorithm runs. The main for loop iterates all active
    processes, attempting to page in the memory each process needs and
    evicting pages that they (ideally) dont need. Each iteration ends by
    logging a current time stamp (ie: tick) for the process and page 
    being worked on.

    When a page fault occurs due to full memory, the timestamps table 
    is iterated to find the minimum lru_tick from the current processes
    pages actively loaded into memory.

*//////////////////////////////////////////////////////////////////////

void pageit(Pentry q[MAXPROCESSES]) { 
    /* Static vars */
    static int initialized = 0;
    static int tick = 1; // artificial time

    // track each processes last access time...
    static int timestamps[MAXPROCESSES][MAXPROCPAGES];

    /* LRU Paging Algorithm */
    // initialize placeholders for process, program counter and page...
    int proc;
    int pc;
    int page;

    // initialize on first iteration...
    if(!initialized) {
        // iterate through indices of each process...
        for(proc=0; proc < MAXPROCESSES; proc++) {
            // iterate through indices of each page...
            for(page=0; page < MAXPROCPAGES; page++) {
                // for each process/page, set access time to 0...
                timestamps[proc][page] = 0; 
            }
        }
        initialized = 1;
    }

    // initialize old, least recently used pages and old tick index...
    int lru_page;
    int curr_page;
    int lru_tick;

    /* select a process */
    // iterate indices of ALL processes...
    for (proc=0; proc<MAXPROCESSES; proc++) {
        // find active process...
        if (q[proc].active) {
            /* determine current page */
            // isolate program counter of process...
            pc = q[proc].pc;
            // find the pc's target page...
            page = pc / PAGESIZE;

            /* is page swapped in? */
            // verify if page is NOT in memory...
            if (!q[proc].pages[page]) {
                /* call pagein() */
                // attempt paging in target page...
                if (!pagein(proc, page)) {
                    /* select a page to evict */
                    lru_tick = tick;
                    lru_page = -1;

                    // on page fault, iterate pages to find LRU for eviction...
                    for (curr_page=0; curr_page<q[proc].npages; curr_page++) {
                        /* iteratively find minimum timestamp */
                        // if page is in memory...
                        if (q[proc].pages[curr_page]) {
                            // iteratively find the oldest tick/page...
                            if (timestamps[proc][curr_page] < lru_tick) {
                                lru_tick = timestamps[proc][curr_page];
                                lru_page = curr_page;
                            }
                        }
                    }
                    /* call pageout */
                    // swap out LRU page (if found)...
                    if (lru_page != -1) {
                        int success = pageout(proc, lru_page);
                        if (!success) {
                            exit(EXIT_FAILURE);
                        }
                    }
                }
            }
            // update timestamp for the referenced page...
            timestamps[proc][page] = tick;
        }
        // advance time for next iteration...
        tick++;
    }
} 
