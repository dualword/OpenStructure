#include <fstream>
#include <iostream>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <ost/string_ref.hh>
#include <stdexcept>
#include "hmm.hh"

namespace ost { namespace seq{ 

int HMMColumn::GetIndex(char olc) const {
  if (olc == 'A') return 0;
  if (olc >= 'C' && olc <= 'I') return (olc-'A') - 1;
  if (olc >= 'K' && olc <= 'N') return (olc-'A') - 2;
  if (olc >= 'P' && olc <= 'T') return (olc-'A') - 3;
  if (olc == 'V' ) return 17;
  if (olc == 'W' ) return 18;
  if (olc == 'Y' ) return 19;
  return -1;
}

HMMColumn HMMColumn::BLOSUMNullModel() {
  HMMColumn col;

  col.freq_[0] = 0.0732;   
  col.freq_[1] = 0.0250;   
  col.freq_[2] = 0.0542;   
  col.freq_[3] = 0.0547;   
  col.freq_[4] = 0.0447;   
  col.freq_[5] = 0.0751;   
  col.freq_[6] = 0.0261;   
  col.freq_[7] = 0.0700;   
  col.freq_[8] = 0.1011;   
  col.freq_[9] = 0.0584;   
  col.freq_[10] = 0.0246;   
  col.freq_[11] = 0.0463;   
  col.freq_[12] = 0.0351;   
  col.freq_[13] = 0.0326;   
  col.freq_[14] = 0.0484;   
  col.freq_[15] = 0.0573;   
  col.freq_[16] = 0.0505;   
  col.freq_[17] = 0.0758;   
  col.freq_[18] = 0.0124;   
  col.freq_[19] = 0.0345;

  //it's an allaline... I know, it doesn;t make much sense
  col.olc_ = 'A';

  return col;
}

Real HMMColumn::GetEntropy() const {
  Real entropy = 0.0;
  for (const Real* j = this->freqs_begin(), *e2 = this->freqs_end(); j != e2; ++j) {

    if (*j>0.0) {
      entropy -= log(*j)**j;
    }
  }
  return entropy;
}

HMMPtr HMM::Load(const std::string& filename) {
  HMMPtr hmm(new HMM);
  boost::iostreams::filtering_stream<boost::iostreams::input> in;
  std::ifstream stream(filename.c_str()); 
  if(!stream){
    throw std::runtime_error("Could not open file!");
  }
  if (filename.rfind(".gz") == filename.size()-3) {
    in.push(boost::iostreams::gzip_decompressor());
  }

  in.push(stream);
  std::string line;
  bool null_found = false;
  ost::StringRef sline;
  while (std::getline(in, line)) {
    sline=ost::StringRef(line.c_str(), line.length());
    if (sline.length()>4 && 
        sline.substr(0, 5)==ost::StringRef("NULL ", 5)) {
      null_found = true;
      break;
    }
  }
  const char* olcs = "ACDEFGHIKLMNPQRSTVWY";
  if (!null_found) {
    throw std::runtime_error("no NULL found in file");
  }
  std::vector<ost::StringRef> chunks = sline.split(); 
  for (int i=1; i<21; ++i) {
    if (chunks[i] != ost::StringRef("*", 1)) {
      Real freq = pow(2.0, -0.001*chunks[i].to_int().second);
      hmm->null_model_.SetFreq(olcs[i-1], freq);
    }
  }
  // read until we hit HMM, then skip 3 more lines
  bool hmm_found = false;
  while (std::getline(in, line)) {
    sline=ost::StringRef(line.c_str(), line.length());
    if (sline.length()>3 && 
        sline.substr(0, 4)==ost::StringRef("HMM ", 4)) {
      for (int i = 0; i<2; ++i ) {
        std::getline(in, line);
      }
      hmm_found = true;
      break;
    }
  }
  if (!hmm_found) {
    throw std::runtime_error("no HMM found in file");
  }
  while (std::getline(in, line)) {
    sline = ost::StringRef(line.c_str(), line.length());
    if (sline.trim().empty()) continue;
    if (sline.trim() == ost::StringRef("//", 2)) break;
    std::vector<ost::StringRef> freqs = sline.split();
    std::string line2;
    std::getline(in, line2);
    ost::StringRef sline2(line2.c_str(), line2.length());
    std::vector<ost::StringRef> trans = sline2.trim().split(); 
    hmm->columns_.push_back(HMMColumn());
    hmm->columns_.back().SetOneLetterCode(freqs[0][0]);
    for (int i = 2; i < 22; ++i) {
      if (freqs[i] != ost::StringRef("*", 1)) {
        hmm->columns_.back().SetFreq(olcs[i-2],
                                     pow(2.0, -0.001*freqs[i].to_int().second));
      }
    }
    HMMState from,to;
    int trans_freq_pos = 0;
    for(uint i = 0; i < 3; ++i){
      from = HMMState(i);
      for(uint j = 0; j < 3; ++j){
        if((i == 1 && j == 2) || (i == 2 && j == 1)) continue; //transition not possible
        if (trans[trans_freq_pos] != ost::StringRef("*", 1)){
          Real t_freq = pow(2.0, -0.001*trans[trans_freq_pos].to_int().second);
          to = HMMState(j);
          hmm->columns_.back().SetTransitionFreq(from, to, t_freq);
        }        
        ++trans_freq_pos;
      }
    }

    hmm->columns_.back().SetNEff(0.001 * trans[trans_freq_pos].to_int().second);
    hmm->columns_.back().SetNEffIns(0.001 * trans[trans_freq_pos+1].to_int().second);
    hmm->columns_.back().SetNEffDel(0.001 * trans[trans_freq_pos+2].to_int().second);
  }
  return hmm;
}

String HMM::GetSequence() const{

  std::stringstream ss;
  for(HMMColumnList::const_iterator i = this->columns_begin(); 
      i != this->columns_end(); ++i){
    ss << i->GetOneLetterCode();
  }
  return ss.str();
}


Real HMM::GetAverageEntropy() const {
  Real n_eff=0.0;
  int n = 0;
  for (std::vector<HMMColumn>::const_iterator
       i = this->columns_begin(), e = this->columns_end(); i != e; ++i) {
    n += 1;
    n_eff += i->GetEntropy();
  }
  return (n > 0) ? n_eff/n : 0.0;
}

}} //ns
