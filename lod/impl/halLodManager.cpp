/*
 * Copyright (C) 2013 by Glenn Hickey (hickey@soe.ucsc.edu)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include <cassert>
#include <deque>
#include <sstream>
#include <fstream>
#include <algorithm>
#include "halLodManager.h"

#ifdef ENABLE_UDC
extern "C" {
#include "common.h"
#include "udc.h"
// Dont want this macro ruining std::max
#ifdef max
#undef max
#endif
}
#endif

using namespace std;
using namespace hal;

LodManager::LodManager()
{

}

LodManager::~LodManager()
{
  for (AlignmentMap::iterator mapIt = _map.begin(); mapIt != _map.end();
       ++mapIt)
  {
    if (mapIt->second.second.get() != NULL)
    {
      mapIt->second.second->close();
    }
  }
}

void LodManager::loadLODFile(const string& lodPath,
                             CLParserConstPtr options)
{
  _map.clear();
  _coarsestLevelWithSeq = 0;

#ifdef ENABLE_UDC
  char* cpath = const_cast<char*>(lodPath.c_str());
  size_t cbufSize = 0;
  char* cbuffer = udcFileReadAll(cpath, NULL, 100000, &cbufSize);
  if (cbuffer == NULL)
  {
    stringstream emes;
    emes << "Error udc-opening " << lodPath;
    throw hal_exception(emes.str());
  }
  string cbufCpy(cbuffer);
  freeMem(cbuffer);
  stringstream ifile(cbufCpy);
#else
  ifstream ifile(lodPath.c_str());
#endif

  if (!ifile.good())
  {
    stringstream emes;
    emes << "Error opening " << lodPath;
    throw hal_exception(emes.str());
  }
  
  string lineBuffer;
  hal_size_t minLen;
  string path;
  hal_size_t lineNum = 1;
  while (ifile.good())
  {
    getline(ifile, lineBuffer);
    stringstream ss(lineBuffer);
    ss >> minLen >> path;
    if (ifile.bad())
    {
      stringstream emes;
      emes << "Error parsing line " << lineNum << " of " << lodPath;
      throw hal_exception(emes.str());
    }
    string fullHalPath = resolvePath(lodPath, path);
    _map.insert(pair<hal_size_t, PathAlign>(
                  minLen, PathAlign(fullHalPath, AlignmentConstPtr())));
    ++lineNum;
  }

  checkMap(lodPath);
}

void LodManager::loadSingeHALFile(const string& halPath,
                                  CLParserConstPtr options)
{
  _map.clear();
  _coarsestLevelWithSeq = 0;
  _map.insert(pair<hal_size_t, PathAlign>(
                0, PathAlign(halPath, AlignmentConstPtr())));
  checkMap(halPath);
}

AlignmentConstPtr LodManager::getAlignment(hal_size_t queryLength,
                                           bool needDNA)
{
  assert(_map.size() > 0);
  AlignmentMap::iterator mapIt = _map.upper_bound(queryLength);
  --mapIt;
  assert(mapIt->first <= queryLength);
  AlignmentConstPtr& alignment = mapIt->second.second;
  if (alignment.get() == NULL)
  {
    alignment = hdf5AlignmentInstanceReadOnly();
    if (_options.get() != NULL)
    {
      alignment->setOptionsFromParser(_options);
    }
    alignment->open(mapIt->second.first);
    checkAlignment(mapIt->first, mapIt->second.first, alignment);
  }
  if (needDNA == true && _coarsestLevelWithSeq < mapIt->first)
  {
    assert(mapIt->first > 0);
    return getAlignment(0, true);
  }
  assert(mapIt->second.second.get() != NULL);
  return alignment;
}

string LodManager::resolvePath(const string& lodPath,
                               const string& halPath)
{
  assert(lodPath.empty() == false && halPath.empty() == false);
  if (halPath[0] == '/' || halPath.find(":/") != string::npos)
  {
    return halPath;
  }
  size_t sPos = lodPath.find_last_of('/');
  if (sPos == string::npos)
  {
    return halPath;
  }
  return lodPath.substr(0, sPos + 1) + halPath;
}

void LodManager::checkMap(const string& lodPath)
{
  if (_map.size() == 0)
  {
    stringstream ss;
    ss << "No entries were found in " << lodPath;
    throw hal_exception(ss.str());
  }
  AlignmentMap::const_iterator mapIt = _map.begin();
  if (mapIt->first != 0)
  {
    stringstream ss;
    ss << "No alignment with range value 0 found in " << lodPath << ". "
       << "A record of the form \"0 pathToOriginalHALFile\" must be present";
    throw hal_exception(ss.str());
  }
 
}

void LodManager::checkAlignment(hal_size_t minQuery,
                                const string& path,
                                AlignmentConstPtr alignment)
{
  if (alignment->getNumGenomes() == 0)
  {
    stringstream ss;
    ss << "No genomes found in base alignment specified in " << path;
    throw hal_exception(ss.str());
  }

  bool seqFound = false;
  deque<string> bfQueue;
  bfQueue.push_back(alignment->getRootName());
  while (bfQueue.size() > 0 && !seqFound)
  {
    string name = bfQueue.back();
    bfQueue.pop_back();
    const Genome* genome = alignment->openGenome(name);
    seqFound = genome->containsDNAArray();
    alignment->closeGenome(genome);
    vector<string> children = alignment->getChildNames(name);
    for (size_t i = 0; i < children.size(); ++i)
    {
      bfQueue.push_front(children[i]);
    }
  }
  if (seqFound == false && minQuery == 0)
  {
    stringstream ss;
    ss << "HAL file for highest level of detail (0) in " << path 
       << "must contain DNA sequence information.";
    throw hal_exception(ss.str());
  }
  else if (seqFound == true)
  {
    _coarsestLevelWithSeq = std::max(_coarsestLevelWithSeq, minQuery);
  }
}