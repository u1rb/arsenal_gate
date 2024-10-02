cd `dirname $0`

mkdir -p build

cmake -B build -S . -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ln -sf build/compile_commands.json compile_commands.json

cmake --build build

build/main