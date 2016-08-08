1. Install kaldi
	
-	Look at kaldi-trunk/INSTALL and follow instructions there

2. Go to see src/Duong-fix.md and follow instructions there.

3. Build portaudio

*	In tools directory

-	./install_portaudio.sh 
-	sudo ldconfig

4. Run makefile in src/online

5. build server-dnn-online and client-online

*	In kaldi-master directory

-	./duongmake.sh 3
-	./duongmake.sh 4

6. Copy resource folder to kaldi-master

