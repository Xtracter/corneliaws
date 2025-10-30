#!/bin/bash


if [[ $1 == "--help" ]];
then

echo "shutdown.sh -f,-http,-ssl,-tls <port>"
echo Peaceful shutdown: shutdown.sh port
echo "Forced shutdown: shutdown.sh -f -http|-ssl|-tls or "" for all"

exit 1
fi

if [[ $1 == "-f" ]];
then
	echo Forcing shutdown .. $2
	if [[ $2 == "" ]];
	then
		ps -ef | grep './cornelia_d' | grep -v grep | awk '{print $2}' | xargs -r kill -9
	fi

	if [[ $2 == "http" ]];
	then
		ps -ef | grep './cornelia_d -c' | grep -v grep | awk '{print $2}' | xargs -r kill -9
	fi

	if [[ $2 == "ssl" ]];
	then
		ps -ef | grep './cornelia_d -ssl' | grep -v grep | awk '{print $2}' | xargs -r kill -9
	fi

	if [[ $2 == "tls" ]];
	then
		ps -ef | grep './cornelia_d -tls' | grep -v grep | awk '{print $2}' | xargs -r kill -9
	fi

else
	echo Asking Cornelia for shutdown..
	echo shutdown > $CORNELIA_HOME/corny.loc
	curl -s 127.0.0.1:$1

fi


