//
// Created by david on 5/17/18.
//

#include "utils.h"
#include <cerrno>

using namespace std;


string tm_pr()
{
    time_t now;
    struct tm *ti;

    time(&now);
    ti = localtime(&now);

    char t_str[64];
    sprintf(t_str, "%02d/%02d %02d:%02d:%02d", ti->tm_mon + 1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec);

    return string(t_str);
}

int64_t get_time()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_millis = time_point_cast<milliseconds>(now);
    auto value = now_millis.time_since_epoch();

    return value.count();
}

std::string get_date()
{
    using namespace std::chrono;
    char tp[64] = { '\0' };
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);
    strftime(tp, 64, "%Y_%m_%d_%H_%M_%S", &tm);
    return std::string(tp);
}

std::string getDateFromTime(time_t t)
{
    char tp[64] = {'\0'};
    std::tm tm = *std::localtime(&t);
    strftime(tp, 64, "%Y_%m_%d_%H_%M_%S", &tm);
    return std::string(tp);
}

string multiHyphen(int count, const char symbol)
{
    string tmp;
    tmp.assign(count, symbol);
    return tmp;
}

/* get string placeholder length */
int placeholdLen(string str)
{
    auto length = 0;
    auto utf8Len = 0;
    for (auto&& c: str)
    {
        if ((c >> 7) == 0) /* considering Chinese chars */
            ++length;
        else
            ++utf8Len;
    }

    return length + utf8Len / 3 * 2;
}

void printHead(int len, const string& str, const char symbol)
{
    auto leftLen = (len - str.size() - 2) >> 1;
    auto rightLen = len - str.size() - 2 - leftLen;

    LOG_I("%s %s %s", multiHyphen(leftLen, symbol).c_str(), str.c_str(), multiHyphen(rightLen, symbol).c_str());
}

names_t splitByRegex(const string& s, const char* delim)
{
    vector<string> res;
    regex pat(delim);
    auto begin = sregex_iterator(s.begin(), s.end(), pat);
    auto end = sregex_iterator();
    for (auto i = begin; i != end; ++i) res.emplace_back((*i).str());

    return res;
}

pair<int, int> parseDesc(string desc)
{
    char* cdesc = strdup(desc.c_str());
    char* save = NULL;
    auto i1 = strtok_r(cdesc, "-", &save);
    auto i2 = strtok_r(NULL, "-", &save);
    auto p = make_pair(atoi(i1), atoi(i2));
    free(cdesc);

    return p;
}

#ifdef OPENCV /* to make utils.h a general tool */
void initImages(images_t& imgs, int p1, vector<int> p2, ImageType type)
{
    imgs.clear();
    auto hasNonPos = [&p2] {
        for (auto&& item: p2)
            if (item <= 0)
                return true;
        return false;
    }();

    if (p1 != p2.size() || hasNonPos)
        throw PPGException("bad shelf info");

    imgs.resize(p1);
    for (int i = 0; i < p1; ++i)
    {
        imgs[i].resize(p2[i]);
        for (int j = 0; j < p2[i]; ++j)
            imgs[i][j] = DetectionImage(i, j, type);
    }
}

using namespace cv;
using namespace cv::ml;
double haze_detection_blk(Mat &im)
{
    const int blockSize = 5;

    // Exclude white area
    Mat im_hsv;
    cvtColor(im,im_hsv,COLOR_BGR2HSV);

    Scalar low_w(0,0,110 );
    Scalar high_w(180,50,255);
    Mat white_mask;
    inRange(im_hsv, low_w, high_w, white_mask);
    white_mask = 255 - white_mask;

    Scalar low_b(0,0,0);
    Scalar high_b(180,50,30);
    Mat black_mask;
    inRange(im_hsv, low_b, high_b, black_mask);
    black_mask = 255 - black_mask;

    Mat msk_and, msk;
    bitwise_and(black_mask, white_mask, msk_and);

    Mat element =  getStructuringElement(MORPH_RECT,Size(blockSize,blockSize));
    dilate(msk_and, msk, element);

    double ratio;
    int total_pix = countNonZero(msk);
    int total_valid_pix = im.rows*im.cols;//countNonZero(valid_im_mask);
    ratio = (double)total_pix/total_valid_pix;

    double M,D;
    Mat im_gray, im_lap;
    cvtColor(im,im_gray,COLOR_BGR2GRAY);
    Laplacian(im_gray,im_lap,CV_8U, 3);
    Scalar lap_mean,lap_dev;
    meanStdDev(im_lap, lap_mean, lap_dev,msk);
    M = lap_mean.val[0];
    D = lap_dev.val[0];

    double D_gray;
    Scalar gray_mean,gray_dev;
    meanStdDev(im_gray,gray_mean,gray_dev);
    D_gray = gray_dev.val[0];
 
    LOG_I("HAZE: %.2f%%; lap_dev: %.2f; gray_dev: %.2f", ratio * 100, D, D_gray);

    constexpr double thr_w = 0.1;
    constexpr double thr_g = 15;

    double ret;
    if(ratio > thr_w && D_gray > thr_g)
        ret = D;
    else
        ret = 100;

    return ret;
}

bool haze_detection_grid(Mat &im, int blk_num, int top_num, int c_size)
{

    if(blk_num == -1)
        blk_num = 2;

    if(top_num == -1)
        top_num = 1;

    if (c_size == -1)
        c_size = 480;
    
    constexpr double thr_D = 15;
    bool ret = false;
    Size crop_size(c_size,c_size);

    int blk_size_x = crop_size.width/blk_num;
    int blk_size_y = crop_size.height/blk_num;
    vector<double> blk_ret;

    for(int i =0 ; i < blk_num;i++)
        for(int j=0;j < blk_num;j++)
        {
            Mat blk = im(Range((im.rows-crop_size.height)/2 + i*blk_size_y,(im.rows-crop_size.height)/2 + (i+1)*blk_size_y),Range((im.cols-crop_size.width)/2 + j*blk_size_x,(im.cols-crop_size.width)/2 + (j+1)*blk_size_x));
            double tmp_ret = haze_detection_blk(blk);
            blk_ret.push_back(tmp_ret);

        }


    sort(blk_ret.begin(),blk_ret.end());

    double sum = 0.0;
    for(int i=0;i<top_num;i++) sum+=blk_ret[i];

    sum/= top_num;

    if(sum < thr_D)
        ret = true;

    return ret;
}

bool haze_detection(Mat &im, int blk_num, int top_num, int c_size) // circle
{
    if(blk_num == -1)
        blk_num = 4;

    if(top_num == -1)
        top_num = 1;

    if (c_size == -1)
        c_size = 60;

    int haze_cnt =0;
    bool ret = false;
    Point p_center(640,360);
    int radius[5];// ={120,180,240,300,360};
    for(int i=0;i<blk_num;i++)
        radius[i] = c_size*(i+2);


    const char* data_xm_ptr = nullptr;
    data_xm_ptr = getenv("ENV_XIAOMENG_DATAPATH");
    if (data_xm_ptr==nullptr){
        LOG_E("can not get ENV_XIAOMENG_DATAPATH");
        exit(1);
    }

    if(blk_num!=5)
    {
        LOG_E("INVALID block number.");
        exit(1);
    }
    string data_xm = data_xm_ptr;
    Ptr< SVM > svm = SVM::create();
    svm = SVM::load(data_xm+"/svmtest.mat");
    vector<double> debug_info;
    int feature_n = 15;


    // Exclude white area
    Mat im_hsv;
    cvtColor(im,im_hsv,COLOR_BGR2HSV);

    Scalar low_w(0,0,30 );
    Scalar high_w(180,50,255);
    Mat white_mask;
    inRange(im_hsv, low_w, high_w, white_mask);
    white_mask = 255 - white_mask;

    Scalar low_b(0,0,0);
    Scalar high_b(180,50,30);
    Mat black_mask;
    inRange(im_hsv, low_b, high_b, black_mask);
    black_mask = 255 - black_mask;

    Mat msk_and, msk_full;
    bitwise_and(black_mask, white_mask, msk_and);

    Mat element =  getStructuringElement(MORPH_RECT,Size(5,5));
    dilate(msk_and, msk_full, element);

    // Calculate Laplacian
    Mat im_gray, im_lap;
    cvtColor(im,im_gray,COLOR_BGR2GRAY);
    Laplacian(im_gray,im_lap,CV_8U, 3);

    for(int i =0;i<blk_num;i++)
    {
        Mat region_msk =Mat::zeros(im.size(), CV_8U);
        int r, r_p;
        if(i==0)
        {
            r = radius[i];
            r_p = 0;
            circle(region_msk,p_center,r,255,-1);
        } else{
            r = radius[i];
            r_p = radius[i-1];
            circle(region_msk,p_center,r,255,-1);
            circle(region_msk,p_center,r_p,0,-1);
        }

        Mat msk;
        bitwise_and(region_msk, msk_full, msk);

        double ratio;
        int total_pix = countNonZero(msk);
        int total_valid_pix = countNonZero(region_msk);
        ratio = (double)total_pix/total_valid_pix;

        double M,D;
        Scalar lap_mean,lap_dev;
        meanStdDev(im_lap, lap_mean, lap_dev,msk);
        M = lap_mean.val[0];
        D = lap_dev.val[0];

        double D_gray;
        Scalar gray_mean,gray_dev;
        meanStdDev(im_gray,gray_mean,gray_dev);
        D_gray = gray_dev.val[0];


        LOG_I("HAZE: %.2f%%; lap_dev: %.2f; gray_dev: %.2f", ratio * 100, D, D_gray);
        // cout << ratio*100 << "% " <<" ;D: "<< D << " ;D_gray: "<< D_gray << endl;

        double thr_w = 0.1;
        double thr_g = 15;
        double thr_D = 15;
        double dist_pen = 1/(1+sqrt(i));
        double ratio_pen = ratio > 0.3? 1.0: sqrt(ratio/0.3);
        double thr_D_r = thr_D* ratio_pen * dist_pen;


        if(ratio > thr_w  && D_gray > thr_g && (D < thr_D_r) )
            haze_cnt++;


        debug_info.push_back(100*ratio);
        debug_info.push_back(D_gray);
        debug_info.push_back(D);
    }


    if(haze_cnt>=top_num)
        ret = true;

    Mat test_data(1,15,CV_32F);
    for(int j=0;j<15;j++) test_data.at<float>(0,j) = debug_info[j];
    Mat svm_output;
    svm->predict(test_data,svm_output);
    bool svm_ret = int(svm_output.at<float>(0,0))==1 ? true : false;
    LOG_I("svm predict: %s; org predict: %s.", svm_ret?"true":"false", ret?"true":"false");



    return svm_ret;

}

#if 0
void reverseImages(images_t& imgs, void *reserved)
{
    for (auto&& layer: imgs)
        for (auto&& img: layer)
        {
            img.reverse();
            if (reserved != NULL)
            {
                img.frame.release();
            }
        }
}
#endif
#endif

void GoodInfo::initNames(const string& name_path)
{
    std::ifstream ifs(name_path);
    if (!ifs.is_open())
    {
        LOG_E("FATAL ERROR: can not found voc.names");
    }

    std::string line;
    int width = 20;
    printHead(5 * width - 30, "parsing voc.names", '>');

    int uniqueIdx = 0;
    bool isDup = false;
    std::set<std::string> goodCodes;
    while (getline(ifs, line))
    {
        auto res = splitByRegex(line, "[^,\\t\\s]+");
        int idx = 0;
        string markName;
        string toBePrinted;
        vector<string> names;
        for (auto&& i: res)
        {
            if (idx == 2 || idx == 3)
                toBePrinted += multiHyphen(5 - placeholdLen(i), ' ');
            else
                toBePrinted += multiHyphen(20 - placeholdLen(i), ' ');

            toBePrinted += i;
            if (idx == 0) markName += i;
            if (idx == 2) markName += std::string("-") + i;
            if (idx == 3)
                markName += std::string("-") + i;
            if (idx == 1)
            {
                names.emplace_back(i);
                auto p = goodCodes.insert(i);
                p.second || (isDup = true); // assemble by good 69-code
            }
            if (idx == 4) names.emplace_back(i);

            ++idx;
        }

        LOG_I("%s", toBePrinted.c_str());

        _goodsMap.insert(std::pair<std::string, std::vector<std::string>>(markName, names));
        _objNames.emplace_back(markName);
        if (isDup)
        {
            /* assemble by 69-code */
            auto baseStr = markName.substr(0, markName.find("-")) + "-0-0";
            auto idx = _assembleMap.find(baseStr);
            _assembleMap[markName] = (idx == _assembleMap.end()) ? -1 : idx->second;
        }
        else
        {
            _assembleMap.insert(std::make_pair(markName, uniqueIdx));
        }
        ++uniqueIdx;
        isDup = false;
    }
    LOG_I("%s", multiHyphen(5 * width - 30, '<').c_str());
    LOG_I("\n");

    printHead(2 * width, "unique index", '>');

    /* to avoid the base good is below corresponding 'duplicated' good */
    for (auto&& pair : _assembleMap)
    {
        if (pair.second < 0)
        {
            auto baseStr = pair.first.substr(0, pair.first.find("-")) + "-0-0";
            auto idx = _assembleMap[baseStr]; // [] returns a lvalue
            _assembleMap[pair.first] = idx;
        }

        stringstream ss;
        ss << multiHyphen(2 * width - pair.first.size() - 5, ' ');
        ss << pair.first;
        ss << multiHyphen(2 * width - to_string_android(pair.second).size() - 35, ' ');
        ss << pair.second;
        LOG_I("%s", ss.str().c_str());
    }

    LOG_I("%s", multiHyphen(2 * width, '<').c_str());
}

string GoodInfo::findGoodCode(size_t idx)
{
    string markName = _objNames[idx];
    auto it = _goodsMap.find(markName);
    string code;
    if (it != _goodsMap.end())
    {
        code = it->second[0];
    }
    else
    {
        LOG_E("good name(%s) not found in voc.names", markName.c_str());
    }
    return code;
}

names_t GoodInfo::getNames()
{
    return _objNames;
}

string GoodInfo::findChName(size_t idx)
{
    string markName = _objNames[idx];
    auto res = _goodsMap.find(markName);
    return res->second[1];
}

string GoodInfo::findName(size_t idx)
{
    if (idx >= _objNames.size())
        throw PPGException("good index out of range");
    return _objNames[idx];
}

int GoodInfo::getUniqueIndex(const string& name) const
{
    auto it = _assembleMap.find(name);
    if (it == _assembleMap.end())
        throw PPGException("unknown good markname");
    return it->second;
}

ShelfInfo& ShelfInfo::operator=(const ShelfInfo &other)
{
    _layerCount = other._layerCount;
    _layerCamCount = other._layerCamCount;
    return *this;
}

void ShelfInfo::init(vector<string> descs)
{
    std::vector<std::set<int>> layerCamIndexVec;
    const char* delim = "-";

    for (auto&& desc: descs)
    {
        char* cdesc = strdup(desc.c_str());
        LOG_I("parsing description %s", desc.c_str());
        char* save = NULL;
        auto i1 = strtok_r(cdesc, delim, &save);
        auto i2 = strtok_r(NULL, delim, &save);
        LOG_I("%s %s", i1, i2);
        if (!i1 || !i2)
            throw PPGException("bad camera description");

        auto layerIndex = atoi(i1);
        auto camIndex = atoi(i2);

        free(cdesc);

        if (layerCamIndexVec.size() < layerIndex + 1)
            layerCamIndexVec.resize(layerIndex + 1);

        auto res = layerCamIndexVec[layerIndex].insert(camIndex);
        if (!res.second)
            throw PPGException("duplicate camera description");
    }

    for (auto&& s: layerCamIndexVec)
    {
        auto sum = std::accumulate(s.begin(), s.end(), 0);
        if (sum != s.size() * (s.size() - 1) / 2)
           throw PPGException("bad camera description");
    }

    _layerCount = layerCamIndexVec.size();
    for (auto&& s: layerCamIndexVec)
    {
        _layerCamCount.push_back(s.size());
    }
}

int ShelfInfo::getLayerCount() { return _layerCount; }
vector<int> ShelfInfo::getCamCount() { return _layerCamCount; }

void printName(GoodInfo& gi, std::vector<int> vec, bool isCh)
{
    auto i = 0;
    for (auto&& v: vec)
    {
        if (v != 0)
        {
            auto name = isCh ? gi.findChName(i) : gi.findName(i);
            name.erase(std::remove(name.begin(), name.end(), '\t'), name.end()); /* remove unwanted heading & trailing tabs */
            stringstream ss;
            ss << multiHyphen(25 - placeholdLen(name), ' ');
            ss << name;
            ss << multiHyphen(5 - to_string_android(v).size(), ' ');
            ss << v;
            LOG_I("%s", ss.str().c_str());
        }
        ++i;
    }
};

/* combine layer-based goods counting to get total info */
vector<int> combineResult(GoodInfo& gi, dvec dv) {
    //auto idx = 0;
    std::vector<int> res;
    res.swap(dv[0]);
    for (unsigned int i = 1; i < dv.size(); ++i)
        std::transform(dv[i].begin(), dv[i].end(), res.begin(), res.begin(), std::plus<int>());

    return res;
};

/*
 * usage:
 * string path = "xxx";
 * std::cout << "The file name is \"" << getFileName(path) << "\"\n";
 */
string getFileName(const string& s)
{
   char sep = '/';

#ifdef _WIN32
   sep = '\\';
#endif

   size_t i = s.rfind(sep, s.length());
   if (i != string::npos) {
       return(s.substr(i+1, s.length() - i));
   }
   return s;
}

PPGException::PPGException(const string& msg)
{
    this->_msg = msg;
}

const char* PPGException::what() const noexcept
{
    return _msg.c_str();
}

#define MAX_CHARS 128

static void err_doit(int errnoflag, int errnum, const char *fmt, va_list args)
{
    char buf[MAX_CHARS];

    vsnprintf(buf, MAX_CHARS - 1, fmt, args);
    if (errnoflag)
    {
        snprintf(buf + strlen(buf), MAX_CHARS - strlen(buf) - 1, ": %s", strerror(errnum));
    }

    strcat(buf, "\n");
    fflush(stdout);

    LOG_E("%s", buf);
    // fputs(buf, stderr);
    // fflush(NULL);
}

void err_quit(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    err_doit(0, 0, fmt, args);
    va_end(args);

    exit(1);
}

void err_sys(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    err_doit(1, errno, fmt, args);
    va_end(args);

    exit(1);
}
