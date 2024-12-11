/*/////////////////////////////////////////////////////////////////////
File: pager-predict.c
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
This file contains a predictive paging implementation. The current
implementation has the capability to compute expected probability 
multiple steps in the future. That said, predicting more than a step
ahead introduces computational overhead that has not proven to produce
reliably improved blocked/compute rates. Looking ahead a single step
provides similar rates while maintaining quick performance.

A dynamic programming approach was partially implemented and tested
poorly in preliminary tests. I still believe this would be the best 
approach, but may be too complicated for this application. This paging
implementation can be called with './test-predict'.

*//////////////////////////////////////////////////////////////////////

#include <stdio.h> 
#include <stdlib.h>

#include "simulator.h"

// define type alias for ease in calling...
typedef int TimeStamps[MAXPROCESSES][MAXPROCPAGES];
typedef int TransitionTable[MAXPROCESSES][MAXPROCPAGES][MAXPROCPAGES];

// variables/type alias for DP solution (abandoned)...
#define MAX_STEPS 50
typedef int DynamicProgTable[MAXPROCESSES][MAXPROCPAGES][MAXPROCPAGES][MAX_STEPS];

/*/////////////////////////////////////////////////////////////////////
approx_probability(TransitionTable transition, 
                int proc, 
                int current_page, 
                int next_page) - inline

    This helper calculates the 'approximate' normalized probability of
    transitioning from the current page to the next page (in a single 
    step). The indexed value stored in 'next' is the total times this 
    transition has occurred. The current value is a frequently used 
    approximation for the total transitions away from the current page, 
    using the number of transitions from and to itself.

    This avoids needing to iterate through all page transitions to get
    a true total. It assumes that any process transitioning to other 
    pages likely does the same with itself (ie: loops). Adding 1
    ensures that this doesnt fail on a divide by zero in case that
    assumption is false.

    score: +/- 0.06 (faster and often better than full)

*//////////////////////////////////////////////////////////////////////

static inline float approx_probability(TransitionTable transition, int proc, int current_page, int next_page) {
    // count of past transitions curr-> next...
    float next_count = (float)transition[proc][current_page][next_page];
    // count of past transitions curr-> curr...
    float current_count = (float)transition[proc][current_page][current_page];

    return next_count/(1+current_count);
}

/*/////////////////////////////////////////////////////////////////////
full_probability(Pentry q[MAXPROCESSES], 
                TransitionTable transition, 
                int proc, 
                int current_page, 
                int next_page) - inline

    This helper calculates the true probability of transition between
    the current page and the next page (in a single step). It uses the 
    same logic above, while using the true counts of all transitions 
    away from the current page rather than an approximation.

    This approach is likely computationally heavy in real application,
    but should give us the most accurate predictions in the context of
    this lab.

    score: +/- 0.06 (slower but consistent performance)

*//////////////////////////////////////////////////////////////////////

static inline float full_probability(Pentry q[MAXPROCESSES], TransitionTable transition, int proc, int current_page, int next_page) {
    // initialize transition count...
    int total_transitions = 0;

    // get total transitions across all pages...
    for (int i=0; i<q[proc].npages; i++) {
        total_transitions += transition[proc][current_page][i];
    }
    // count of past transitions curr-> next...
    float next_count = (float)transition[proc][current_page][next_page];

    return next_count/(1+total_transitions);
}

/*/////////////////////////////////////////////////////////////////////
lookahead_probability(TransitionTable transition, 
                        int proc, 
                        int current_page, 
                        int target_page, 
                        int steps) - inline

    This helper leverages our one-step probability functions above to 
    determine predictions for 'steps'-steps between. It relies on the 
    idea that taking a Markov matrix to the power of 'steps' gets you
    the probability of the transition between current and target in 
    'steps' steps.

    To simulate this dynamically, we find the probability of moving to 
    the an intermediate page (temp_page) and multiply that by the result
    of recursively finding the probability of reaching the target in 
    one less step (since we already know the first step).

*//////////////////////////////////////////////////////////////////////

static inline float lookahead_probability(Pentry q[MAXPROCESSES], TransitionTable transition, int proc, int curr_page, int target_page, int steps) {
    if (steps == 1) {
        return full_probability(q,transition, proc, curr_page, target_page);
    }

    float prob = 0.0;
    for (int temp_page=0; temp_page<MAXPROCPAGES; temp_page++) {
        if (temp_page != curr_page) {
            float temp_prob = full_probability(q, transition, proc, curr_page, temp_page);
            float future_prob = lookahead_probability(q, transition, proc, temp_page, target_page, steps-1);
            prob += temp_prob * future_prob;
        }
    }
    return prob;
}

/*/////////////////////////////////////////////////////////////////////
best_guess(TransitionTable transition, 
                    int proc, 
                    int current_page, 
                    int max_steps)

    This helper leverages our recursive lookahead_probability function
    that returns the probability of transitioning between two states,
    accumulating the total probability across the number of steps
    studied.

    This formulates the overall probability of needing a page in the
    testable future. Unfortunately, this process is computationally 
    heavy and sacrifices accuracy for slower prediction. Given paging
    algorithms are constantly running, this is a problem.

    score: +/- 0.058 (slow performance) - three step lookahead
            +/- 0.058 (faster) - one step lookahead

*//////////////////////////////////////////////////////////////////////

int best_guess(Pentry q[MAXPROCESSES], TransitionTable transition, int proc, int curr_page, int max_steps) {
    // initialize placeholders for the best guesses...
    int best_page = -1;
    float best_prob = 0.0;

    // iterate next pages to find most likely successor...
    for (int next_page=0; next_page<q[proc].npages; next_page++) {
        // skip iteration if next==current (already loaded)...
        if (next_page != curr_page) {
            // initialize total probability count...
            float total_prob = 0.0;

            // find probability from curr->target for each num of steps...
            for (int steps = 1; steps <= max_steps; steps++) {
                total_prob += lookahead_probability(q, transition, proc, curr_page, next_page, max_steps);
            }

            // replace prob/page if improved...
            if (total_prob > best_prob) {
                best_prob = total_prob;
                best_page = next_page;
            }
        }
    }

    return best_page;
}

/*/////////////////////////////////////////////////////////////////////
** inactive **
swap_page(Pentry q[MAXPROCESSES], 
            TimeStamps timestamps, 
            int proc, 
            int page, 
            int tick) - (FIND_LRU macro has symmetric logic)

    This helper packages the LRU page swapping policy from our previous
    implementation to simplify our main code with the predictive
    approach. This strategy requires two swaps per iteration and was
    seriously slimmed down by containing this logic.

    It calls pagein() to load in a specific process's page, handling
    page faults using a timestamp based LRU policy. The process's pages
    are iterated to find the page IN-MEMORY that has the lowest time
    stamp. Once found, the page is paged out it the paging in is 
    attempted again. 
    
    Unfortunately, both the inline and macro implementations nearly 
    double the rate of blocked/compute time. Although the logic is 
    identical, it seems that this approach suffers from inefficiencies
    in compiling/optimization overhead despite attempts to inline.

*//////////////////////////////////////////////////////////////////////

static inline int swap_page(Pentry q[MAXPROCESSES], TimeStamps timestamps, int proc, int page, int tick) {
    // use LRU to evict on page fault...
    int lru_tick = tick;
    int lru_page = -1;

    // on paging fail, iterate pages to find LRU for eviction...
    for (int curr_page = 0; curr_page < q[proc].npages; curr_page++) {
        // if page is in memory...
        if (q[proc].pages[curr_page] && curr_page != page) {
            // iteratively find the oldest tick/page...
            if (timestamps[proc][curr_page] < lru_tick) {
                lru_tick = timestamps[proc][curr_page];
                lru_page = curr_page;
            }
        }
    }

    // swap out LRU page (if found)...
    if (lru_page != -1) {
        int success = pageout(proc, lru_page);
        if (!success) {
            return 0;
        }
    }

    return lru_page;
}

#define FIND_LRU(q, timestamps, proc, page, tick) {\
    if (!pagein(proc, predicted_page)) {\
        lru_tick = tick;\
        lru_page = -1;\
        for (int curr_page = 0; curr_page < q[proc].npages; curr_page++) {\
            if (q[proc].pages[curr_page] && curr_page != page) {\
                if (timestamps[proc][curr_page] < lru_tick) {\
                    lru_tick = timestamps[proc][curr_page];\
                    lru_page = curr_page;\
                }\
            }\
        }\
        if (lru_page != -1) {\
            pageout(proc, lru_page);\
        }\
    }\
}

/*/////////////////////////////////////////////////////////////////////
pageit(Pentry q[MAXPROCESSES])

    This paging function handles the logic behind the predictive paging
    algorithm. It builds prediction into the previous LRU implementation.
    This allows the prediction logic to be developed on top of a tested
    LRU paging algorithm, to effective manage page faults when the 
    predictions are incorrect.

    It initializes our transition table to a zero matrix to be filled 
    as our algorithm runs. It iteratively analyzes each active process
    to predict which pages are most likely to occurr in the near future.
    LRU replacement is used in the event of any page faults.

    Once predicted pages are loaded into memory, we repeat the process
    to determine which pages each process has immediate need for. Again
    using the LRU policy to handle any page faults. This second process
    ensures that any short-term needs are ideally met, while ensuring 
    that any leftover pages are filled with our predicted pages.

*//////////////////////////////////////////////////////////////////////

void pageit(Pentry q[MAXPROCESSES]) { 
    /* Static vars */
    static int initialized = 0;
    static int tick = 1; // artificial time

    // track each processes last access time (LRU)...
    static int timestamps[MAXPROCESSES][MAXPROCPAGES];

    // transition matrix (predictive)...
    // each process has a probability/adjacency matrices...
    static TransitionTable transitions;

    // tracks each processes current page...
    static int current_page[MAXPROCESSES];

    /* Local vars */
    // initialize placeholders for process, program counter and page...
    int proc;
    int pc;
    int page;

    // initialize on first iteration...
    if (!initialized) {
        // iterate through indices of each process...
        for (proc=0; proc<MAXPROCESSES; proc++) {
            // set all to -1; no active processes yet...
            current_page[proc] = -1;

            for (page=0; page<MAXPROCPAGES; page++) {
                // for each process/page, set access time to 0...
                timestamps[proc][page] = 0; 

                // initialize all markov probabilities to 0...
                for (int next_page=0; next_page<MAXPROCPAGES; next_page++) {
                    transitions[proc][page][next_page] = 0;
                }
            }
        }
        initialized = 1;
    }

    // initialize old, least recently used pages and old tick index...
    int lru_page;
    int curr_page;
    int lru_tick;

    // initialize predicted page and max_probability...
    int predicted_page;

    /* select a process */
    for (proc=0; proc<MAXPROCESSES; proc++) {
        // find active process...
        if (q[proc].active) {
            /* determine current page */
            // isolate pc and find its current page...
            pc = q[proc].pc;
            page = pc / PAGESIZE;

            // checks whether current process has been called before...
            if (current_page[proc] != -1) {
                // increment transition between current page and target page...
                transitions[proc][current_page[proc]][page]++;
            }
            // update current processes's page index...
            current_page[proc] = page;

            /* predict the next page */
            // when max_step==1; this only predicts a single step ahead...
            predicted_page = best_guess(q, transitions, proc, page, 1);

            /* load predicted page if not in memory */
            if (predicted_page != -1 && !q[proc].pages[predicted_page]) {
                // attempt paging in target page...
                if (!pagein(proc, predicted_page)) {
                    // use LRU to evict on page fault...
                    lru_tick = tick;
                    lru_page = -1;

                    // on paging fail, iterate pages to find LRU for eviction...
                    for (curr_page = 0; curr_page < q[proc].npages; curr_page++) {
                        // if page is in memory...
                        if (q[proc].pages[curr_page] && curr_page != page) {
                            // iteratively find the oldest tick/page...
                            if (timestamps[proc][curr_page] < lru_tick) {
                                lru_tick = timestamps[proc][curr_page];
                                lru_page = curr_page;
                            }
                        }
                    }

                    if (lru_page != -1) {
                        pageout(proc, lru_page);
                    }
                }
                // attempted inline func/macro, degraded performance...
                //FIND_LRU(q,timestamps,proc,predicted_page,tick);
            }

            /* Handle current page if not in memory */
            if (!q[proc].pages[page]) {
                // attempt paging in target page...
                if (!pagein(proc, page)) {
                    // use LRU to evict on page fault...
                    lru_tick = tick;
                    lru_page = -1;

                    // on paging fail, iterate pages to find LRU for eviction...
                    for (curr_page = 0; curr_page < q[proc].npages; curr_page++) {
                        // if page is in memory...
                        if (q[proc].pages[curr_page] && curr_page != page) {
                            // iteratively find the oldest tick/page...
                            if (timestamps[proc][curr_page] < lru_tick) {
                                lru_tick = timestamps[proc][curr_page];
                                lru_page = curr_page;
                            }
                        }
                    }

                    if (lru_page != -1) {
                        pageout(proc, lru_page);
                    }
                }
            }

            /* Update LRU timestamp */
            timestamps[proc][page] = tick;
        }
    }

    /* Advance time for next iteration */
    tick++;
}