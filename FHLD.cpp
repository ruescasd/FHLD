/*
Fully Homomorphic Liquid Democracy demo using Shoup's HELib/NTL (https://github.com/shaih/HElib). This is experimental and can easily break.

source set_env
g++ FHLD.cpp $HELIB/src/fhe.a -I$HELIB/src -o FHLD -L/usr/local/lib -lntl

 */
#include "FHE.h"
#include "timing.h"
#include "EncryptedArray.h"
#include <NTL/lzz_pXFactoring.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <iterator>

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

vector<long> getVote(int vote, int slots) {
  vector<long> ret(slots);
  if(vote > 0 && vote < slots + 1) {
    ret[vote - 1] = 1;
  }

  return ret;
}

vector<vector <long> > readVotes(string fileName, int slots) {
  std::ifstream file(fileName.c_str());
  std::string str;

  vector<vector <long> > votes;

  while (std::getline(file, str)) {
      votes.push_back(getVote(atoi(str.c_str()), slots));
      cerr << ".";
  }

  return votes;
}

vector<vector <long> > readDelegateVotes(string fileName, int slots, int delegateLimit) {
  std::ifstream file(fileName.c_str());
  std::string str;

  vector<vector <long> > votes;
  votes.assign(delegateLimit, getVote(-1, slots));

  while (std::getline(file, str)) {
    vector<string> tokens = split(str, ' ');
    int delegate = atoi(tokens[0].c_str());
    int vote = atoi(tokens[1].c_str());

    // cerr << delegate << " " << vote << "\n";
    cerr << ".";

    votes[delegate - 1] = getVote(vote, slots);
  }

  return votes;
}

void printVote(PlaintextArray vote) {
  vector<long> voteLong;
  vote.decode(voteLong);

  int pos = find (voteLong.begin(), voteLong.end(), 1) - voteLong.begin();
  if(pos == voteLong.size()) {
    cerr << "<blank vote>\n";
  } else {
    cerr << pos + 1 << '\n';
  }
}

void printVoteFull(PlaintextArray vote) {
  vector<long> _vote;
  vote.decode(_vote);

  for(int i = 0; i < _vote.size(); i++) {
    cerr << _vote[i] << " ";
  }
  cerr << "\n";

}

void printVotes(vector<PlaintextArray> votes) {
  for(int i = 0; i < votes.size(); i++) {
    printVote(votes[i]);
  }
}

void printTally(PlaintextArray tally) {
  vector<long> totals;
  tally.decode(totals);
  for(int i = 0; i < totals.size(); i++) {
    if(totals[i] != 0) {
      cerr << i + 1 << ": " << totals[i] << "\n";
    }
  }
}

void show(Ctxt ctxt, EncryptedArray ea, const FHESecKey secretKey) {
  PlaintextArray p(ea);
  ea.decrypt(ctxt, secretKey, p);

  printVoteFull(p);
}

vector<PlaintextArray> encodeVotes(vector<vector <long> > votes, EncryptedArray ea) {
  vector<PlaintextArray> ret;
  for(int i = 0; i < votes.size(); i++) {
    PlaintextArray vote(ea);
    vote.encode(votes[i]);
    ret.push_back(vote);
  }

  return ret;
}

vector<Ctxt> encryptVotes(vector<PlaintextArray> votes, EncryptedArray ea, const FHEPubKey& publicKey) {
  vector<Ctxt> ret;
  for(int i = 0; i < votes.size(); i++) {
    Ctxt c(publicKey);
    ea.encrypt(c, publicKey, votes[i]);
    ret.push_back(c);
  }

  return ret;
}

vector<PlaintextArray> decryptVotes(vector<Ctxt> votes, EncryptedArray ea, const FHESecKey secretKey) {
  vector<PlaintextArray> ret;
  for(int i = 0; i < votes.size(); i++) {
    PlaintextArray p(ea);
    ea.decrypt(votes[i], secretKey, p);
    ret.push_back(p);
  }

  return ret;
}

Ctxt tallyVotes(vector<Ctxt> votes) {
  Ctxt zero = votes[0];
  for(int i = 1; i < votes.size(); i++) {
    zero += votes[i];
  }

  return zero;
}

PlaintextArray decrypt(Ctxt ctxt, EncryptedArray ea, const FHESecKey secretKey) {
  PlaintextArray p(ea);
  ea.decrypt(ctxt, secretKey, p);

  return p;
}



Ctxt select(Ctxt ctxt, int value, EncryptedArray ea, const FHEPubKey& publicKey) {
  PlaintextArray mask(ea);
  mask.encode(getVote(value, ea.size()));

  Ctxt maskCtxt(publicKey);
  ea.encrypt(maskCtxt, publicKey, mask);

  Ctxt ret(ctxt);
  ret.multiplyBy(maskCtxt);

  return ret;
}

vector<Ctxt> getWeightFactors(Ctxt total, EncryptedArray ea, const FHEPubKey& publicKey, int delegateLimit) {
  cerr << "Obtaining weight factors";
  vector<Ctxt> ret;

  for(int i = 0; i < delegateLimit; i++) {
    Ctxt selected = select(total, i + 1, ea, publicKey);
    totalSums(ea, selected);
    ret.push_back(selected);
    cerr << ".";
  }
  cerr << "done\n";

  return ret;
}

vector<Ctxt> applyWeights(vector<Ctxt> weightFactors, vector<Ctxt> delegateVotes, EncryptedArray ea, const FHESecKey secretKey) {
  vector<Ctxt> ret;

  for(int i = 0; i < weightFactors.size(); i++) {
    delegateVotes[i] *= weightFactors[i];
    // cerr << "weighted delegate " << i + 1<< "\n";
    // show(delegateVotes[i], ea, secretKey);
    // cerr << "======================\n";
    ret.push_back(delegateVotes[i]);
  }

  return ret;
}

Ctxt liquidTally(Ctxt directTally, vector<Ctxt> weights) {
  Ctxt ret(directTally);

  for(int i = 0; i < weights.size(); i++) {
    ret.addCtxt(weights[i]);
  }

  return ret;
}


int main(int argc, char *argv[]) {

  /*************************** INIT ***************************/
  /* most of the init code is copied directly from HElibs general test (https://github.com/shaih/HElib/blob/master/src%2FTest_General.cpp) */

  cerr << "*************************** INIT ***************************" << "\n";

  argmap_t argmap;
  argmap["R"] = "1";
  argmap["p"] = "113";
  argmap["r"] = "1";
  argmap["d"] = "1";
  argmap["c"] = "2";
  argmap["k"] = "80";
  argmap["L"] = "0";
  argmap["s"] = "0";
  argmap["m"] = "0";

  long R = atoi(argmap["R"]);
  long p = atoi(argmap["p"]);
  long r = atoi(argmap["r"]);
  long d = atoi(argmap["d"]);
  long c = atoi(argmap["c"]);
  long k = atoi(argmap["k"]);
  //  long z = atoi(argmap["z"]);
  long L = atoi(argmap["L"]);
  if (L==0) { // determine L based on R,r
    L = 3*R+3;
    if (p>2 || r>1) { // add some more primes for each round
      long addPerRound = 2*ceil(log((double)p)*r*3)/(log(2.0)*NTL_SP_NBITS) +1;
      L += R * addPerRound;
    }
  }
  long s = atoi(argmap["s"]);
  long chosen_m = atoi(argmap["m"]);

  long w = 64; // Hamming weight of secret key
  //  long L = z*R; // number of levels

  long m = FindM(k, L, c, p, d, s, chosen_m, true);

  cerr << "\n\nR=" << R
       << ", p=" << p
       << ", r=" << r
       << ", d=" << d
       << ", c=" << c
       << ", k=" << k
       << ", w=" << w
       << ", L=" << L
       << ", m=" << m
       << endl;

  FHEcontext context(m, p, r);
  buildModChain(context, L, c);

  context.zMStar.printout();
  cerr << endl;

  FHESecKey secretKey(context);
  const FHEPubKey& publicKey = secretKey;
  secretKey.GenSecKey(w); // A Hamming-weight-w secret key

  ZZX G;

  if (d == 0)
    G = context.alMod.getFactorsOverZZ()[0];
  else
    G = makeIrredPoly(p, d);

  cerr << "G = " << G << "\n";
  cerr << "generating key-switching matrices... ";
  addSome1DMatrices(secretKey); // compute key-switching matrices that we need
  cerr << "done\n";

  cerr << "computing masks and tables for rotation...";
  EncryptedArray ea(context, G);
  cerr << "done\n";

  long nslots = ea.size();
  cerr << "slots = " << nslots << "\n";

  // set this to the maximum amount of delegates you want to use
  int delegateLimit = 10;
  if(delegateLimit > nslots) {
    cerr << "delegateLimit must be <= nslots\n";
    exit(1);
  }

  cerr << "delegateLimit = " << delegateLimit << "\n";
  /*************************** INIT ***************************/

  cerr << "*************************** INIT ***************************" << "\n\n";

  cerr << "Reading delegate votes";
  vector<vector <long> > delegateVotes = readDelegateVotes("delegate_votes.txt", nslots, delegateLimit);
  cerr << "done\n";
  cerr << "Encoding delegate votes...\n";
  vector<PlaintextArray> delegateVotesEncoded = encodeVotes(delegateVotes, ea);
  cerr << "Encrypting delegate votes...\n";
  vector<Ctxt> delegateVotesEncrypted = encryptVotes(delegateVotesEncoded, ea, publicKey);

  cerr << "Reading direct votes";
  vector<vector <long> > votes = readVotes("direct_votes.txt", nslots);
  cerr << "done\n";
  cerr << "Encoding direct votes...\n";
  vector<PlaintextArray> votesEncoded = encodeVotes(votes, ea);
  cerr << "Encrypting direct votes...\n";
  vector<Ctxt> votesEncrypted = encryptVotes(votesEncoded, ea, publicKey);

  cerr << "Reading delegations";
  vector<vector <long> > delegations = readVotes("delegations.txt", nslots);
  cerr << "done\n";
  cerr << "Encoding delegations...\n";
  vector<PlaintextArray> delegationsEncoded = encodeVotes(delegations, ea);
  cerr << "Encrypting delegations...\n";
  vector<Ctxt> delegationsEncrypted = encryptVotes(delegationsEncoded, ea, publicKey);

  cerr << "Tallying delegations (+)...\n";
  Ctxt delegateWeights = tallyVotes(delegationsEncrypted);
  vector<Ctxt> weightFactors = getWeightFactors(delegateWeights, ea, publicKey, delegateLimit);
  cerr << "Applying weights (x)...\n";
  vector<Ctxt> weightedDelegateVotes = applyWeights(weightFactors, delegateVotesEncrypted, ea, secretKey);

  cerr << "Tallying direct votes (+)...\n";
  Ctxt directTally = tallyVotes(votesEncrypted);
  cerr << "Final tally (+)...\n";
  Ctxt tally = liquidTally(directTally, weightedDelegateVotes);

  cerr << "Decrypting final tally...\n";
  PlaintextArray totals = decrypt(tally, ea, secretKey);
  printTally(totals);
}