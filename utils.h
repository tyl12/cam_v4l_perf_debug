#ifndef __UTILS_H
#define __UTILS_H
#include <set>
#include <string>
#include <regex>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <thread>
#include <stdarg.h>
//#ifndef DEBUG_NATIVE
//#include "../jni/jniapi.h"
//#endif

#define PPG_EXPORTS __attribute__ ((visibility("default")))

#ifdef OPENCV
#include "PingGaiNet.h"
#endif

std::string tm_pr(); /* cur time at the form of '%m/%d %H:%M:%S' */
#if !defined(__ANDROID__)|| defined DEBUG_NATIVE || defined DEBUG_CAMERA_PCI
#define LOG_I(fmt, ...) \
do { \
  printf("%s ", tm_pr().c_str()); \
  printf((fmt), ##__VA_ARGS__); \
  printf("\n"); \
  fflush(NULL); } /* to get a continuous LOG */ \
while(0) /* swallow the semicolon (in else statements) */

#define LOG_E(fmt, ...) \
do { \
  printf("\x1B[31m" "%s ", tm_pr().c_str()); \
  printf("FATAL ERROR:\n" "\x1B[37m"); \
  LOG_I(fmt, ##__VA_ARGS__); \
  exit(1); } \
while(0)

#else
//#include <android/log.h>
//// #define LOG_NDEBUG 0 /* not found in log.h */
//#define LOG_TAG "VISION"
//#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
//#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif

using namespace std;

using dstr = std::pair<std::string, std::string>;
using names_t = std::vector<std::string>;
using assem_t = std::map<std::string, int>;
using map_t = std::map<std::string, std::vector<std::string>>;
using dvec = vector<vector<int>>;

template <typename T>
string to_string_android(T t)
{
    stringstream ss;
    ss << t;
    return ss.str();
}

std::string get_date(); /* get current date with format '%Y_%m_%d_%H_%M_%S' */
int64_t get_time();
std::string getDateFromTime(time_t t);
string multiHyphen(int count, const char symbol = '-');
int placeholdLen(string str);
void printHead(int len, const string& str, const char symbol = '-');
names_t splitByRegex(const string& s, const char* delim);
pair<int, int> parseDesc(string desc);

string getFileName(const string& s);

#ifdef OPENCV
using images_t = std::vector<std::vector<DetectionImage>>;
void initImages(images_t& imgs, int p1, vector<int> p2, ImageType type);
bool haze_detection(cv::Mat &im, int, int, int);
#if 0
void reverseImages(images_t& imgs, void *reversed = NULL);
#endif
#endif

class GoodInfo
{
private:
    names_t _objNames;
    assem_t _assembleMap;
    map_t _goodsMap;

public:
    GoodInfo() = default;
    void initNames(const string&);
    string findGoodCode(size_t idx);
    string findChName(size_t idx);
    string findName(size_t idx);
    names_t getNames();
    int getUniqueIndex(const string& name) const;
};

class ShelfInfo
{
private:
    int _layerCount;
    vector<int> _layerCamCount;
public:
    ShelfInfo() = default;
    ShelfInfo(int p1, vector<int> p2): _layerCount(p1), _layerCamCount(p2) {} /* only use for debugging */
    ShelfInfo& operator=(const ShelfInfo&);
    void init(vector<string>);
    int getLayerCount();
    vector<int> getCamCount();
};

class PPGException: public exception
{
private:
    string _msg;
public:
    explicit PPGException(const string& msg);
    const char *what() const noexcept;
};

typedef struct {
    int64_t startTime;
    int64_t endTime;
    char userInfo[256];
} ShoppingMetaInfo;


class thread_guard{
private:
    thread mThread;
public:
    explicit thread_guard(thread&& t): mThread(move(t)){
        LOG_I(__PRETTY_FUNCTION__);
    }
    thread_guard(thread_guard&& o) noexcept: mThread(move(o.mThread)){
        LOG_I(__PRETTY_FUNCTION__);
    }
    thread_guard& operator=(thread_guard&& o) noexcept{
        LOG_I(__PRETTY_FUNCTION__);
        mThread=move(o.mThread);
        return *this;
    }
    ~thread_guard(){
        LOG_I(__PRETTY_FUNCTION__);
        join();
    }
    bool joinable(){
        return mThread.joinable();
    }
    void join(){
        if (mThread.joinable())
            mThread.join();
    }
    thread_guard(thread& t) = delete;
    thread_guard(const thread& t) = delete;
    thread_guard& operator=(thread& t) = delete;
};
void printName(GoodInfo& gi, vector<int> vec, bool isCh = false);
vector<int> combineResult(GoodInfo& gi, dvec dv);
#ifndef DEBUG_NATIVE
void native_callback(GoodInfo& gi, const vector<int>& results);
#endif
void err_quit(const char *fmt, ...);
void err_sys(const char *fmt, ...);
#endif
