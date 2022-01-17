APP_NAME="hellopng"
INPUT_FILE="gimp_8x8complex" ## try gzipsample or deflate_source
INPUT_EXTENSION="png"

echo "Building $APP_NAME... (this shell script must be run from the app's root directory)"

echo "deleting previous build..."
rm -r -f build

echo "Creating build folder..."
mkdir build

echo "copying resources..."
cp resources/$INPUT_FILE.$INPUT_EXTENSION build/$INPUT_FILE.$INPUT_EXTENSION

echo "Compiling $APP_NAME..."
# clang $MAC_FRAMEWORKS -lstdc++ -std="c99" -o3 -o build/$APP_NAME src/hello$INPUT_EXTENSION.c
gcc -fsanitize=undefined -g -o3 $MAC_FRAMEWORKS -lstdc++ -std="c99" -o build/$APP_NAME src/hello$INPUT_EXTENSION.c

# echo "Running $APP_NAME"
(cd build && time ./$APP_NAME $INPUT_FILE.$INPUT_EXTENSION)

# debugging with gdb
# ls -la build/$APP_NAME
# sudo chmod +x build/$APP_NAME
# sudo gdb --args build/$APP_NAME build/$INPUT_FILE.$INPUT_EXTENSION

# mac os profiling - (imo annoying because you have to open a UI)
# xcrun xctrace record --template='Time Profiler' --launch -- build/$APP_NAME build/$INPUT_FILE.$INPUT_EXTENSION 
# xcrun xctrace record --template='Zombies' --launch -- build/$APP_NAME build/$INPUT_FILE.$INPUT_EXTENSION 

# this tool seems awesome but it doesn't work on macos thx 2 apple
# (cd build && valgrind --leak-check=full --track-origins=yes ./$APP_NAME $INPUT_FILE.$INPUT_EXTENSION)

