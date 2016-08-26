## Release Notes

#### 8/25/16: Refactoring to improve code organization
A big set of commits to clean the structure and simplify onvm source code.
We separated all functions into the main.c of the manager into modules :
 - `onvm_stats` : functions displaying statistics
 - `onvm_pkt` : functions related to packet processing
 - `onvm_nf` : functions related to NFs management.

Each module comes with a header file with commented prototypes. And each c and h file has been "cut" into parts :
 - interfaces, or functions called outside of the module
 - internal functions, the functions called only inside the module and doing all the work
 - helper functions, simple and short functions used many times through the module.

**API Changes:**
 - NFs now need to call functions like `onvm_nflib_*` instead of `onvm_nf_*`.  For example, `onvm_nflib_init` instead of `onvm_nf_init`.  The example NFs have all been updated accordingly.
 - NF `Makefiles` need to be updated to find the path to `onvm_nflib`.


#### 4/24/16: Initial Release
Initial source code release.
