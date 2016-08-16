// onlinebin/ client-online.cc

// Copyright 2012 Cisco Systems (author: Matthias Paulik)
// Copyright 2013 Polish-Japanese Institute of Information Technology (author: Danijel Korzinek)

//   Modifications to the original contribution by Cisco Systems made by:
//   Nguyen Hai Duong
//   Mar 2016
//   Recognize through wav file
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

namespace kaldi {

bool WriteFull(int32 desc, char* data, int32 size);
bool ReadLine(int32 desc, std::string* str);
std::string TimeToTimecode(float time);

}  //namespace kaldi

int main(int argc, char** argv) {
  using namespace kaldi;
  typedef kaldi::int32 int32;
  #if !defined(_MSC_VER)
  try {

    const char *usage =
        "Sends an audio file to the KALDI audio server (onlinebin/online-audio-server-decode-faster)\n"
            "and prints the result on terminal \n\n"
            "e.g.: ./online-audio-client 192.168.50.12 9012 'scp:wav_files.scp'\n\n";
    ParseOptions po(usage);

    int32 channel = -1;
    int32 packet_size = 12288;

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

    sockaddr_in server;
    server.sin_addr.s_addr = addr;
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    if (::connect(client_desc, (struct sockaddr*) &server, sizeof(server))) {
      std::cerr << "ERROR: couldn't connect to server!" << std::endl;
      close(client_desc);
      return -1;
    }

    // KALDI_VLOG(2) << "Connected to KALDI server at host " << server_addr_str
    //     << " port " << server_port << std::endl;

    // Creat a client file

    // mkdir("client-log", 0700);
    // std::string log_file = "client-log/client_log";
    // std::ofstream client_file(log_file.c_str());
    // client_file << "Client log file \n";
    // client_file.close();

    SequentialTableReader < WaveHolder > reader(wav_rspecifier);
    for (; !reader.Done(); reader.Next()) {
      std::string wav_key = reader.Key();
      // std::cout<< "wav_key : " << wav_key << "\n";
      KALDI_VLOG(2) << "File: " << wav_key << std::endl;

      const WaveData &wave_data = reader.Value();

      if (wave_data.SampFreq() != 16000)
        KALDI_ERR << "Sampling rates other than 16kHz are not supported!";

      int32 num_chan = wave_data.Data().NumRows(), this_chan = channel;

      std::cout <<std::endl<< wav_key << " : " << std::endl;
      // std::cout << wave_data.Data().NumCols() << " (dim)"  <<"\n";

      {   // This block works out the channel (0=left, 1=right...)
        KALDI_ASSERT(num_chan > 0);  // should have been caught in
        // reading code if no channels.
        if (channel == -1) {
          this_chan = 0;
          if (num_chan != 1)
            KALDI_WARN << "Channel not specified but you have data with "
                << num_chan << " channels; defaulting to zero";
        } else {
          if (this_chan >= num_chan) {
            KALDI_WARN << "File with id " << wav_key << " has " << num_chan
                << " channels but you specified channel " << channel
                << ", producing no output.";
            continue;
          }
        }
      }
      
      OnlineVectorSource au_src(wave_data.Data().Row(this_chan));
      char* pack_buffer = new char[packet_size];
      Vector < BaseFloat > data(packet_size / 2);

      // clock_t start = clock();
      // int32 sent_times = 0;
      bool flag_end = false;
      while (true) {

        bool get_data = au_src.Read(&data);
        // Send data to server
        if(get_data){
          // std::cout<<"Sending [" << sent_times <<"]\n";
          for (int32 i = 0; i < packet_size / 2; i++) {
            short sample = (short) data(i);
            memcpy(&pack_buffer[i * 2], (char*) &sample, 2);
          }
          WriteFull(client_desc, (char*) &packet_size, 4);
          WriteFull(client_desc, pack_buffer, packet_size);
          // // Save data of wave in file : 
          // {
          //   client_file.open(log_file.c_str(), std::ios::app);
          //   client_file << "\nPacket [" << sent_times <<"] :\n" ;
          //   for (int i = 0; i < packet_size/2; ++i)
          //   {
          //     client_file << data(i) << " ";
          //     if(((i+1)%10)==0) client_file << "\n";
          //   }
          //   client_file.close();
          // }
          // sent_times++;
          usleep(1000000);
        }else{
          // send last packet
          // std::cout << "Send last packet !\n";
          int32 size = 0;
          WriteFull(client_desc, (char*) &size, 4);
        }
        // Get word form server
        while (true) {
          std::string line;
          if (!ReadLine(client_desc, &line))
            KALDI_ERR << "Server disconnected!";

          if(line=="RESULT:DONE"){
            flag_end=true; break;
          }else if(line=="..."){
            std::cout << "\n"; break;
          }else if(line=="NONE"){
            break;
          }else{
            std::cout << line << " ";
          }
        }
        if((!get_data)||flag_end) break;
      }
    delete[] pack_buffer;
    }
    
    close(client_desc);
    
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
