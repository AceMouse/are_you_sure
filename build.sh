set -ex
g++ -I ~/lemon/include example.cpp -O3 -DNDEBUG -march=native -std=c++11 -Wall -Wno-maybe-uninitialized -L ~/lemon/lib -lemon -o example
