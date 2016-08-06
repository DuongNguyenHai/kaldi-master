#!/bin/bash

. ./path.sh
. ./cmd.sh

njobs=1

rm -rf data mfcc exp

# prepare data for training
local/digit_data_prep.sh $RESOURCEDATA || exit 1
# utils/fix_data_dir.sh data/train
local/digit_prepare_dict.sh

# --share-silence-phones false
utils/prepare_lang.sh data/local/dict '!SIL' data/local/lang data/lang
local/digit_prepare_lm.sh

mfccdir=mfcc

# Feature extraction
for x in test train; do 
 steps/make_mfcc.sh --cmd "$train_cmd" --nj $njobs \
   data/$x exp/make_mfcc/$x $mfccdir || exit 1;
 steps/compute_cmvn_stats.sh data/$x exp/make_mfcc/$x $mfccdir || exit 1;
done

# utils/combine_data.sh data/test data/test_{1,2,3,4,5,6}
# steps/compute_cmvn_stats.sh data/test exp/make_mfcc/test $mfccdir


# Train monophone models on a subset of the data
steps/train_mono.sh --nj $njobs --cmd "$train_cmd" \
  data/train data/lang exp/mono

utils/mkgraph.sh --mono data/lang exp/mono exp/mono/graph

steps/decode.sh --config conf/decode.config --nj $njobs --cmd "$decode_cmd" \
  exp/mono/graph data/test exp/mono/decode

for x in exp/*/decode*; do [ -d $x ] && grep WER $x/wer_* | utils/best_wer.sh; done

# Get alignments from monophone system.
steps/align_si.sh --nj $njobs --cmd "$train_cmd" \
  data/train data/lang exp/mono exp/mono_ali

# train tri1 [first triphone pass]
steps/train_deltas.sh --cmd "$train_cmd" \
 159 795 data/train data/lang exp/mono_ali exp/tri1

# decode tri1
utils/mkgraph.sh data/lang exp/tri1 exp/tri1/graph

steps/decode.sh --config conf/decode.config --nj $njobs --cmd "$decode_cmd" \
  exp/tri1/graph data/test exp/tri1/decode

# align tri1
steps/align_si.sh --nj $njobs --cmd "$train_cmd" \
  --use-graphs true data/train data/lang exp/tri1 exp/tri1_ali

# train tri2a [delta+delta-deltas]
steps/train_deltas.sh --cmd "$train_cmd" 159 795 \
 data/train data/lang exp/tri1_ali exp/tri2a

# decode tri2a
utils/mkgraph.sh data/lang exp/tri2a exp/tri2a/graph
steps/decode.sh --config conf/decode.config --nj $njobs --cmd "$decode_cmd" \
  exp/tri2a/graph data/test exp/tri2a/decode

# train and decode tri2b [LDA+MLLT]
steps/train_lda_mllt.sh --cmd "$train_cmd" \
  --splice-opts "--left-context=3 --right-context=3" \
 159 795 data/train data/lang exp/tri1_ali exp/tri2b
utils/mkgraph.sh data/lang exp/tri2b exp/tri2b/graph

steps/decode.sh --config conf/decode.config --nj $njobs --cmd "$decode_cmd" \
   exp/tri2b/graph data/test exp/tri2b/decode

# you could run these scripts at this point, that use VTLN.
# local/run_vtln.sh
# local/run_vtln2.sh

# Align all data with LDA+MLLT system (tri2b)
steps/align_si.sh --nj $njobs --cmd "$train_cmd" --use-graphs true \
   data/train data/lang exp/tri2b exp/tri2b_ali

#  Do MMI on top of LDA+MLLT.
steps/make_denlats.sh --nj $njobs --cmd "$train_cmd" \
  data/train data/lang exp/tri2b exp/tri2b_denlats
steps/train_mmi.sh data/train data/lang exp/tri2b_ali exp/tri2b_denlats exp/tri2b_mmi
steps/decode.sh --config conf/decode.config --iter 4 --nj $njobs --cmd "$decode_cmd" \
   exp/tri2b/graph data/test exp/tri2b_mmi/decode_it4
steps/decode.sh --config conf/decode.config --iter 3 --nj $njobs --cmd "$decode_cmd" \
   exp/tri2b/graph data/test exp/tri2b_mmi/decode_it3

# Do the same with boosting.
steps/train_mmi.sh --boost 0.05 data/train data/lang \
   exp/tri2b_ali exp/tri2b_denlats exp/tri2b_mmi_b0.05
steps/decode.sh --config conf/decode.config --iter 4 --nj $njobs --cmd "$decode_cmd" \
   exp/tri2b/graph data/test exp/tri2b_mmi_b0.05/decode_it4
steps/decode.sh --config conf/decode.config --iter 3 --nj $njobs --cmd "$decode_cmd" \
   exp/tri2b/graph data/test exp/tri2b_mmi_b0.05/decode_it3

# Do MPE.
steps/train_mpe.sh data/train data/lang exp/tri2b_ali exp/tri2b_denlats exp/tri2b_mpe
steps/decode.sh --config conf/decode.config --iter 4 --nj $njobs --cmd "$decode_cmd" \
   exp/tri2b/graph data/test exp/tri2b_mpe/decode_it4
steps/decode.sh --config conf/decode.config --iter 3 --nj $njobs --cmd "$decode_cmd" \
   exp/tri2b/graph data/test exp/tri2b_mpe/decode_it3


## Do LDA+MLLT+SAT, and decode.
steps/train_sat.sh 159 795 data/train data/lang exp/tri2b_ali exp/tri3b
utils/mkgraph.sh data/lang exp/tri3b exp/tri3b/graph
steps/decode_fmllr.sh --config conf/decode.config --nj $njobs --cmd "$decode_cmd" \
  exp/tri3b/graph data/test exp/tri3b/decode

# (
#  utils/mkgraph.sh data/lang_ug exp/tri3b exp/tri3b/graph_ug
#  steps/decode_fmllr.sh --config conf/decode.config --nj $njobs --cmd "$decode_cmd" \
#    exp/tri3b/graph_ug data/test exp/tri3b/decode_ug
# )


# Align all data with LDA+MLLT+SAT system (tri3b)
steps/align_fmllr.sh --nj $njobs --cmd "$train_cmd" --use-graphs true \
  data/train data/lang exp/tri3b exp/tri3b_ali


# We have now added a script that will help you find portions of your data that
# has bad transcripts, so you can filter it out.  Below we demonstrate how to
# run this script.
# steps/cleanup/find_bad_utts.sh --nj $njobs --cmd "$train_cmd" data/train data/lang \
#   exp/tri3b_ali exp/tri3b_cleanup
# The following command will show you some of the hardest-to-align utterances in the data.
# head  exp/tri3b_cleanup/all_info.sorted.txt

## MMI on top of tri3b (i.e. LDA+MLLT+SAT+MMI)
steps/make_denlats.sh --config conf/decode.config \
   --nj $njobs --cmd "$train_cmd" --transform-dir exp/tri3b_ali \
  data/train data/lang exp/tri3b exp/tri3b_denlats
steps/train_mmi.sh data/train data/lang exp/tri3b_ali exp/tri3b_denlats exp/tri3b_mmi

steps/decode_fmllr.sh --config conf/decode.config --nj $njobs --cmd "$decode_cmd" \
  --alignment-model exp/tri3b/final.alimdl --adapt-model exp/tri3b/final.mdl \
   exp/tri3b/graph data/test exp/tri3b_mmi/decode

# Do a decoding that uses the exp/tri3b/decode directory to get transforms from.
steps/decode.sh --config conf/decode.config --nj $njobs --cmd "$decode_cmd" \
  --transform-dir exp/tri3b/decode  exp/tri3b/graph data/test exp/tri3b_mmi/decode2

# demonstration scripts for online decoding.
# local/online/run_gmm.sh
# local/online/run_nnet2.sh
# local/online/run_baseline.sh
# Note: for online decoding with pitch, look at local/run_pitch.sh,
# which calls local/online/run_gmm_pitch.sh

# best of nnet2
local/online/run_nnet2_multisplice.sh
# local/online/run_nnet2_multisplice_disc.sh

# ##some older scripts:
# # local/run_nnet2.sh
# # local/online/run_nnet2_baseline.sh

# ## if you have a WSJ setup, you can use the following script to do joint
# ## RM/WSJ training; this doesn't require that the phone set be the same, it's
# ## a demonstration of a multilingual script.
# local/online/run_nnet2_wsj_joint.sh
# ## and the discriminative-training continuation of the above.
# local/online/run_nnet2_wsj_joint_disc.sh

# ## The following is an older way to do multilingual training, from an
# ## already-trained system.
# #local/online/run_nnet2_wsj.sh
