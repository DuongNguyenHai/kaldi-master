#!/bin/bash

. path.sh || exit 1

echo "--- Building a language model ..."

lang=data/lang
lang_test=data/lang_test
mkdir -p $lang_test

# Now we prepare a simple grammar G.fst that's a kind of loop of
# digits (no silence in this, since that's handled in L.fst)
# there are 10 options: 0-9 and end-of-sentence.
penalty=`perl -e '$prob = 1/11; print -log($prob); '` # negated log-prob,
  # which becomes the cost on the FST.
( for x in `echo không một hai ba bốn năm sáu bảy tám chín`; do
   echo 0 0 $x $x $penalty   # format is: from-state to-state input-symbol output-symbol cost
 done
 echo 0 $penalty # format is: state final-cost
) | fstcompile --isymbols=$lang/words.txt --osymbols=$lang/words.txt \
   --keep_isymbols=false --keep_osymbols=false |\
   fstarcsort --sort_type=ilabel > $lang/G.fst

cp -a $lang/. $lang_test

# Checking that G is stochastic [note, it wouldn't be for an Arpa]
fstisstochastic data/lang/G.fst || echo Error: G is not stochastic

# Checking that G.fst is determinizable.
fstdeterminize data/lang/G.fst /dev/null || echo Error determinizing G.

# Checking that L_disambig.fst is determinizable.
fstdeterminize data/lang/L_disambig.fst /dev/null || echo Error determinizing L.

# Checking that disambiguated lexicon times G is determinizable
fsttablecompose data/lang/L_disambig.fst data/lang/G.fst | \
   fstdeterminize >/dev/null || echo Error

# Checking that LG is stochastic:
fsttablecompose data/lang/L.fst data/lang/G.fst | \
   fstisstochastic || echo Error: LG is not stochastic.

# Checking that L_disambig.G is stochastic:
fsttablecompose data/lang/L_disambig.fst data/lang/G.fst | \
   fstisstochastic || echo Error: LG is not stochastic.

echo "!!! Succeeded in formatting data."
