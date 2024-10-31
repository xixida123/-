#!/bin/bash

# Specify cross-compiler path
export CC=/home/uisrc/fmsh/FMQL/arm-buildroot-linux-gnueabihf-6.4.0/bin/arm-buildroot-linux-gnueabihf-gcc

# Specify additional library search path
export LD_LIBRARY_PATH=/home/uisrc/fmsh/FMQL/arm-buildroot-linux-gnueabihf-6.4.0/lib:$LD_LIBRARY_PATH

# Define the target name
SOURCE="*"
TARGET="main"
THREADS=${THREADS:-4}  # Default to 4 threads if not specified
CFLAGS="-D_GNU_SOURCE"
INCLUDE_DIRS="-I/home/uisrc/fmsh/code/tcp_queue"
# Build the target
function build {
    echo "Building $TARGET with $THREADS threads..."
    $CC $CFLAGS ${INCLUDE_DIRS} -Wall ${SOURCE}.c -o $TARGET -lpthread -O2 &
    wait
    if [ $? -eq 0 ]; then
        echo "$TARGET compiled successfully."
    else
        echo "Compilation failed."
        exit 1
    fi
}

# Clean the build
function clean {
    echo "Cleaning up..."
    rm -f $TARGET *.o *~
    echo "Clean completed."
}

# Install the target (copy to remote system)
function install {
    echo "Installing $TARGET..."
    scp $TARGET root@192.168.1.36:/root/uisrc/code/
    if [ $? -eq 0 ]; then
        echo "$TARGET installed successfully."
    else
        echo "Installation failed."
        exit 1
    fi
}

# Check the command-line argument to decide the action
if [ "$1" == "clean" ]; then
    clean
elif [ "$1" == "install" ]; then
    build && install
else
    build
fi

