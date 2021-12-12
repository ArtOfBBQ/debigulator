APP_NAME="hellogzip"
IMAGE_NAME="predictable"
IMAGE_TYPE="gz"

echo "Building $APP_NAME... (this shell script must be run from the app's root directory)"

echo "deleting previous build..."
rm -r -f build

echo "Creating build folder..."
mkdir build

echo "copying resources..."
cp resources/$IMAGE_NAME.$IMAGE_TYPE build/$IMAGE_NAME.$IMAGE_TYPE

echo "Compiling $APP_NAME..."
# clang -g $MAC_FRAMEWORKS -lstdc++ -std="c99" -o build/$APP_NAME src/hello$IMAGE_TYPE.c
clang -g $MAC_FRAMEWORKS -lstdc++ -std="c99" -o build/$APP_NAME src/hello$IMAGE_TYPE.c


echo "Running $APP_NAME"
(cd build && ./$APP_NAME $IMAGE_NAME.$IMAGE_TYPE)

