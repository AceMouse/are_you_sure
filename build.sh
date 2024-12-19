set -ex
#g++ are_you_sure.cpp -Wall -g -o are_you_sure
g++ -I ~/lemon/include example.cpp -g -O3 -DNDEBUG -march=native -std=c++11 -Wall -Wno-maybe-uninitialized -L ~/lemon/lib -lemon -o example
#g++ -I ~/lemon/include similarity_sort.cpp -g -O3 -DNDEBUG -march=native -std=c++11 -Wall -Wno-maybe-uninitialized -L ~/lemon/lib -lemon -o similarity_sort
