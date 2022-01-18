# debigulator

"It worked! The de-bigulator worked!"

This is a self-study repository. It can decompress and read gzip and/or .png files,
because those both use the same underlying compression algorithm.

# how to build the examples

You can try just running the build script I'm using
```
bash build.sh
```

or you can directly compile hellogz.c or hellopng.c with clang (or the compiler of your choice)
```
clang src/hellopng.c -o build/hellopng
```
```
clang src/hellogz.c -o build/hellogz
```

# how to run the examples

Just pass the file you want to inspect as an argument
```
build/hellopng resources/fs_angrymob.png
```
```
build/hellogz resources/gzipsample.gz
```


# Example output from the mac os terminal
```
build/hellopng resources/gimp_test.png
```

```
Inspecting file: gimp_test.png
bytes read from raw file: 30522
finished decode_PNG, result was: SUCCESS
rgba values in image: 4194304
pixels in image (info from image header): 1048576
image width: 1024
image height: 1024
average pixel: [248,249,251,158]

░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒
                                                                                
                                                                                
            ░░░░░░                                                              
        ▒▒░░  ░░░░                                                              
      ▒▒██░░▒▒░░▒▒                                                              
      ░░▓▓      ▒▒                                                              
        ░░                                                                      
    ░░                              ░░                                          
                    ▒▒▒▒▓▓░░        ▓▓▒▒              ░░        ░░              
    ░░▒▒          ▓▓▒▒▒▒▓▓  ░░    ░░░░░░                          ▒▒▓▓▒▒        
    ░░  ▒▒      ░░▓▓▒▒░░░░░░▓▓░░▒▒░░  ▒▒      ░░██░░  ▒▒        ▓▓░░░░▒▒░░▒▒▒▒▒▒
      ▒▒    ██▒▒  ▒▒  ░░  ▓▓██    ░░░░░░  ▒▒░░░░      ░░      ░░██      ▓▓    ▓▓
        ░░  ▒▒██▒▒▒▒▒▒  ░░░░          ▒▒    ░░░░    ░░▓▓      ░░▒▒▓▓▒▒  ▒▒      
                                                  ▒▒░░▒▒░░▓▓░░        ░░▒▒      
                                                            ▓▓░░    ▒▒░░▒▒      
                                                            ▒▒▒▒  ░░▒▒          
                                                            ▒▒░░░░░░            
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
real	0m0.285s
user	0m0.075s
sys	0m0.004s
```

The original image:
![alt text](https://github.com/ArtOfBBQ/debigulator/blob/main/resources/gimp_test.png?raw=true)

Close enough ;p

# Which files do I need if I only want to read a .png file?
```
#include "inflate.h"
#include "decode_png.h"
```

# Which files do I need if I only want to read a .gz file?
```
#include "inflate.h"
#include "decode_gz.h"
```

# Where can I get a full explanation of how this works?

You can see Casey Muratori's mind-bogglingly amazing lessons,
which should cost thousands of $ but are somehow free, on youtube:
https://youtu.be/lkEWbIUEuN0
... or use the query function on his website, https://handmadehero.org
, to search for whatever you want to learn.

Or if you like to torture yourself and slowly die 1000 deaths inside
you can read the official documentation:
https://www.w3.org/TR/2003/REC-PNG-20031110/

