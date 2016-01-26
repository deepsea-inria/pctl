#!/bin/bash

TARGET=$1

(

    cd $TARGET

    git clone https://github.com/deepsea-inria/cmdline.git

    git clone https://github.com/deepsea-inria/chunkedseq.git
    
)
