#!/bin/bash

TARGET=$1

(

    cd $TARGET

    git clone https://github.com/deepsea-inria/pctl.git

    ./pctl/script/get-dependencies.sh $TARGET
    
)

