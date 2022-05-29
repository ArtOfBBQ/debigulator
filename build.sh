APP_NAME="hellopng"
INPUT_FILE="font"
INPUT_EXTENSION="png"
ADDITIONAL_SOURCES="src/decodedimage.c src/decode_png.c src/inflate.c src/decode_gz.c"

MAC_FRAMEWORKS="
    -framework AppKit 
    -framework MetalKit 
    -framework Metal"

echo "Building $APP_NAME... (this shell script must be run from the app's root directory"

echo "deleting previous build..."
rm -r -f build

echo "Creating build folder..."
mkdir build

echo "copying resources..."
# cp resources/$INPUT_FILE.$INPUT_EXTENSION build/$INPUT_FILE.$INPUT_EXTENSION
cp resources/font.png build/font.png
cp resources/fs_angrymob.png build/fs_angrymob.png
cp resources/fs_bribery.png build/fs_bribery.png
cp resources/fs_bridge.png build/fs_bridge.png
cp resources/fs_cannon.png build/fs_cannon.png
cp resources/fs_birdmystic.png build/fs_birdmystic.png
cp resources/structuredart1.png build/structuredart1.png
cp resources/structuredart2.png build/structuredart2.png
cp resources/structuredart3.png build/structuredart3.png

echo "Compiling $APP_NAME..."
# clang -g -Wno-padded -Wno-gnu-empty-initializer -Wno-poison-system-directories -lstdc++ -std="c99" -o3 -o build/$APP_NAME src/concat_pngs.c $ADDITIONAL_SOURCES
clang++ $MAC_FRAMEWORKS -g -o0 -x objective-c++ -objC -std="c++17" -Wno-padded -Wno-gnu-empty-initializer -lstdc++ -o build/$APP_NAME src/hello$INPUT_EXTENSION.c $ADDITIONAL_SOURCES
# gcc -fsanitize=undefined -g -o3 $MAC_FRAMEWORKS -lstdc++ -std="c99" -o build/$APP_NAME src/hello$INPUT_EXTENSION.c
# gcc -fsanitize=undefined -g -Wall -lstdc++ -std="c99" -o3 -o build/$APP_NAME src/hellopng.c $ADDITIONAL_SOURCES


# echo "Running $APP_NAME"
(cd build && ./$APP_NAME $INPUT_FILE.$INPUT_EXTENSION)

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
# (cd build && valgrind --leak-check=full --track-origins=yes ./$APP_NAME $INPUT_FILE.$INPUT_EXTENSION)

