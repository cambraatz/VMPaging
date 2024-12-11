# CS3753 (Operating Systems)
## Programming Assignment 4
University of Colorado Boulder<br>
Fall 2024<br>

Project Completed by: Cameron Braatz 12/10/2024

### Public Source Code:
By Andy Sayler - 2012
http://www.andysayler.com

#### Adopted From:
Assignment by Dr. Alva Couch
http://www.cs.tufts.edu/~couch/

#### With help from:
Junho Ahn - 2012


## Project Structure
### Folders
handout - Assignment description and documentation

### Files
- `Makefile` - GNU makefile to build all relevant code
- `pager-basic.c` - Basic paging strategy implementation that runs one process at a time.
- `pager-lru.c` - LRU paging strategy implementation (you code this).
- `pager-predict.c` - Predictive paging strategy implementation (you code this).
- `api-test.c` - A `pageit()` implmentation that tests that simulator state changes
- `simulator.c` - Core simualtor code (look but don't touch)
- `simulator.h` - Exported functions and structs for use with simulator
- `programs.c` - Defines test "programs" for simulator to run
- `pgm*.pseudo` - Pseudo code of test programs from which `programs.c` was generated.
- `pager-predict.c` - .zip file containing; `Makefile`, `pager-lru.c`, `pager-predict.c` and this `README` prepared for grading.


### Executables
- `test-*` - Runs simulator using "programs" defined in `programs.c` and paging strategy defined in `pager-*.c`. Includes various run-time options. Run with '-help' for details.
- `test-api` - Runs a test of the simulator state changes

### Examples
Build:<br>
 `make`

Clean:<br>
 `make clean`

View test options:<br>
 `./test-* -help`

Run pager-basic test:<br>
 `./test-basic`

Run API test:<br>
 `./test-api`

Run LRU Paging test:<br>
 `./test-lru`

Run Predictive Paging test:<br>
 `./test-predict`
