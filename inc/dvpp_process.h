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

* File dvpp_process.h
* Description: handle dvpp process
*/
#pragma once
#include <cstdint>

#include "acl/acl.h"
#include "acl/ops/acl_dvpp.h"
#include "utils.h"
#include "ffmpeg_read.h"

typedef void (*CBFunc)(acldvppStreamDesc *input, acldvppPicDesc *output, void *userdata);
/**
 * DvppProcess
 */

typedef enum E_DVPP_STATE {
    DVPP_VIDEO_START = 0,
    DVPP_VIDEO_END = 1
} E_DVPP_STATE;

class ReadRTSPbyFFmpeg : public CallBack
{
    private :
    void *user_data;
    public :
    ReadRTSPbyFFmpeg();
    virtual ~ReadRTSPbyFFmpeg();
    void *getData() {return user_data;}
    void setData(void *user_data) {this->user_data = user_data;}
    virtual void CallBackInitFun(int32_t width, int32_t height,acldvppStreamFormat video_type);
    virtual void CallBackFun(uint8_t *frame, int32_t frame_size);
};

class DvppProcess {
public:
    DvppProcess();

    ~DvppProcess();

    Result Resize(ImageData& src, ImageData& dest,
                  uint32_t width, uint32_t height);
    Result DrawRect(ImageData& src,vector<Rect> rects);
    Result CvtJpegToYuv420sp(ImageData& src, ImageData& dest);
    Result InitResource();
    void DestroyResource();

    Result setChannel(acldvppStreamFormat video_type);

    Result Restrister(std::string rtsp_url, aclvdecCallback func);
    aclvdecChannelDesc* getVdecChannelDesc(){return vdecChannelDesc_;}
    acldvppStreamDesc* getVdecStreamDesc(){return streamInputDesc_;}
    aclrtStream getRTStreamDesc(){return stream_;}
    VideoDecode *getVDObject(){return vd;}
    aclrtContext getContext(){return context_;}
    void Wait();
    Result SaveDvppOutputData(const char *fileName, void *devPtr, uint32_t dataSize);
//    void *ThreadFunc(void *arg);

protected:
    int isInitOk_;
    int32_t deviceId_;
    aclrtContext context_;
    aclrtStream stream_;
    /*picture decode*/
    acldvppChannelDesc *dvppChannelDesc_;
    /*video decode */
    aclvdecChannelDesc *vdecChannelDesc_;
    acldvppStreamDesc *streamInputDesc_;
    VideoDecode *vd;
    pthread_t threadId_;
    pthread_t encId_;
    bool isGlobalContext_;
    E_DVPP_STATE state_;
    CallBack *cb;
    CallBack *encCb;
    ReadRTSPbyFFmpeg rrf;
};

