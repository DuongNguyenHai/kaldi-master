#!/bin/bash

# Copyright 2014 (author: Hainan Xu, Ahmed Ali)
# Apache 2.0

. ./path.sh
. ./cmd.sh

nJobs=40
nDecodeJobs=40

AUDIO_PATH=/export/corpora5/LDC/LDC2013S08/
TEXT_PATH=/export/corpora5/LDC/LDC2013T20/

galeData=GALE/

# You can run the script from here automatically, but it is recommended to run the data preparation,
# and features extraction manually and and only once.
# By copying and pasting into the shell.

local/gale_data_prep_audio.sh $galeData $AUDIO_PATH 
  
local/gale_data_prep_txt.sh  $galeData $TEXT_PATH

local/gale_data_prep_split.sh $galeData 

local/gale_prep_dict.sh 

utils/prepare_lang.sh data/local/dict "<UNK>" data/local/lang data/lang   

local/gale_train_lms.sh

local/gale_format_data.sh 

# Now make MFCC features.
# mfccdir should be some place with a largish disk where you
# want to store MFCC features.
mfccdir=mfcc

for x in train dev ; do
  steps/make_mfcc_pitch.sh --cmd "$train_cmd" --nj $nJobs \
    data/$x exp/make_mfcc/$x $mfccdir
  utils/fix_data_dir.sh data/$x # some files fail to get mfcc for many reasons
  steps/compute_cmvn_stats.sh data/$x exp/make_mfcc/$x $mfccdir
done

# Let's create a subset with 10k segments to make quick flat-start training:
utils/subset_data_dir.sh data/train 10000 data/train.10K || exit 1;

# Train monophone models on a subset of the data, 10K segment
# Note: the --boost-silence option should probably be omitted by default
steps/train_mono.sh --nj 40 --cmd "$train_cmd" \
  data/train.10K data/lang exp/mono || exit 1;

# Get alignments from monophone system.
steps/align_si.sh --nj $nJobs --cmd "$train_cmd" \
  data/train data/lang exp/mono exp/mono_ali || exit 1;

# train tri1 [first triphone pass]
steps/train_deltas.sh --cmd "$train_cmd" \
  2500 30000 data/train data/lang exp/mono_ali exp/tri1 || exit 1;

# First triphone decoding
utils/mkgraph.sh data/lang_dev exp/tri1 exp/tri1/graph || exit 1;
steps/decode.sh  --nj $nDecodeJobs --cmd "$decode_cmd" \
  exp/tri1/graph data/dev exp/tri1/decode &

steps/align_si.sh --nj $nJobs --cmd "$train_cmd" \
  data/train data/lang exp/tri1 exp/tri1_ali || exit 1;

# Train tri2a, which is deltas+delta+deltas
steps/train_deltas.sh --cmd "$train_cmd" \
  3000 40000 data/train data/lang exp/tri1_ali exp/tri2a || exit 1;

# tri2a decoding
utils/mkgraph.sh data/lang_dev exp/tri2a exp/tri2a/graph || exit 1;
steps/decode.sh --nj $nDecodeJobs --cmd "$decode_cmd" \
  exp/tri2a/graph data/dev exp/tri2a/decode &

# train and decode tri2b [LDA+MLLT]
steps/train_lda_mllt.sh --cmd "$train_cmd" 4000 50000 \
  data/train data/lang exp/tri1_ali exp/tri2b || exit 1;
utils/mkgraph.sh data/lang_dev exp/tri2b exp/tri2b/graph || exit 1;
steps/decode.sh --nj $nDecodeJobs --cmd "$decode_cmd" exp/tri2b/graph data/dev exp/tri2b/decode &

# Align all data with LDA+MLLT system (tri2b)
steps/align_si.sh --nj $nJobs --cmd "$train_cmd" \
  --use-graphs true data/train data/lang exp/tri2b exp/tri2b_ali  || exit 1;

#  Do MMI on top of LDA+MLLT.
steps/make_denlats.sh --nj $nJobs --cmd "$train_cmd" \
 data/train data/lang exp/tri2b exp/tri2b_denlats || exit 1;
 
steps/train_mmi.sh data/train data/lang exp/tri2b_ali \
 exp/tri2b_denlats exp/tri2b_mmi 

steps/decode.sh  --iter 4 --nj $nJobs --cmd "$decode_cmd"  exp/tri2b/graph \
 data/dev exp/tri2b_mmi/decode_it4 &
steps/decode.sh  --iter 3 --nj $nJobs --cmd "$decode_cmd" exp/tri2b/graph \
 data/dev exp/tri2b_mmi/decode_it3 & # Do the same with boosting.

steps/train_mmi.sh --boost 0.1 data/train data/lang exp/tri2b_ali \
exp/tri2b_denlats exp/tri2b_mmi_b0.1 

steps/decode.sh  --iter 4 --nj $nJobs --cmd "$decode_cmd" exp/tri2b/graph \
 data/dev exp/tri2b_mmi_b0.1/decode_it4 & 
steps/decode.sh  --iter 3 --nj $nJobs --cmd "$decode_cmd" exp/tri2b/graph \
 data/dev exp/tri2b_mmi_b0.1/decode_it3 &

# Do MPE.
steps/train_mpe.sh data/train data/lang exp/tri2b_ali exp/tri2b_denlats exp/tri2b_mpe || exit 1;

steps/decode.sh  --iter 4 --nj $nDecodeJobs --cmd "$decode_cmd" exp/tri2b/graph \
 data/dev exp/tri2b_mpe/decode_it4 &

steps/decode.sh  --iter 3 --nj $nDecodeJobs --cmd "$decode_cmd" exp/tri2b/graph \
 data/dev exp/tri2b_mpe/decode_it3 &

# From 2b system, train 3b which is LDA + MLLT + SAT.
steps/train_sat.sh --cmd "$train_cmd" \
  5000 100000 data/train data/lang exp/tri2b_ali exp/tri3b || exit 1;
utils/mkgraph.sh data/lang_dev exp/tri3b exp/tri3b/graph|| exit 1;
steps/decode_fmllr.sh --nj $nDecodeJobs --cmd "$decode_cmd" \
  exp/tri3b/graph data/dev exp/tri3b/decode &

# From 3b system, align all data.
steps/align_fmllr.sh --nj $nJobs --cmd "$train_cmd" \
  data/train data/lang exp/tri3b exp/tri3b_ali || exit 1;

## SGMM (subspace gaussian mixture model), excluding the "speaker-dependent weights"
steps/train_ubm.sh --cmd "$train_cmd" 700 \
 data/train data/lang exp/tri3b_ali exp/ubm5a || exit 1;
 
steps/train_sgmm2.sh --cmd "$train_cmd" 5000 20000 data/train data/lang exp/tri3b_ali \
  exp/ubm5a/final.ubm exp/sgmm_5a || exit 1;

utils/mkgraph.sh data/lang_dev exp/sgmm_5a exp/sgmm_5a/graph || exit 1;

steps/decode_sgmm2.sh --nj $nDecodeJobs --cmd "$decode_cmd" --config conf/decode.config \
  --transform-dir exp/tri3b/decode exp/sgmm_5a/graph data/dev exp/sgmm_5a/decode &

steps/align_sgmm2.sh --nj $nJobs --cmd "$train_cmd" --transform-dir exp/tri3b_ali \
  --use-graphs true --use-gselect true data/train data/lang exp/sgmm_5a exp/sgmm_5a_ali || exit 1;

## boosted MMI on SGMM
steps/make_denlats_sgmm2.sh --nj $nJobs --sub-split 30 --beam 9.0 --lattice-beam 6 \
  --cmd "$decode_cmd" --transform-dir \
  exp/tri3b_ali data/train data/lang exp/sgmm_5a_ali exp/sgmm_5a_denlats || exit 1;
  
steps/train_mmi_sgmm2.sh --cmd "$train_cmd" --num-iters 8 --transform-dir exp/tri3b_ali --boost 0.1 \
  data/train data/lang exp/sgmm_5a exp/sgmm_5a_denlats exp/sgmm_5a_mmi_b0.1
 
#decode GMM MMI
utils/mkgraph.sh data/lang_dev exp/sgmm_5a_mmi_b0.1 exp/sgmm_5a_mmi_b0.1/graph || exit 1;

steps/decode_sgmm2.sh --nj $nDecodeJobs --cmd "$decode_cmd" --config conf/decode.config \
  --transform-dir exp/tri3b/decode exp/sgmm_5a_mmi_b0.1/graph data/dev exp/sgmm_5a_mmi_b0.1/decode &
  
for n in 1 2 3 4; do
  steps/decode_sgmm2_rescore.sh --cmd "$decode_cmd" --iter $n --transform-dir exp/tri3b/decode data/lang_dev \
    data/dev exp/sgmm_5a_mmi_b0.1/decode exp/sgmm_5a_mmi_b0.1/decode$n
  
  steps/decode_sgmm_rescore.sh --cmd "$decode_cmd" --iter $n --transform-dir exp/tri3b/decode data/lang_dev \
    data/dev exp/sgmm_5a/decode exp/sgmm_5a_mmi_onlyRescoreb0.1/decode$n
done

local/nnet/run_dnn.sh

time=$(date +"%Y-%m-%d-%H-%M-%S")
#get WER
for x in exp/*/decode*; do [ -d $x ] && grep WER $x/wer_* | utils/best_wer.sh; \
done | sort -n -r -k2 > RESULTS.$USER.$time # to make sure you keep the results timed and owned

#get detailed WER; reports, conversational and combined
local/split_wer.sh $galeData > RESULTS.details.$USER.$time

echo training succedded
exit 0





