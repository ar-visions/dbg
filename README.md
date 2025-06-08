# dbg component (A-type, universal object for C or C++)
- componentized debugger using LLDB api; accessed by universal object

# example use-case
```c
#include <dbg>

object on_break(dbg debug, path source, i32 line, i32 column) {
	// column is -1 if we do not have this information
	print("breakpoint hit on %o:%i:%i", source, line, column);
	print("arguments: %o ... locals: %o ... statics: %o ... globals: %o ... registers: %o ... this/self: %o", 
	cont(debug);
	return null;
}

int main(int argc, symbol argv[]) {
	map args  = A_arguments(argc, argv);
    	dbg debug = dbg(
		location,  f(path, "%o", get(args, string("binary")),
		arguments, f(string, "--an-arg %i", 1));
	break_line(debug, 4, on_break);
	start(debug);
	while(running(debug)) {
		usleep(1000000);
	}
}
```

orbiter
[https://github.com/ar-visions/orbiter.git]

hyperspace
spatial dev kit, ai module & training scripts
[https://github.com/ar-visions/hyperspace.git]
