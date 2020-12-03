/**
* Copyright 2020 Huawei Technologies Co., Ltd
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.apache.org/licenses/LICENSE-2.0

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.

* File dvpp_process.cpp
* Description: handle dvpp process
*/

#include <iostream>
#include "acl/acl.h"
#include "dvpp_resize.h"
#include "dvpp_crop.h"
#include "dvpp_jpegd.h"
#include "dvpp_process.h"

using namespace std;

DvppProcess::DvppProcess()
    : deviceId_(0), context_(nullptr), stream_(nullptr),isInitOk_(false), dvppChannelDesc_(nullptr),state_(DVPP_VIDEO_START) {
    isGlobalContext_ = false;
}

DvppProcess::~DvppProcess()
{
    DestroyResource();
}

void DvppProcess::DestroyResource()
{
    aclError aclRet;
    state_ = DVPP_VIDEO_END;
    pthread_join(threadId_, NULL);
    if (dvppChannelDesc_ != nullptr) {
        aclRet = acldvppDestroyChannel(dvppChannelDesc_);
        if (aclRet != ACL_ERROR_NONE) {
            ERROR_LOG("acldvppDestroyChannel failed, aclRet = %d", aclRet);
        }

        (void)acldvppDestroyChannelDesc(dvppChannelDesc_);
        dvppChannelDesc_ = nullptr;
    }
}

Result DvppProcess::InitResource()
{
//    aclError aclRet;

//    const char *aclConfigPath = "../src/acl.json";
//    aclError aclRet = aclInit(aclConfigPath);
//    if (aclRet != ACL_ERROR_NONE) {
//        ERROR_LOG("acl init failed");
//        return FAILED;
//    }
//    INFO_LOG("acl init success");
    auto aclRet = aclrtSetDevice(deviceId_);
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

    dvppChannelDesc_ = acldvppCreateChannelDesc();
    if (dvppChannelDesc_ == nullptr) {
        ERROR_LOG("acldvppCreateChannelDesc failed");
        return FAILED;
    }

    aclRet = acldvppCreateChannel(dvppChannelDesc_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppCreateChannel failed, aclRet = %d", aclRet);
        return FAILED;
    }
//    stream_ = stream;
    isInitOk_ = true;
    INFO_LOG("dvpp init resource ok");
    return SUCCESS;
}

Result DvppProcess::Resize(ImageData& dest, ImageData& src,
                        uint32_t width, uint32_t height) {
    aclrtSetCurrentContext(getContext());
    DvppResize resizeOp(stream_, dvppChannelDesc_, width, height);
    return resizeOp.Process(dest, src);
}

Result DvppProcess::DrawRect(ImageData& src,vector<Rect> rects) {
    if (rects.size() <= 0)
        return SUCCESS;
    aclrtSetCurrentContext(getContext());
    DvppCrop drawOp(stream_, dvppChannelDesc_, rects);
    return drawOp.DrawRect(src, rects);
}

Result DvppProcess::CvtJpegToYuv420sp(ImageData& dest, ImageData& src) {
    aclrtSetCurrentContext(getContext());
    DvppJpegD jpegD(stream_, dvppChannelDesc_);
    return jpegD.Process(dest, src);
}

static void *ThreadFunc(void *arg)
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

void DvppProcess::Wait()
{
    vd->wait();
}

Result DvppProcess::setChannel(acldvppStreamFormat video_type)
{
    //示例中使用的是H265_MAIN_LEVEL视频编码协议
    auto ret = aclvdecSetChannelDescEnType(vdecChannelDesc_, video_type);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvdecSetChannelDescEnType failed, aclRet = %d", ret);
        return FAILED;
    }
    //示例中使用的是PIXEL_FORMAT_YVU_SEMIPLANAR_420
    ret = aclvdecSetChannelDescOutPicFormat(vdecChannelDesc_, PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvdecSetChannelDescOutPicFormat failed, aclRet = %d", ret);
        return FAILED;
    }
    /* 5.创建视频码流处理的通道 */
    ret = aclvdecCreateChannel(vdecChannelDesc_);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvdecCreateChannel failed, aclRet = %d", ret);
        return FAILED;
    }
    return SUCCESS;
}

Result DvppProcess::Restrister(std::string rtsp_url, aclvdecCallback func)
{
    aclError ret;

    if (func == NULL)
        return FAILED;
    /* Vdec 资源初始化 */
    // create threadId
    int createThreadErr = pthread_create(&threadId_, nullptr, ThreadFunc, &state_);
    aclrtSubscribeReport(static_cast<uint64_t>(threadId_), stream_);

    streamInputDesc_ = acldvppCreateStreamDesc();
    //vdecChannelDesc_是aclvdecChannelDesc类型
    vdecChannelDesc_ = aclvdecCreateChannelDesc();

    // channelId: 0-15
    ret = aclvdecSetChannelDescChannelId(vdecChannelDesc_, 10);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvdecSetChannelDescChannelId failed, aclRet = %d", ret);
        return FAILED;
    }
    ret = aclvdecSetChannelDescThreadId(vdecChannelDesc_, threadId_);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvdecSetChannelDescThreadId failed, aclRet = %d", ret);
        return FAILED;
    }
    /* 设置回调函数 callback*/
    ret = aclvdecSetChannelDescCallback(vdecChannelDesc_, func);
    if (ret != ACL_ERROR_NONE) {
        ERROR_LOG("aclvdecSetChannelDescCallback failed, aclRet = %d", ret);
        return FAILED;
    }

    INFO_LOG("start create video decode");
    vd = new VideoDecode();
    cb = &rrf;
    rrf.setData(static_cast<void*>(this));
    vd->registerCallBack(cb);
    vd->Init(rtsp_url);
    vd->read();
//    vd->wait();
    INFO_LOG("dvpp register ok");

//    Wait();
    return SUCCESS;
}

ReadRTSPbyFFmpeg::ReadRTSPbyFFmpeg() : user_data(nullptr)
{

}

ReadRTSPbyFFmpeg::~ReadRTSPbyFFmpeg()
{

}

void ReadRTSPbyFFmpeg::CallBackInitFun(int32_t width, int32_t height,acldvppStreamFormat video_type)
{
    DvppProcess *dp = (DvppProcess *)user_data;
    VideoDecode *vd = dp->getVDObject();
    aclrtSetCurrentContext(dp->getContext());
    dp->setChannel(video_type);

}

void ReadRTSPbyFFmpeg::CallBackFun(uint8_t *frame, int32_t frame_size)
{
//    void *dataDev;
    void *picOutBufferDev_;
    aclError ret;
    if (user_data == nullptr)
        return ;
//    INFO_LOG("frame size %d", frame_size);
    DvppProcess *dp = (DvppProcess *)user_data;
    VideoDecode *vd = dp->getVDObject();
    size_t DataSize = (vd->getWidth() * vd->getHeight() * 3) / 2;

    aclrtSetCurrentContext(dp->getContext());

    if(frame_size == 0 || frame == nullptr)
    {
        acldvppSetStreamDescEos(dp->getVdecStreamDesc() ,1);
        ret = aclvdecSendFrame(dp->getVdecChannelDesc(), dp->getVdecStreamDesc(), nullptr/*picOutputDesc_*/, nullptr, nullptr);
        if (ret != ACL_ERROR_NONE) {
            ERROR_LOG("aclvdecSendFrame failed, aclRet = %d", ret);
        }
        return ;
    }

    //inBufferDev_表示Device存放输入视频数据的内存，inBufferSize_表示内存大小
    auto aclRet = acldvppSetStreamDescData(dp->getVdecStreamDesc(), frame);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppSetStreamDescData failed, aclRet = %d", aclRet);
    }
    aclRet = acldvppSetStreamDescSize(dp->getVdecStreamDesc(), frame_size);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppSetStreamDescSize failed, aclRet = %d", aclRet);
    }
    //申请Device内存picOutBufferDev_，用于存放VDEC解码后的输出数据
    aclRet = acldvppMalloc(&picOutBufferDev_, DataSize);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppMalloc failed, aclRet = %d", aclRet);
    }
    //创建输出图片描述信息，设置图片描述信息的属性
    //picOutputDesc_是acldvppPicDesc类型
    acldvppPicDesc *picOutputDesc_ = acldvppCreatePicDesc();
    aclRet = acldvppSetPicDescData(picOutputDesc_, picOutBufferDev_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppSetPicDescData failed, aclRet = %d", aclRet);
    }
    aclRet = acldvppSetPicDescSize(picOutputDesc_, DataSize);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppSetPicDescSize failed, aclRet = %d", aclRet);
    }

    aclRet = acldvppSetPicDescWidth(picOutputDesc_, vd->getWidth());
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppSetPicDescWidth failed, aclRet = %d", aclRet);
    }
    aclRet = acldvppSetPicDescHeight(picOutputDesc_, vd->getHeight());
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppSetPicDescHeight failed, aclRet = %d", aclRet);
    }

    aclRet = acldvppSetPicDescFormat(picOutputDesc_, PIXEL_FORMAT_YVU_SEMIPLANAR_420);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppSetPicDescFormat failed, aclRet = %d", aclRet);
    }
    // 执行视频码流解码，解码每帧数据后，系统自动调用callback回调函数将解码后的数据写入文件，再及时释放相关资源
    aclRet = aclvdecSendFrame(dp->getVdecChannelDesc(), dp->getVdecStreamDesc(), picOutputDesc_, nullptr, nullptr);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("aclvdecSendFrame failed, aclRet = %d", aclRet);
    }
//    acldvppDestroyPicDesc(picOutputDesc_);
//    INFO_LOG("read frame from ffmpeg w: %d, h: %d\n", vd->getWidth(), vd->getHeight());
    //......
}