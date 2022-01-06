APP_NAME="hellogzip"
INPUT_FILE="gzipsample" ## try gzipsample or deflate_source
INPUT_EXTENSION="gz"

echo "Building $APP_NAME... (this shell script must be run from the app's root directory)"

echo "deleting previous build..."
rm -r -f build

echo "Creating build folder..."
mkdir build

echo "copying resources..."
cp resources/$INPUT_FILE.$INPUT_EXTENSION build/$INPUT_FILE.$INPUT_EXTENSION

echo "Compiling $APP_NAME..."
# clang -g $MAC_FRAMEWORKS -lstdc++ -std="c99" -o build/$APP_NAME src/hello$INPUT_EXTENSION.c
clang -fsanitize=undefined -g $MAC_FRAMEWORKS -lstdc++ -std="c99" -o build/$APP_NAME src/hello$INPUT_EXTENSION.c


echo "Running $APP_NAME"
(cd build && ./$APP_NAME $INPUT_FILE.$INPUT_EXTENSION)

