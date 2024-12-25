set -ex
g++ -I ~/lemon/include -std=c++20 example.cpp -g -DNDEBUG -march=native -std=c++11 -Wall -Wno-maybe-uninitialized -L ~/lemon/lib -lemon -o example
