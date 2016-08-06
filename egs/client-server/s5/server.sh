#!/bin/bash

# Server (gmm)

opt=$1

case $opt in
	2 )
	../../../src/onlinebin/online-audio-server-decode-faster \
	--verbose=1 --rt-min=0.5 --rt-max=3.0 --max-active=6000 --beam=20.0 --acoustic-scale=0.083333 \
	exp/tri3b_online/final.mdl exp/tri3b-gmm/graph/HCLG.fst exp/tri3b-gmm/graph/words.txt '1:2:3:4:5' \
	exp/tri3b-gmm/graph/phones/word_boundary.int 5010 exp/tri3b_online/final.mat;;

	* )
	../../../src/online2bin/server-dnn-online --online=true --do-endpointing=true \
	--config=exp/nnet2_online/nnet_ms_a_online/conf/online_nnet2_decoding.conf \
	--max-active=7000 --beam=20.0 --lattice-beam=10.0 --acoustic-scale=0.1 \
	--word-symbol-table=exp/tri3b-dnn/graph/words.txt exp/nnet2_online/nnet_ms_a_online/final.mdl \
	exp/tri3b-dnn/graph/HCLG.fst 5010
esac