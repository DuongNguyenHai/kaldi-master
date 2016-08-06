#!/bin/bash

# This script prepares the lang/ directory.

# Decided to do this using something like a real lexicon, although we
# could also have used whole-word models.

locdict=data/local/dict
mkdir -p $locdict

echo "--- Preparation Dictionary ..."

cat >$locdict/lexicon.txt <<EOF
không kh ob ng
một m oc tw
hai h ai
ba b a
bốn b od nw
năm n ad mw
sáu s au
bảy b ay
tám t ab mw
chín ch ib nw
EOF

echo SIL > $locdict/silence_phones.txt
echo SIL > $locdict/optional_silence.txt
echo SIL > $locdict/context_indep.txt

# grep -v -w sil $locdict/lexicon.txt | \
#  awk '{for(n=2;n<=NF;n++) seen[$n]=1; } END{print "sil"; for (w in seen) { print w; }}' \
#  	> $locdict/nonsilence_phones.txt

# list of phones.
cat $locdict/lexicon.txt | awk '{for(n=2;n<=NF;n++) seen[$n]=1; } END{print "sil"; for (w in seen) { print w; }}' \
 > $locdict/phone.list
grep -v -w sil $locdict/phone.list > $locdict/nonsilence_phones.txt


echo -e "!SIL\tSIL" >> $locdict/lexicon.txt
touch 		 $locdict/extra_questions.txt

echo "!!! Dictionary preparation succeeded"

# a : a, á : aa, à : ab, ă : ac, â : ad, ắ : ae, ằ : af,ẳ : ag, ạ : ah, ả : aj, ặ : ak, ấ : al, ầ : am, ẩ : an, ậ : ao 
