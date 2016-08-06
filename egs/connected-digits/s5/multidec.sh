#!/bin/bash

echo

word=$( tr '[:upper:]' '[:lower:]' <<<"$1" )
njobs=1
decode_cmd="run.pl"

srcdir=exp/nnet2_online/nnet_ms_a
# dir=exp/nnet2_online/nnet_a
dataTest=mytest/test-part1

case $word in

	gmm )
		echo "Below is the basic online decoding.  There is no endpointing being done: the utterances"
		echo "are supplied as .wav files.  And the speaker information is known, so we can use adaptation"
		echo "info from previous utterances of the same speaker"
		steps/online/decode.sh --config conf/decode.config --cmd "$decode_cmd" --nj $njobs exp/tri3b/graph \
  		$dataTest exp/tri3b_online/decode;;

	gmm-utt )
		echo	"Below is like the basic online decoding with endpointing, except we treat each utterance separately and"
		echo	"do not carry forward the speaker adaptation state from the previous utterance."
		steps/online/decode.sh --per-utt true --config conf/decode.config \
	  	--cmd "$decode_cmd" --nj $njobs exp/tri3b/graph \
	  	$dataTest exp/tri3b_online/decode_per_utt;;
	  	
	gmm-end )
		echo	"online decoding with endpointing"
		steps/online/decode.sh --do-endpointing true \
	  	--config conf/decode.config --cmd "$decode_cmd" --nj $njobs exp/tri3b/graph \
	  	$dataTest exp/tri3b_online/decode_endpointing;;
	  	
	dnn )
		steps/online/nnet2/decode.sh --config conf/decode.config --cmd "$decode_cmd" --nj $njobs \
		exp/tri3b/graph $dataTest ${srcdir}_online/decode;;
		
	dnn-utt )
		steps/online/nnet2/decode.sh --config conf/decode.config --cmd "$decode_cmd" --nj $njobs \
    	--per-utt true \
    	exp/tri3b/graph $dataTest ${srcdir}_online/decode_per_utt;;

  	* )
		echo "Please choose mode a of decoding : gmm, gmm-utt, dnn, dnn-utt"
		echo "Example : $0 gmm"
esac
