#!/bin/bash

# Client (gmm)

opt=$1

case $opt in
	2 )
		../../../src/onlinebin/online-audio-client --htk --vtt \
		localhost 5010 scp:mytest/test-one/split1/1/wav.scp;;
	* )
	../../../src/onlinebin/client-online \
		localhost 5010 scp:mytest/test-one/split1/1/wav.scp
esac
