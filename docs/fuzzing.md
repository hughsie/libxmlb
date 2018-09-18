Fuzzing
=======

    CC=afl-gcc meson --default-library=static ../
    AFL_HARDEN=1 ninja
    afl-fuzz -m 300 -i fuzzing -o findings ./src/xb-tool --force dump @@

Generating
----------

    mkdir -p fuzzing
    ./src/xb-tool convert ../data/fuzzing-src/appdata.xml fuzzing/appdata.xmlb
