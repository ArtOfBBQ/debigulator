APP_NAME="hellogz"
INPUT_FILE="gimp_test" ## try gzipsample or gimp_test
INPUT_EXTENSION="png" ## try gimp_test, fs_angrymob or gzipsample.gz
ADDITIONAL_SOURCES="src/decode_png.c src/inflate.c src/decode_gz.c"

echo "Building $APP_NAME... (this shell script must be run from the app's root directory)"

echo "deleting previous build..."
rm -r -f build

echo "Creating build folder..."
mkdir build

echo "copying resources..."
cp resources/$INPUT_FILE.$INPUT_EXTENSION build/$INPUT_FILE.$INPUT_EXTENSION

echo "Compiling $APP_NAME..."
clang -Weverything -Wno-padded -Wno-gnu-empty-initializer -Wno-poison-system-directories -lstdc++ -std="c99" -o3 -o build/$APP_NAME src/hello$INPUT_EXTENSION.c $ADDITIONAL_SOURCES
# clang++ -std="c++17" -Wno-padded -Wno-gnu-empty-initializer -Wno-poison-system-directories -lstdc++ -o build/$APP_NAME src/hello$INPUT_EXTENSION.c $ADDITIONAL_SOURCES
# gcc -fsanitize=undefined -g -o3 $MAC_FRAMEWORKS -lstdc++ -std="c99" -o build/$APP_NAME src/hello$INPUT_EXTENSION.c

# echo "Running $APP_NAME"
(cd build && time ./$APP_NAME $INPUT_FILE.$INPUT_EXTENSION)

# debugging with gdb
# sudo chmod +x build/$APP_NAME
# sudo gdb --args build/$APP_NAME build/$INPUT_FILE.$INPUT_EXTENSION

# mac os profiling from cli
# this is the xcode tool that coincidentally benefits from valgrind
# suddenly not working on mac os
# (cd build && leaks --atExit -- ./$APP_NAME $INPUT_FILE.$INPUT_EXTENSION)

# mac os profiling - (imo annoying because you have to open a UI)
# xcrun xctrace record --template='Time Profiler' --launch -- build/$APP_NAME build/$INPUT_FILE.$INPUT_EXTENSION 
# xcrun xctrace record --template='Zombies' --launch -- build/$APP_NAME build/$INPUT_FILE.$INPUT_EXTENSION 

# this tool seems awesome but it doesn't work on macos thx apple
# (cd build && valgrind --leak-check=full --track-origins=yes ./$APP_NAME $INPUT_FILE.$INPUT_EXTENSION)

