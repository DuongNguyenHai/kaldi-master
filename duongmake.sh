#!/bin/bash

# kaldi-master/duongmake.sh
# You must put this file as subfolder of folder kaldi-master

OPT=$1
D_LIBS=$(pwd)/tools

case $OPT in

	1)	# compile all program in onlinebin
		cd 'src/onlinebin'
		make;;

	2)	# compile all program in online2bin except server-dnn-online
		cd 'src/online2bin'
		make;;

	3)	# complie server-dnn-online
		# The reason for this compile is make file in online2bin cant include portadio, so I have to include it directly
		cd 'src/decoder'; make
		cd '../nnet2'; make
		cd '../feat'; make
		cd '../online2'; make
		cd '../online2bin'
		g++ -msse -msse2 -Wall -I.. -pthread -DKALDI_DOUBLEPRECISION=0 -DHAVE_POSIX_MEMALIGN \
		-Wno-sign-compare -Wno-unused-local-typedefs -Winit-self -DHAVE_EXECINFO_H=1-rdynamic \
		-DHAVE_CXXABI_H -DHAVE_ATLAS -I ../../tools/ATLAS/include \
		-I ../../tools/openfst/include  \
		-I ../../tools/portaudio/install/include -g  -rdynamic \
		-Wl,-rpath=$D_LIBS/openfst/lib \
		server-dnn-online.cc ../online/kaldi-online.a ../online2/kaldi-online2.a \
		../ivector/kaldi-ivector.a ../nnet2/kaldi-nnet2.a ../lat/kaldi-lat.a \
		../decoder/kaldi-decoder.a ../cudamatrix/kaldi-cudamatrix.a \
		../feat/kaldi-feat.a ../transform/kaldi-transform.a ../gmm/kaldi-gmm.a \
		../thread/kaldi-thread.a ../hmm/kaldi-hmm.a ../tree/kaldi-tree.a \
		../matrix/kaldi-matrix.a ../fstext/kaldi-fstext.a ../util/kaldi-util.a \
		../base/kaldi-base.a   -L ../../tools/openfst/lib -lfst \
		/usr/lib/libcblas.so.3 \
		/usr/lib/liblapack_atlas.so.3 -lm -lpthread -ldl -o server-dnn-online;;


	4)	# Compile client-pa for test online-decoding in "voxfore demo"
		cd 'src/onlinebin'
		g++ -msse -msse2 -Wall -I.. -DKALDI_DOUBLEPRECISION=0 -DHAVE_POSIX_MEMALIGN -Wno-sign-compare \
		-Wno-unused-local-typedefs -Winit-self -DHAVE_EXECINFO_H=1 -rdynamic -DHAVE_CXXABI_H -DHAVE_ATLAS \
		-I$D_LIBS/ATLAS/include -I$D_LIBS/openfst/include \
		-Wno-sign-compare -I../../tools/portaudio/install/include -g   \
		-rdynamic -Wl,-rpath=$D_LIBS/openfst/lib client-online.cc \
		../online/kaldi-online.a \
		../lat/kaldi-lat.a ../decoder/kaldi-decoder.a ../feat/kaldi-feat.a ../transform/kaldi-transform.a \
		../gmm/kaldi-gmm.a ../thread/kaldi-thread.a ../hmm/kaldi-hmm.a ../tree/kaldi-tree.a ../matrix/kaldi-matrix.a \
		../util/kaldi-util.a ../base/kaldi-base.a ../../tools/portaudio/install/lib/libportaudio.a \
		-lrt -L$D_LIBS/openfst/lib -lfst /usr/lib/libatlas.so.3 /usr/lib/libf77blas.so.3 \
		/usr/lib/libcblas.so.3 /usr/lib/liblapack_atlas.so.3 -lm -lpthread -ldl -o client-online;;

	*)
		echo "usage : duongmake.sh [option]"
		echo "duongmake.sh 1 : compile all programs in onlinebin "
		echo "duongmake.sh 2 : compile all programs in online2bin except server-dnn-online"
		echo "duongmake.sh 3 : compile server-dnn-online"

esac

