#ifndef __XM_PERF_H
#define __XM_PERF_H

#include <cstdlib>
#include "utils.h"

using namespace std;

class perf
{
private:
    bool over;
    string sTag;
    int64_t startTime;
    perf(const perf&) {}
public:
    perf() = default;

    perf(string tag)
    {
        over = false;
        sTag = tag;
        startTime = get_time();
        printf("PERF-%s: start\n", sTag.c_str());
    }
    void start(string tag)
    {
        over = false;
        sTag = tag;
        startTime = get_time();
        printf("PERF-%s: start\n", sTag.c_str());
    }

    virtual ~perf()
    {
        if (!over)
        {
            done();
        }
    }

    void reset()
    {
        startTime = get_time();
        over = false;
        printf("PERF-%s: restart\n", sTag.c_str());
    }

    void done()
    {
        if (!over)
        {
            auto end = get_time();
            printf("PERF-%s: done, cost %.2f sec\n", sTag.c_str(), (end - startTime) / 1000.0);
            over = true;
        }
        else
        {
            printf("PERF-%s: already done\n", sTag.c_str());
        }
    }
};
#endif
