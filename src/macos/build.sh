APP_NAME="helloimage"
PLATFORM="macos"
IMAGE_NAME="fs_angrymob"
IMAGE_TYPE="png"

echo "Building $APP_NAME for $PLATFORM... (this shell script must be run from the app's root directory)"

echo "deleting previous build..."
rm -r -f build/$PLATFORM

echo "Creating build folder..."
mkdir build
mkdir build/$PLATFORM
mkdir build/$PLATFORM/$APP_NAME.app

echo "copying resources..."
cp resources/$IMAGE_NAME.$IMAGE_TYPE build/$PLATFORM/$APP_NAME.app/$IMAGE_NAME.$IMAGE_TYPE

echo "Compiling $APP_NAME..."
clang -g $MAC_FRAMEWORKS -lstdc++ -std="c99" -o build/$PLATFORM/$APP_NAME.app/$APP_NAME src/$PLATFORM/hello$IMAGE_TYPE.c

echo "Running $APP_NAME"
(cd build/$PLATFORM/$APP_NAME.app && ./$APP_NAME $IMAGE_NAME.$IMAGE_TYPE)

