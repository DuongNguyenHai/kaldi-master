#!/bin/bash

## check resource error
if [ ! -d $1 -o -L $1 ]; then
	echo "The resource was not found . Resource should be a directory, not be a file or symbol link."
  exit 1;
fi

RESOURCE=$1
DIR=data
TMP=data/tmp
# Tạo folder data
mkdir -p $TMP
# Lấy địa chỉ của tất cả các file .wav và lưu vào file tmp_records.txt
find $RESOURCE -type f -name '*.wav' | sort > $TMP/tmp_records.txt

## Tạo các file tạm
echo "--- Preparing train_wav.scp, train_trans.txt and train.utt2spk ..."
# tạo file chứa tất cả utterance-id
# awk -F'connected-digits-full/|.wav' '{gsub("/","-",$2);print $2}' < $TMP/tmp_records.txt > $TMP/tmp_utterance.txt
awk -F'.wav' '{print $1}' < $TMP/tmp_records.txt | rev | cut -d "/" -f1-3 | rev | tr '/' '-' > $TMP/tmp_utterance.txt
# tạo file chứa tất cả transcipt
awk '{gsub(".wav",".txt",$NF);print $0}' < $TMP/tmp_records.txt | xargs cat | tr "[:upper:]" "[:lower:]" > $TMP/tmp_trans.txt
# tạo file chứa tất cả speaker-id
awk -F'-' '{print $1}' < $TMP/tmp_utterance.txt > $TMP/temp_spkear.txt

## Check error (the total of line in each file has to been the same)
N1=`cat $TMP/tmp_records.txt | wc -l`
N2=`cat $TMP/tmp_utterance.txt | wc -l`
N3=`cat $TMP/tmp_trans.txt | wc -l`

if [ N1 -eq $N2 -a $N1 -eq $N3 ]; then
	echo "Resource error. Please check your data"
	exit 1
fi

mkdir -p $DIR/train
## Tạo các file chính
# tạo file text
paste $TMP/tmp_utterance.txt $TMP/tmp_trans.txt > $DIR/train/text
# tạo file wav
paste $TMP/tmp_utterance.txt $TMP/tmp_records.txt > $DIR/train/wav.scp
# tạo file utt2spk 
paste $TMP/tmp_utterance.txt $TMP/temp_spkear.txt > $DIR/train/utt2spk
# tạo file spk2utt
utils/utt2spk_to_spk2utt.pl $DIR/train/utt2spk > $DIR/train/spk2utt


for x in test ; do
	mkdir -p $DIR/$x
	cp -a $DIR/train/. $DIR/$x
done

rm -R $TMP

echo "!!! Data preparation succeeded"

# utils/validate_data_dir.sh --no-text --no-feats data/train || exit 1;

