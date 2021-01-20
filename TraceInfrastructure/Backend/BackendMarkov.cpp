#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

using namespace std;
using json = nlohmann::json;

class dict
{
public:
    unordered_map<uint64_t, unordered_map<uint64_t, uint64_t>> base;
    ~dict()
    {
        char *tfn = getenv("MARKOV_FILE");
        string fileName;
        if (tfn != nullptr)
        {
            fileName = tfn;
        }
        else
        {
            fileName = "markov.bin";
        }
        ofstream fp(fileName, std::ios::out | std::ios::binary);
        for (auto addr1 : base)
        {
            fp.write((const char *)&addr1.first, sizeof(uint64_t));
            uint64_t length = addr1.second.size();
            fp.write((const char *)&length, sizeof(std::size_t));
            for (auto addr2 : addr1.second)
            {
                fp.write((const char *)&addr2.first, sizeof(uint64_t));
                fp.write((const char *)&addr2.second, sizeof(std::size_t));
            }
        }
        fp.close();
    }
};

struct labelMap
{
    map<string, map<string, uint64_t>> blockLabels;
    ~labelMap()
    {
        json labelMap;
        for (const auto &bbid : blockLabels)
        {
            for (const auto &label : bbid.second)
            {
                labelMap[bbid.first][label.first] = label.second;
            }
        }
        ofstream file;
        char *labelFileName = getenv("LABEL_FILE");
        if (labelFileName == nullptr)
        {
            file.open("Labels.json");
        }
        else
        {
            file.open(labelFileName);
        }
        file << setw(4) << labelMap;
        file.close();
    }
};

uint64_t b;
uint64_t *markovResult;
bool markovInit = false;
dict TraceAtlasMarkovMap;
labelMap TraceAtlasLabelMap;
vector<char *> labelList;

extern "C"
{
    extern uint64_t MarkovBlockCount;
    void MarkovIncrement(uint64_t a)
    {
        if (markovInit)
        {
            TraceAtlasMarkovMap.base[b][a]++;
        }
        else
        {
            markovInit = true;
        }
        b = a;
        if (!labelList.empty())
        {
            string labelName(labelList.back());
            if (TraceAtlasLabelMap.blockLabels.find(to_string(a)) == TraceAtlasLabelMap.blockLabels.end())
            {
                TraceAtlasLabelMap.blockLabels[to_string(a)] = map<string, uint64_t>();
            }
            if (TraceAtlasLabelMap.blockLabels[to_string(a)].find(labelName) == TraceAtlasLabelMap.blockLabels[to_string(a)].end())
            {
                TraceAtlasLabelMap.blockLabels[to_string(a)][labelName] = 0;
            }
            TraceAtlasLabelMap.blockLabels[to_string(a)][labelName]++;
        }
    }
    void TraceAtlasMarkovKernelEnter(char *label)
    {
        labelList.push_back(label);
    }
    void TraceAtlasMarkovKernelExit()
    {
        labelList.pop_back();
    }
}