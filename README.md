# Heapgame Generator 
This is the repository for the heapgame generator. 

## Compile 
`cd src/preload && make`&& make install 
`cd ../rec && make`&& make install 
`cd ../../test && make`
`make test`  

## Quick Start
Run the console by executing `hth_console`. 
Set the executable you want to analyse by typing `set executable [file]`. 
Furthermore, set the (absolute) location of the preload library by executing `set preload [path]/libhtheap.so`.
Lastly, we can set the output filename by calling `set outfile output.txt`. 
Now we can simply start analysing the program by calling `start`.
Arguments can be specified by appending them to `start`, similar to GDB.  
If we want a fresh run of the program, we simply call `restart` and once we're done we call `stop`. 
For each operation performed onto the target program, We can specify it is a new operation by typing `next`. 

A list of commands can be written to a file. 
These commands will be imperatively executed when given as an argument to the `hth_console`. 


## Custom Allocators
The heapgame generator is able to analyse custom allocators. 
The only caveat is that the preload library needs to be recompiled. 
For example, if our custom allocator uses `my_alloc` and `my_free` instead of `malloc` and `free`, 
we can simply run `cd src/preload && make MALMACRO=my_alloc FREEMACRO=my_free`. 
For various allocators, multiple versions of the library can be made so not to have to recompile whenever swapping. 

This only works if the function prototypes do not change. 
If function prototypes change, the source code needs to be edited unfortunately. 
Furthermore, note that *internal* functions cannot be overridden with `LD_PRELOAD`, 
as local functions are prioritised over dynamically loaded functions. 


## Running outside of Console
Instead of calling `start`, the executable can also be run manually. 
For this, make sure to have the `LD_PRELOAD` environment variable set to the `libhtheap.so`. 
Also, the `HTH_MSQ` environment variable needs to be set. 
This value can be obtained by running `show msqid`. 


## Console Command cheat sheet 
| command | arguments                | result         | 
| ------- | ------------------------ | -------------- |
| `set`     | `debug [1|0]`             | Setting or unsetting debug mode | 
| `set`     | `[config|configfile|c] (filename)`  | Execute commands from a configfile |
| `set`     | `[exec|executable|target] (filename)` | Specify file to run |
| `set`     | `[preload|ld_preload] (filename)` | Specify the absolute location of the preload library `libhtheap.so` |
| `set`     | `[out|outfile] (filename)` | Specify the output file where results will be written to |
| `[save|export]`     |  | Exports the results to the specified file |
| `start` | `(arguments)` | Starts the target process with the given arguments |
| `stop` | | Stops the target process | 
| `restart` | `(arguments)` | Stops the targets process and starts it again, with the given arguments | 
| `custom` | `(string)` | Adds a custom line to the output at the given state | 
| `next` | | Specifies the end of an operation, and hence the start of the next operation | 
| `[h|help]` | | Shows a deprecated help message | 
| `show` | `help` | Shows a deprecated help message | 
| `show` | `version` | Does not work yet | 
| `show` | `child` | Prints the PID of the manager child process | 
| `show` | `msqid` | Prints the message queue id | 
| `show` | `running` | Prints whether the target process is running | 
| `[q|quit|exit]` | | Exit the console |

This table might not be up to date anymore.   

## Full Example
After installing or linking the applications, an example puzzle can be generated from the synthetic example in our paper. 
To do this, go to synthetic-example and run `make puzzle`. Two puzzles will appear, a simplified puzzle and a regular one. 
This puzzle can be entered and played in the website on https://hacktheheap.io/game.htm on the bottom left corner. 
