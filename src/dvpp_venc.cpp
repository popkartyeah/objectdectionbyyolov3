//
// Created by media on 20-11-24.
//


#include <iostream>
#include "acl/acl.h"
#include "dvpp_venc.h"

using namespace std;

DvppVenc::DvppVenc()
: deviceId_(0), context_(nullptr), vencFrameConfig_(nullptr), stream_(nullptr),isInitOk_(false), vencChannelDesc_(nullptr),state_(DVENC_VIDEO_START) {
    isGlobalContext_ = false;
}

DvppVenc::~DvppVenc()
{
    DestroyResource();
}

void DvppVenc::DestroyResource()
{
    aclError aclRet;
    state_ = DVENC_VIDEO_END;
    pthread_join(threadId_, NULL);
    aclvencDestroyFrameConfig(vencFrameConfig_);

    auto ret = aclvencDestroyChannel(vencChannelDesc_);
    aclvencDestroyChannelDesc(vencChannelDesc_);
    vencChannelDesc_ = nullptr;
    (void)aclrtUnSubscribeReport(static_cast<uint64_t>(threadId_), stream_);

    // destory thread
//    runFlag = false;
    void *res = nullptr;
    int joinThreadErr = pthread_join(threadId_, &res);

//    fclose(outFileFp_);

    ret = aclrtDestroyStream(stream_);
    stream_ = nullptr;
    ret = aclrtDestroyContext(context_);
    context_ = nullptr;
//    if (dvppChannelDesc_ != nullptr) {
//        aclRet = acldvppDestroyChannel(dvppChannelDesc_);
//        if (aclRet != ACL_ERROR_NONE) {
//            ERROR_LOG("acldvppDestroyChannel failed, aclRet = %d", aclRet);
//        }
//
//        (void)acldvppDestroyChannelDesc(dvppChannelDesc_);
//        dvppChannelDesc_ = nullptr;
//    }
}

Result DvppVenc::InitResource()
{
        aclError aclRet;

    //    const char *aclConfigPath = "../src/acl.json";
    //    aclError aclRet = aclInit(aclConfigPath);
    //    if (aclRet != ACL_ERROR_NONE) {
    //        ERROR_LOG("acl init failed");
    //        return FAILED;
    //    }
    //    INFO_LOG("acl init success");
    aclRet = aclrtSetDevice(deviceId_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acl open device %d failed", deviceId_);
        return FAILED;
    }
    INFO_LOG("open device %d success", deviceId_);

    // create context (set current)
    aclRet = aclrtCreateContext(&context_, deviceId_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acl create context failed");
        return FAILED;
    }
    INFO_LOG("create context success");
    // create stream
    aclRet = aclrtCreateStream(&stream_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("dvpp create stream failed");
        return FAILED;
    }

//    dvppChannelDesc_ = acldvppCreateChannelDesc();
//    if (dvppChannelDesc_ == nullptr) {
//        ERROR_LOG("acldvppCreateChannelDesc failed");
//        return FAILED;
//    }

//    aclRet = acldvppCreateChannel(dvppChannelDesc_);
//    if (aclRet != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppCreateChannel failed, aclRet = %d", aclRet);
//        return FAILED;
//    }
    //    stream_ = stream;
    isInitOk_ = true;
    INFO_LOG("dvpp init resource ok");
    return SUCCESS;
}

//Result DvppVenc::Resize(ImageData& dest, ImageData& src,
//uint32_t width, uint32_t height) {
//    aclrtSetCurrentContext(getContext());
//    DvppResize resizeOp(stream_, dvppChannelDesc_, width, height);
//    return resizeOp.Process(dest, src);
//}

//Result DvppVenc::DrawRect(ImageData& src,vector<Rect> rects) {
//    if (rects.size() <= 0)
//        return SUCCESS;
//    aclrtSetCurrentContext(getContext());
//    DvppCrop drawOp(stream_, dvppChannelDesc_, rects);
//    return drawOp.DrawRect(src, rects);
//}
//
//Result DvppVenc::CvtJpegToYuv420sp(ImageData& dest, ImageData& src) {
//    aclrtSetCurrentContext(getContext());
//    DvppJpegD jpegD(stream_, dvppChannelDesc_);
//    return jpegD.Process(dest, src);
//}

static void *ThreadEncFunc(void *arg)
{
    // Notice: create context for this thread
    int32_t* runFlag = reinterpret_cast<int32_t*>(arg);
    int deviceId = 0;
    aclrtContext context = nullptr;
    aclError ret = aclrtCreateContext(&context, deviceId);
    while (*runFlag == 0) {
        // Notice: timeout 1000ms
        aclError aclRet = aclrtProcessReport(1000);
    }

    ret = aclrtDestroyContext(context);
    return (void*)0;
}

void DvppVenc::Wait()
{
//    vd->wait();
}

Result DvppVenc::setChannel(acldvppStreamFormat video_type, uint32_t inputWidth, uint32_t inputHeight)
{
    //示例中使用的是H265_MAIN_LEVEL视频编码协议
    auto ret = aclvencSetChannelDescEnType(vencChannelDesc_, video_type);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvencSetChannelDescEnType failed, aclRet = %d", ret);
        return FAILED;
    }
    //示例中使用的是PIXEL_FORMAT_YVU_SEMIPLANAR_420
    ret = aclvencSetChannelDescPicFormat(vencChannelDesc_, PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    ret = aclvencSetChannelDescPicWidth(vencChannelDesc_, inputWidth);
    ret = aclvencSetChannelDescPicHeight(vencChannelDesc_, inputHeight);
    ret = aclvencSetChannelDescKeyFrameInterval(vencChannelDesc_, 30);
//    ret = aclvencSetChannelDescOutPicFormat(vencChannelDesc_, PIXEL_FORMAT_YVU_SEMIPLANAR_420);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("aclvencSetChannelDescOutPicFormat failed, aclRet = %d", ret);
//        return FAILED;
//    }
    /* 5.创建视频码流处理的通道 */
    ret = aclvencCreateChannel(vencChannelDesc_);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvencCreateChannel failed, aclRet = %d", ret);
        return FAILED;
    }
    vencFrameConfig_ = aclvencCreateFrameConfig();
    ret = aclvencSetFrameConfigForceIFrame(vencFrameConfig_, 0);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvencSetFrameConfigForceIFrame failed, aclRet = %d", ret);
        return FAILED;
    }
    return SUCCESS;
}

Result DvppVenc::Restrister(aclvencCallback func)
{
    aclError ret;

    if (func == NULL)
        return FAILED;
    /* Vdec 资源初始化 */
    // create threadId
    int createThreadErr = pthread_create(&threadId_, nullptr, ThreadEncFunc, &state_);
    aclrtSubscribeReport(static_cast<uint64_t>(threadId_), stream_);

//    streamInputDesc_ = acldvppCreateStreamDesc();

    vencChannelDesc_ = aclvencCreateChannelDesc();

    ret = aclvencSetChannelDescThreadId(vencChannelDesc_, threadId_);
    /* 设置回调函数 callback*/
    ret = aclvencSetChannelDescCallback(vencChannelDesc_, func);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvencSetChannelDescCallback failed, aclRet = %d", ret);
        return FAILED;
    }
    setChannel(H264_HIGH_LEVEL, 1920, 1080);
    //示例中使用的是H265_MAIN_LEVEL视频编码协议
//    ret = aclvencSetChannelDescEnType(vencChannelDesc_, H265_MAIN_LEVEL);
    //vdecChannelDesc_是aclvdecChannelDesc类型
//    vdecChannelDesc_ = aclvdecCreateChannelDesc();
//
//    // channelId: 0-15
//    ret = aclvdecSetChannelDescChannelId(vdecChannelDesc_, 10);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("aclvdecSetChannelDescChannelId failed, aclRet = %d", ret);
//        return FAILED;
//    }
//    ret = aclvdecSetChannelDescThreadId(vdecChannelDesc_, threadId_);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("aclvdecSetChannelDescThreadId failed, aclRet = %d", ret);
//        return FAILED;
//    }
//    /* 设置回调函数 callback*/
//    ret = aclvdecSetChannelDescCallback(vdecChannelDesc_, func);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("aclvdecSetChannelDescCallback failed, aclRet = %d", ret);
//        return FAILED;
//    }

    INFO_LOG("start create video encode");
//    vd = new VideoDecode();
//    cb = &rrf;
//    rrf.setData(static_cast<void*>(this));
//    vd->registerCallBack(cb);
//    vd->Init(rtsp_url);
//    vd->read();
    //    vd->wait();
    //    Wait();
    return SUCCESS;
}

Result DvppVenc::SendFrame(acldvppPicDesc *encodeInputDesc_)
{
//    acldvppPicDesc *encodeInputDesc_ = acldvppCreatePicDesc();
//    auto ret = acldvppSetPicDescWidth(encodeInputDesc_, img.width);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetPicDescWidth failed, aclRet = %d", ret);
//        return FAILED;
//    }
//    ret = acldvppSetPicDescHeight(encodeInputDesc_, img.height);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetPicDescHeight failed, aclRet = %d", ret);
//        return FAILED;
//    }
//
//    ret = acldvppSetPicDescFormat(encodeInputDesc_, PIXEL_FORMAT_YVU_SEMIPLANAR_420);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetPicDescFormat failed, aclRet = %d", ret);
//        return FAILED;
//    }
//    acldvppSetPicDescData(encodeInputDesc_, reinterpret_cast<void *>(img.data.get()));
//    acldvppSetPicDescSize(encodeInputDesc_, img.size);
//    INFO_LOG("encode image data size %d", img.size);
    aclrtSetCurrentContext(getContext());
    // 执行视频码流编码，编码每帧数据后，系统自动调用callback回调函数将编码后的数据写入文件，再及时释放相关资源
    auto ret = aclvencSendFrame(vencChannelDesc_, encodeInputDesc_, nullptr, vencFrameConfig_, nullptr);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvencSendFrame failed, aclRet = %d", ret);
        return FAILED;
    }
//    acldvppDestroyPicDesc(encodeInputDesc_);
    return SUCCESS;
}


Result DvppVenc::SendFrameEnd()
{

    aclrtSetCurrentContext(getContext());
    aclvencSetFrameConfigEos(vencFrameConfig_,1);
    // 执行视频码流编码，编码每帧数据后，系统自动调用callback回调函数将编码后的数据写入文件，再及时释放相关资源
    auto ret = aclvencSendFrame(vencChannelDesc_, nullptr, nullptr, vencFrameConfig_, nullptr);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvencSendFrame failed, aclRet = %d", ret);
        return FAILED;
    }
    //    acldvppDestroyPicDesc(encodeInputDesc_);
    return SUCCESS;
}
//ReadRTSPbyFFmpeg::ReadRTSPbyFFmpeg() : user_data(nullptr)
//{
//
//}
//
//ReadRTSPbyFFmpeg::~ReadRTSPbyFFmpeg()
//{
//
//}

//void ReadRTSPbyFFmpeg::CallBackInitFun(int32_t width, int32_t height,acldvppStreamFormat video_type)
//{
//    DvppProcess *dp = (DvppProcess *)user_data;
//    VideoDecode *vd = dp->getVDObject();
//    aclrtSetCurrentContext(dp->getContext());
//    dp->setChannel(video_type);
//
//}

//void ReadRTSPbyFFmpeg::CallBackFun(uint8_t *frame, int32_t frame_size)
//{
//    void *dataDev;
//    void *picOutBufferDev_;
//    aclError ret;
//    if (user_data == nullptr)
//        return ;
//    //    INFO_LOG("frame size %d", frame_size);
//    DvppProcess *dp = (DvppProcess *)user_data;
//    VideoDecode *vd = dp->getVDObject();
//    size_t DataSize = (vd->getWidth() * vd->getHeight() * 3) / 2;
//
//    aclrtSetCurrentContext(dp->getContext());
//    auto aclRet = acldvppMalloc(&dataDev, frame_size);
//    if (aclRet != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppMalloc frame failed, aclRet = %d", aclRet);
//    }
//    // copy input to device memory
//    aclRet = aclrtMemcpy(dataDev, frame_size, frame, frame_size, ACL_MEMCPY_HOST_TO_DEVICE);
//
//    //inBufferDev_表示Device存放输入视频数据的内存，inBufferSize_表示内存大小
//    ret = acldvppSetStreamDescData(dp->getVdecStreamDesc(), dataDev);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetStreamDescData failed, aclRet = %d", aclRet);
//    }
//    ret = acldvppSetStreamDescSize(dp->getVdecStreamDesc(), frame_size);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetStreamDescSize failed, aclRet = %d", aclRet);
//    }
//    //申请Device内存picOutBufferDev_，用于存放VDEC解码后的输出数据
//    ret = acldvppMalloc(&picOutBufferDev_, DataSize);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppMalloc failed, aclRet = %d", aclRet);
//    }
//    //创建输出图片描述信息，设置图片描述信息的属性
//    //picOutputDesc_是acldvppPicDesc类型
//    acldvppPicDesc *picOutputDesc_ = acldvppCreatePicDesc();
//    ret = acldvppSetPicDescData(picOutputDesc_, picOutBufferDev_);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetPicDescData failed, aclRet = %d", aclRet);
//    }
//    ret = acldvppSetPicDescSize(picOutputDesc_, DataSize);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetPicDescSize failed, aclRet = %d", aclRet);
//    }
//
//    ret = acldvppSetPicDescWidth(picOutputDesc_, vd->getWidth());
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetPicDescWidth failed, aclRet = %d", aclRet);
//    }
//    ret = acldvppSetPicDescHeight(picOutputDesc_, vd->getHeight());
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetPicDescHeight failed, aclRet = %d", aclRet);
//    }
//
//    ret = acldvppSetPicDescFormat(picOutputDesc_, PIXEL_FORMAT_YVU_SEMIPLANAR_420);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppSetPicDescFormat failed, aclRet = %d", aclRet);
//    }
//    // 执行视频码流解码，解码每帧数据后，系统自动调用callback回调函数将解码后的数据写入文件，再及时释放相关资源
//    ret = aclvdecSendFrame(dp->getVdecChannelDesc(), dp->getVdecStreamDesc(), picOutputDesc_, nullptr, nullptr);
//    if (ret != ACL_ERROR_NONE) {
//        ERROR_LOG("aclvdecSendFrame failed, aclRet = %d", aclRet);
//    }
//    //    INFO_LOG("read frame from ffmpeg w: %d, h: %d\n", vd->getWidth(), vd->getHeight());
//    //......
//    acldvppFree(dataDev);
//}