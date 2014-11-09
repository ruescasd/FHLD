FHLD (Full Homomorphic Liquid Democracy)
====

Building HElib (see http://tommd.github.io/posts/HELib-Intro.html)
=====

wget http://www.shoup.net/ntl/ntl-6.2.1.tar.gz
tar xzf ntl-6.2.1.tar.gz
cd ntl-6.2.1.tar.gz
./configure && make && sudo make install # or something like this

git clone git://github.com/shaih/HElib.git
cd HElib/src
make

Setting up FHLD
=====

Edit the set_env file and set the HELIB variable path according to your path

Running
=====

./run.sh

How it works
======