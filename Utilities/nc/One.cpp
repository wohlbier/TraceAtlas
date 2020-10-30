#include "One.h"
#include <algorithm>
#include <tuple>

using namespace std;

const uint64_t minIteration = 512;

bool isStronglyConnected(const std::vector<std::vector<uint64_t>> &matrix, const set<uint64_t> nodes)
{
    for (auto n : nodes)
    {
        for (auto m : nodes)
        {
            //check if n and m are connected at all
            set<uint64_t> visited;
            set<uint64_t> toVisit;

            toVisit.insert(m);
            toVisit.insert(n);

            while (true)
            {
                set<uint64_t> nextVisit;
                for (auto tv : toVisit)
                {
                    visited.insert(tv);
                    for (int i = 0; i < matrix[tv].size(); i++)
                    {
                        if (matrix[tv][i] != 0 && nodes.find(i) != nodes.end())
                        {
                            if (visited.find(i) == visited.end())
                            {
                                nextVisit.insert(i);
                            }
                        }
                    }
                }
                if (nextVisit.empty())
                {
                    break;
                }
                else
                {
                    toVisit = nextVisit;
                }
            }

            if (nodes != visited)
            {
                return false;
            }
        }
    }
    return true;
}

std::set<std::set<uint64_t>> StepOne(const std::vector<std::vector<uint64_t>> &matrix)
{
    set<set<uint64_t>> result;

    //first get all edges over some arbitrary minimum (for now)
    vector<tuple<uint64_t, uint64_t, uint64_t>> minCounts;
    for (int i = 0; i < matrix.size(); i++)
    {
        auto row = matrix[i];
        for (int j = 0; j < row.size(); j++)
        {
            auto entry = row[j];
            if (entry >= minIteration)
            {
                minCounts.push_back({i, j, entry});
            }
        }
    }

    sort(minCounts.begin(), minCounts.end(),
         [](const tuple<uint64_t, uint64_t, uint64_t> &a, const tuple<uint64_t, uint64_t, uint64_t> &b) -> bool {
             return get<2>(a) > get<2>(b);
         });

    //start with each min as a seed and build from there
    for (auto seed : minCounts)
    {
        set<uint64_t> k;
        k.insert(get<0>(seed));
        k.insert(get<1>(seed));

        while (!isStronglyConnected(matrix, k))
        {
            //get all edges originating in the kernel, add the biggest
            vector<tuple<uint64_t, uint64_t>> potentials;
            for (auto ki : k)
            {
                for (int i = 0; i < matrix[ki].size(); i++)
                {
                    if (matrix[ki][i] != 0)
                    {
                        potentials.push_back({i, matrix[ki][i]});
                    }
                }
            }
            sort(potentials.begin(), potentials.end(),
                 [](const tuple<uint64_t, uint64_t> &a, const tuple<uint64_t, uint64_t> &b) -> bool {
                     return get<1>(a) > get<1>(b);
                 });
            for (int i = 0; i < potentials.size(); i++)
            {
                auto block = get<0>(potentials[i]);
                if (k.find(block) == k.end())
                {
                    k.insert(block);
                    break;
                }
            }
        }

        result.insert(k);
    }

    return result;
}