#!/bin/bash

# Date : 1/3/2016
# Decode an audio.wav file
# Tool decode : online2-wav-nnet2-latgen-faster
# It read in a wav file and simulates online neural nets.

if [[ $# != 1 ]]; then
	echo "Usage: $0 <audio-dir> "
	exit 1;
fi

audio=$1
srcdir=exp/nnet2_online/nnet_ms_a
graphdir=exp/tri3b/graph

/home/season/kaldi-trunk/src/online2bin/online-audio-server-decode-faster-dnn --do-endpointing=true \
    --online=true \
    --config=${srcdir}_online/conf/online_nnet2_decoding.conf \
    --max-active=7000 --beam=15.0 --lattice-beam=6 \
    --acoustic-scale=0.1 --word-symbol-table=$graphdir/words.txt \
   ${srcdir}_online/final.mdl $graphdir/HCLG.fst "ark:echo audio transcipt:|" "scp:echo transcipt: $audio|" \
   ark:/dev/null
