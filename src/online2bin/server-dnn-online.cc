// online2bin/server-dnn-online.cc

// Copyright 2014  Johns Hopkins University (author: Daniel Povey)

//   Modifications to the original contribution by Johns Hopkins made by:
//   Nguyen Hai Duong
//   Mar 2016
//   Decode speech, using feature batches received over a network connection. Use dnn technology
//   This version decode online data from client (user can directly speak through microphone ro recognize)
//   Client : onlinebin/client-online.cc

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

#include "feat/wave-reader.h"
#include "online2/online-nnet2-decoding.h"
#include "online2/online-nnet2-feature-pipeline.h"
#include "online2/onlinebin-util.h"
#include "online2/online-timing.h"
#include "online2/online-endpoint.h"
#include "fstext/fstext-lib.h"
#include "lat/lattice-functions.h"
#include "thread/kaldi-thread.h"

// Duong
#include "online/online-tcp-source.h"
#include "online/online-feat-input.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include <signal.h>
#include <sys/stat.h>
#include <pthread.h> //for threading , link with lpthread

namespace kaldi {

// This class is for a very simple TCP server implementation in UNIX sockets.

class TcpServer {
 public:
  TcpServer();
  ~TcpServer();

  bool Listen(int32 port);  //start listening on a given port
  int32 Accept();  //accept a client and return its descriptor

 private:
  struct sockaddr_in h_addr_, client;
  int32 server_desc_, clntSock, *new_sock;
};

//write a line of text to socket
bool WriteLine(int32 socket, std::string line);

//constant allowing to convert frame count to time
const float kFramesPerSecond = 100.0f;

// Global variables
typedef kaldi::int32 int32;
typedef kaldi::int64 int64;
TcpServer tcp_server;
std::string word_syms_rxfilename;
    
OnlineEndpointConfig endpoint_config;

// feature_config includes configuration for the iVector adaptation,
// as well as the basic features.
OnlineNnet2FeaturePipelineConfig feature_config;  
OnlineNnet2DecodingConfig nnet2_decoding_config;

BaseFloat chunk_length_secs = 0.05;
BaseFloat samp_freq = 16000;
int32 packet_size = 12288/2;
bool do_endpointing = false;
bool online = true;

std::string nnet2_rxfilename, fst_rxfilename;
int32 port;

TransitionModel trans_model;
nnet2::AmNnet nnet;
fst::Fst<fst::StdArc> *decode_fst;
fst::SymbolTable *word_syms = NULL;

OnlineNnet2FeaturePipelineInfo *feature_info;
OnlineIvectorExtractorAdaptationState *adaptation_state;

pthread_t threadID;              /* Thread ID from pthread_create() */
struct ThreadArgs *threadArgs;   /* Pointer to argument structure for thread */

// get word from lattice and sent word to client
inline void GetDiagnosticsAndSendClient(const fst::SymbolTable *word_syms,
                                  const CompactLattice &clat,
                                  std::vector<int32> &words,
                                  int32& clntSock);
// sent word to client
inline void SentWord(const fst::SymbolTable *word_syms,
              std::vector<int32> &words,
              int32 &clntSock);
// Socket descriptor for client
struct ThreadArgs{
    int clntSock;
};
// Decoding a client
void *DecodeClient(void *);

}  // namespace kaldi

int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    using namespace fst;
    
    signal(SIGPIPE, SIG_IGN);

    const char *usage =
        "Starts a TCP server that receives RAW audio and outputs aligned words.\n"
          "A sample client can be found in: onlinebin/online-audio-client\n\n"
          "Usage: online-audio-server-decode-faster [options] <nnet2-in> "
          "<word-symbol-table> [lda-matrix-in] <fst-in> <tcp-port> \n\n"
          "example: online-audio-server-decode-faster-dnn --online=true --do-endpointing=false"
          "--config=exp/nnet2_online/nnet_ms_a_online/conf/online_nnet2_decoding.conf"
          "--max-active=7000 --beam=20.0 --lattice-beam=10.0 --acoustic-scale=0.1"
          "--word-symbol-table=exp/tri3b-dnn/graph/words.txt exp/nnet2_online/nnet_ms_a_online/final.mdl"
          "exp/tri3b-dnn/graph/HCLG.fst 5010\n\n";
    
    ParseOptions po(usage);
    
    po.Register("chunk-length", &chunk_length_secs,
                "Length of chunk size in seconds, that we process.  Set to <= 0 "
                "to use all input in one chunk.");
    po.Register("word-symbol-table", &word_syms_rxfilename,
                "Symbol table for words [for debug output]");
    po.Register("do-endpointing", &do_endpointing,
                "If true, apply endpoint detection");
    po.Register("online", &online,
                "You can set this to false to disable online iVector estimation "
                "and have all the data for each utterance used, even at "
                "utterance start.  This is useful where you just want the best "
                "results and don't care about online operation.  Setting this to "
                "false has the same effect as setting "
                "--use-most-recent-ivector=true and --greedy-ivector-extractor=true "
                "in the file given to --ivector-extraction-config, and "
                "--chunk-length=-1.");
    po.Register("num-threads-startup", &g_num_threads,
                "Number of threads used when initializing iVector extractor.");
    
    feature_config.Register(&po);
    nnet2_decoding_config.Register(&po);
    endpoint_config.Register(&po);
    
    po.Read(argc, argv);

    if (po.NumArgs() != 3) {
      po.PrintUsage();
      return 1;
    }

    nnet2_rxfilename = po.GetArg(1);
    fst_rxfilename = po.GetArg(2);
    port = strtol(po.GetArg(3).c_str(), 0, 10);
    
    std::cout << "Reading acoustic model: " << nnet2_rxfilename << "...\n";   
    {
      bool binary;
      Input ki(nnet2_rxfilename, &binary);
      trans_model.Read(ki.Stream(), binary);
      nnet.Read(ki.Stream(), binary);
    }

    std::cout << "Reading FST: " << fst_rxfilename << "..." << std::endl;
    decode_fst = ReadFstKaldi(fst_rxfilename);

    std::cout << "Reading words symbol: " << word_syms_rxfilename << "..." << std::endl;
    if (word_syms_rxfilename != "")
      if (!(word_syms = fst::SymbolTable::ReadText(word_syms_rxfilename)))
        KALDI_ERR << "Could not read symbol table from file "
                  << word_syms_rxfilename;
    
    // Listen port
    if (!tcp_server.Listen(port))
      return 0;

    // OnlineTimingStats timing_stats;
    
    // Creat a server log

    // mkdir("sever-log", 0700);
    // std::string log_file = "/home/season/kaldi-trunk/egs/client-server/s5/sever-log/server_log";
    // std::ofstream server_file(log_file.c_str());
    // // server_file << "Server log file \n";
    // server_file.close();

    // Global config
    OnlineNnet2FeaturePipelineInfo feature_info2(feature_config);

    if (!online) {
      feature_info2.ivector_extractor_info.use_most_recent_ivector = true;
      feature_info2.ivector_extractor_info.greedy_ivector_extractor = true;
      chunk_length_secs = -1.0;
    }
    feature_info = &feature_info2;

    OnlineIvectorExtractorAdaptationState adaptation_state2(
      feature_info2.ivector_extractor_info);
    adaptation_state = &adaptation_state2;

    OnlineSilenceWeighting silence_weighting(
      trans_model,
      feature_info2.silence_weighting_config);

    // Waiting until connect to client
    tcp_server.Accept();

    
  } catch(const std::exception& e) {
    std::cerr << e.what();
    return -1;
  }
} // main()

namespace kaldi {
// IMPLEMENTATION OF THE CLASSES/METHODS ABOVE MAIN
TcpServer::TcpServer() {
  server_desc_ = -1;
}

bool TcpServer::Listen(int32 port) {
  h_addr_.sin_addr.s_addr = INADDR_ANY;
  // h_addr_.sin_addr.s_addr = inet_addr("192.168.1.97");
  h_addr_.sin_port = htons(port);
  h_addr_.sin_family = AF_INET;

  server_desc_ = socket(AF_INET, SOCK_STREAM, 0);

  if (server_desc_ == -1) {
    KALDI_ERR << "Cannot create TCP socket!";
    return false;
  }

  int32 flag = 1;
  int32 len = sizeof(int32);
  if( setsockopt(server_desc_, SOL_SOCKET, SO_REUSEADDR, &flag, len) == -1){
    KALDI_ERR << "Cannot set socket options!\n";
    return false;
  }

  if (bind(server_desc_, (struct sockaddr*) &h_addr_, sizeof(h_addr_)) == -1) {
    KALDI_ERR << "Cannot bind to port: " << port << " (is it taken?)";
    return false;
  }

  if (listen(server_desc_, 5) == -1) {
    KALDI_ERR << "Cannot listen on port!";
    return false;
  }

  std::cout << "TcpServer: Listening on port: " << port << std::endl;

  return true;

}

TcpServer::~TcpServer() {
  if (server_desc_ != -1)
    close(server_desc_);
}

int32 TcpServer::Accept() {
  std::cout << "\n\nWaiting for client..." << std::endl;

  socklen_t len;

  len = sizeof(struct sockaddr);

  while( (clntSock = accept(server_desc_, (struct sockaddr*) &client, &len) )) {
    std::cout<<"+ New client["<<clntSock<<"]["<<inet_ntoa(client.sin_addr)<<"]["<<ntohs(client.sin_port)<<"]\n";
    
    // Create separate memory for client argument
    if ((threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL)
        perror("malloc() failed");

    threadArgs -> clntSock = clntSock;
  
    if( pthread_create( &threadID, NULL ,  DecodeClient , (void *) threadArgs) < 0)
    {
        perror("could not create thread");
        return 1;
    }
  }
  return 1;
}

bool WriteLine(int32 socket, std::string line) {
  line = line + "\n";

  const char* p = line.c_str();
  int32 to_write = line.size();
  int32 wrote = 0;
  while (to_write > 0) {
    int32 ret = write(socket, p + wrote, to_write);
    if (ret <= 0)
      return false;

    to_write -= ret;
    wrote += ret;
  }

  return true;
}

inline void GetDiagnosticsAndSendClient(const fst::SymbolTable *word_syms,
                                  const CompactLattice &clat,
                                  std::vector<int32> &words,
                                  int32& clntSock) {
  if (clat.NumStates() == 0) {
    // std::cout << "Empty lattice.\n";
    return;
  }
  CompactLattice best_path_clat;
  CompactLatticeShortestPath(clat, &best_path_clat);
  
  Lattice best_path_lat;
  ConvertLattice(best_path_clat, &best_path_lat);
  
  LatticeWeight weight;
  std::vector<int32> alignment;

  GetLinearSymbolSequence(best_path_lat, &alignment, &words, &weight);
  // std::cout << "words.size() : " << words.size() <<"\n";
  SentWord(word_syms,words,clntSock);
  
}

inline void SentWord(const fst::SymbolTable *word_syms,
              std::vector<int32> &words,
              int32& clntSock){
  if (words.size() > 0) {
    std::cout << ". client[" <<clntSock<< "] ";
    for (size_t i = 0; i < words.size(); i++) {
      if (words[i] == 0)
        continue;  //skip silences...

      std::string word = word_syms->Find(words[i]);
      if (word.empty())
        word = "???";

      std::cout<<word<<' ';
      WriteLine(clntSock, word.c_str()); // command sent string to client
    }
    std::cout<<std::endl;
    WriteLine(clntSock, "...");
  }else{
    WriteLine(clntSock, "NONE");
  }
}

void *DecodeClient(void *threadArgs){

  OnlineTcpVectorSource* au_src = NULL;
  int clntSock = ((struct ThreadArgs *) threadArgs) -> clntSock;

  au_src = new OnlineTcpVectorSource(clntSock);

  OnlineNnet2FeaturePipeline feature_pipeline(*feature_info);
    feature_pipeline.SetAdaptationState(*adaptation_state);
  
  SingleUtteranceNnet2Decoder decoder(nnet2_decoding_config,
                                      trans_model,
                                      nnet,
                                      *decode_fst,
                                      &feature_pipeline);

  Vector<BaseFloat> data(packet_size);
  // std::vector<std::pair<int32, BaseFloat> > delta_weights;
  std::vector<int32> words;
  bool end_of_utterance = true;

  while(au_src->IsConnected()){
      // if (!au_src->IsConnected()) break;
      bool data_comming = au_src->Read(&data);

      if(!data_comming) {
        // std::cout << "RESULT:DONE !!! \n";
        WriteLine(clntSock, "RESULT:DONE");
        continue;
      }

      if (!au_src->IsConnected()) break;

      if(data_comming){  
        feature_pipeline.AcceptWaveform(samp_freq, data);
        decoder.AdvanceDecoding();
      }else{
        decoder.FinalizeDecoding();
      }

      CompactLattice clat;
      decoder.GetLattice(end_of_utterance, &clat);
      GetDiagnosticsAndSendClient(word_syms, clat, words,clntSock);
    
  }

  std::cout << "- Client[" <<clntSock<< "] Decoding complete !" << std::endl;
  close(clntSock);
  return 0;
}

}  // namespace kaldi