
# Install Build Prerequisites

    $ sudo apt update
    $ sudo apt install build-essential cmake git libglib2.0-dev libgstreamer1.0-dev libspdlog libssl-dev nlohmann-json3-dev pkg-config

# Build

    $ git clone https://github.com/hatchbed/just_annotate.git
    $ mkdir -p just_annotate/build
    $ cd just_annotate/build
    $ cmake -DCMAKE_BUILD_TYPE=Release
    $ make -j4 

# Run

    $ ./just_annotate
