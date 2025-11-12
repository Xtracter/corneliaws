#!/bin/bash


	if [[ $1 == "" ]];
	then
	echo "shutdown" > $CORNELIA_HOME/stat
	curl -s -k http://localhost:8080 > /dev/null
	curl -s -k https://localhost:8443 > /dev/null
	curl -s -k https://localhost:8444 > /dev/null
	fi


	if [[ $1 == "kill" ]];
	then
		ps -ef | grep './cornelia_d' | grep -v grep | awk '{print $2}' | xargs -r kill -9
	fi

	if [[ $1 == "http" ]];
	then
		ps -ef | grep './cornelia_d -c' | grep -v grep | awk '{print $2}' | xargs -r kill -9
	fi

	if [[ $1 == "ssl" ]];
	then
		ps -ef | grep './cornelia_d -ssl' | grep -v grep | awk '{print $2}' | xargs -r kill -9
	fi

	if [[ $1 == "tls" ]];
	then
		ps -ef | grep './cornelia_d -tls' | grep -v grep | awk '{print $2}' | xargs -r kill -9
	fi



