Fuzzing
=======

    CC=afl-gcc meson --default-library=static ../
    AFL_HARDEN=1 ninja

Breaking XMLb
-------------

    afl-fuzz -m 300 -i fuzzing-src -o findings ./src/xb-tool --force dump @@
    mkdir -p fuzzing-src
    ./src/xb-tool compile fuzzing-src/appdata.xmlb ../data/fuzzing-src/appdata.xml

Breaking XPath
--------------

    ./src/xb-tool compile xpath.xmlb ../data/fuzzing-src/appdata.xml
    afl-fuzz -m 300 -i ../data/fuzzing-xpath/ -o findings ./src/xb-tool query-file xpath.xmlb @@
