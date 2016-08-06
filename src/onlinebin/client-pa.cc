// onlinebin/ client-pa.cc

// Copyright 2012 Cisco Systems (author: Matthias Paulik)
// Copyright 2013 Polish-Japanese Institute of Information Technology (author: Danijel Korzinek)

//   Modifications to the original contribution by Cisco Systems made by:
//   Nguyen Hai Duong
//   Mar 2016
//   Recognize directly through microphone
//   server : online2bin/server-dnn-online.cc

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <fstream>
#if !defined(_MSC_VER)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include "util/parse-options.h"
#include "util/kaldi-table.h"
#include "feat/wave-reader.h"
#include "online/online-audio-source.h"

// Duong
#include <sys/stat.h>
#include <typeinfo>
#include <stdio.h>
#include <stdlib.h>

namespace kaldi {

bool WriteFull(int32 desc, char* data, int32 size);
bool ReadLine(int32 desc, std::string* str);
std::string TimeToTimecode(float time);

}  //namespace kaldi

int main(int argc, char** argv) {

  using namespace kaldi;
  typedef kaldi::int32 int32;
  // Up to delta-delta derivative features are calculated (unless LDA is used)
  const int32 kDeltaOrder = 2;
  // Time out interval for the PortAudio source
  const int32 kTimeout = 500; // half second
  // Input sampling frequency is fixed to 16KHz
  const int32 kSampleFreq = 16000;
  // PortAudio's internal ring buffer size in bytes
  const int32 kPaRingSize = 32768;
  // Report interval for PortAudio buffer overflows in number of feat. batches
  const int32 kPaReportInt = 4;
  int32 channel = -1;
  int32 packet_size = 2048;

  #if !defined(_MSC_VER)
  try {

    const char *usage =
        "Sends an audio file to the KALDI audio server (onlinebin/online-audio-server-decode-faster)\n"
            "and prints the result on terminal \n\n"
            "e.g.: ./online-audio-client 192.168.50.12 9012 'scp:wav_files.scp'\n\n";
    ParseOptions po(usage);

    po.Register(
        "channel", &channel,
        "Channel to extract (-1 -> expect mono, 0 -> left, 1 -> right)");
    po.Register("packet-size", &packet_size, "Send this many bytes per packet");

    po.Read(argc, argv);
    if (po.NumArgs() != 3) {
      po.PrintUsage();
      return 1;
    }

    std::string server_addr_str = po.GetArg(1);
    std::string server_port_str = po.GetArg(2);
    int32 server_port = strtol(server_port_str.c_str(), 0, 10);
    std::string wav_rspecifier = po.GetArg(3);

    int32 client_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (client_desc == -1) {
      std::cerr << "ERROR: couldn't create socket!" << std::endl;
      return -1;
    }
    // std::cout << "server_addr_str : " << server_addr_str <<"\n"
    //           << "server_port : " << server_port <<"\n"
    //           << "wav_rspecifier : " << wav_rspecifier  << "\n"
    //           << "client_desc : " << client_desc <<"\n";

    struct hostent* hp;
    unsigned long addr;

    addr = inet_addr(server_addr_str.c_str());
    if (addr == INADDR_NONE) {
      hp = gethostbyname(server_addr_str.c_str());
      if (hp == NULL) {
        std::cerr << "ERROR: couldn't resolve host string: " << server_addr_str
                  << std::endl;
        close(client_desc);
        return -1;
      }

      addr = *((unsigned long*) hp->h_addr);
    }

    // sockaddr_in server;
    // server.sin_addr.s_addr = addr;
    // server.sin_family = AF_INET;
    // server.sin_port = htons(server_port);
    // if (::connect(client_desc, (struct sockaddr*) &server, sizeof(server))) {
    //   std::cerr << "ERROR: couldn't connect to server!" << std::endl;
    //   close(client_desc);
    //   return -1;
    // }

    // KALDI_VLOG(2) << "Connected to KALDI server at host " << server_addr_str
    //     << " port " << server_port << std::endl;

    // Creat a client file

    mkdir("client-log", 0700);
    std::string log_file = "client-log/client_log";
    std::ofstream client_file(log_file.c_str());
    client_file << "Client log file \n";
    client_file.close();


    std::string log_total = "client-log/total_file";
    std::ofstream total_file(log_total.c_str());
    total_file << "Total log file \n";
    total_file.close();

    std::string audio = "client-log/audio.raw";
    // std::ofstream audioFile(audio.c_str());
    // audioFile.close();


    OnlinePaSource au_src(kTimeout, kSampleFreq, kPaRingSize, kPaReportInt);
    char* pack_buffer = new char[packet_size];
    Vector<BaseFloat> data(packet_size/2);
    Vector<BaseFloat> raw_data;
    
    int32 count_times = 0;
    bool flag_end = false;

    while (true) {

      bool get_data = au_src.Read(&data);
      if(au_src.TimedOut()){
        std::cout << "Time out ! \n";
      }
      count_times++;
      // Fill data to raw_data
      raw_data.Resize(count_times*packet_size/2, kCopyData);
      std::cout<<"count_times["<<count_times<<"], size: "<<count_times*packet_size/2<<"\n";
      raw_data.Range((count_times-1)*packet_size/2, packet_size/2).CopyFromVec(data);
      if(count_times>30) break;
    }

    std::cout << "Finish !, size of raw_data : "<<raw_data.Dim()<<"\n";
    /// Print raw_file
    // client_file.open(log_file.c_str(), std::ios::app);
    // for (int i = 0; i < 21540; ++i)
    // {
    //   if(((i)%1024)==0) client_file << "\nPacket : "<<((i+1)/1024)<<"\n";
    //   client_file << raw_data(i) << " ";
    //   if(((i+1)%10)==0) client_file << "\n";
    // }
    // client_file.close();

    // Fill raw_data to total
    float total2[raw_data.Dim()];
    std::cout<<"total2: "<<(sizeof(total2)/sizeof(*total2))<<"\n";
    
    float *total = new float[raw_data.Dim()];
    for (int i = 0; i < raw_data.Dim(); ++i)
    {
      // float sample = (float) raw_data(i);
      *(total+i) = (float) raw_data(i);
    }

    
    // total_file.open(log_total.c_str(), std::ios::app);
    // for (int i = 0; i < 21540; ++i)
    // {
    //   if(((i)%1024)==0) total_file << "\nPacket : "<<((i+1)/1024)<<"\n";
    //   total_file << total[i] << " ";
    //   if(((i+1)%10)==0) total_file << "\n";
      
    // }
    // total_file.close();


    FILE  *fid;
    fid = fopen("client-log/recorded.raw", "wb");
    if( fid == NULL )
    {
        printf("Could not open file.");
    }
    else
    {
        fwrite( total, sizeof(float), 32768, fid );
        fclose( fid );
        printf("Wrote data to 'recorded.raw'\n");
    }
  }

  catch (const std::exception& e) {
    std::cerr << e.what();
    return -1;
  }

#endif
  return 0;
}


namespace kaldi {

bool WriteFull(int32 desc, char* data, int32 size) {
  int32 to_write = size;
  int32 wrote = 0;
  // int Dcount=0;
  while (to_write > 0) {
    int32 ret = write(desc, data + wrote, to_write);
    // std::cout << "Dcount : " <<Dcount++<< ", ret : " << ret << "\n";
    if (ret <= 0)
      return false;

    to_write -= ret;
    wrote += ret;
  }

  return true;
}

int32 buffer_offset = 0;
int32 buffer_fill = 0;
char read_buffer[1025];

bool ReadLine(int32 desc, std::string* str) {
  *str = "";

  while (true) {
    if (buffer_offset >= buffer_fill) {
      buffer_fill = read(desc, read_buffer, 1024);

      if (buffer_fill <= 0)
        break;

      buffer_offset = 0;
    }

    for (int32 i = buffer_offset; i < buffer_fill; i++) {
      if (read_buffer[i] == '\r' || read_buffer[i] == '\n') {
        read_buffer[i] = 0;
        *str += (read_buffer + buffer_offset);

        buffer_offset = i + 1;

        if (i < buffer_fill) {
          if (read_buffer[i] == '\n' && read_buffer[i + 1] == '\r') {
            read_buffer[i + 1] = 0;
            buffer_offset = i + 2;
          }
          if (read_buffer[i] == '\r' && read_buffer[i + 1] == '\n') {
            read_buffer[i + 1] = 0;
            buffer_offset = i + 2;
          }
        }

        return true;
      }
    }

    read_buffer[buffer_fill] = 0;
    *str += (read_buffer + buffer_offset);
    buffer_offset = buffer_fill;
  }

  return false;
}

std::string TimeToTimecode(float time) {

  char buf[64];

  int32 h, m, s, ms;
  s = (int32) time;
  ms = (int32)((time - (float) s) * 1000.0f);
  m = s / 60;
  s %= 60;
  h = m / 60;
  m %= 60;

#if !defined(_MSC_VER)
  snprintf(buf, 64, "%02d:%02d:%02d.%03d", h, m, s, ms);
#endif

  return buf;
}

}  //namespace kaldi
