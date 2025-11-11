#!/bin/bash

touch $CORNELIA_HOME/conf/corny.conf
curl -s -out-null localhost:8080 > /dev/null
