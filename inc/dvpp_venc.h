//
// Created by media on 20-11-24.
//

#ifndef objectdetection_dvpp_venc_H
#define objectdetection_dvpp_venc_H
#pragma once
#include <cstdint>

#include "acl/acl.h"
#include "acl/ops/acl_dvpp.h"
#include "utils.h"
//#include "ffmpeg_read.h"

typedef void (*CBFunc)(acldvppStreamDesc *input, acldvppPicDesc *output, void *userdata);
/**
 * DvppVenc
 */

typedef enum E_DVENC_STATE {
    DVENC_VIDEO_START = 0,
    DVENC_VIDEO_END = 1
} E_DVENC_STATE;

//class ReadRTSPbyFFmpeg : public CallBack
//{
//    private :
//    void *user_data;
//    public :
//    ReadRTSPbyFFmpeg();
//    virtual ~ReadRTSPbyFFmpeg();
//    void *getData() {return user_data;}
//    void setData(void *user_data) {this->user_data = user_data;}
//    virtual void CallBackInitFun(int32_t width, int32_t height,acldvppStreamFormat video_type);
//    virtual void CallBackFun(uint8_t *frame, int32_t frame_size);
//};

class DvppVenc {
    public:
    DvppVenc();

    ~DvppVenc();
    Result InitResource();
    void DestroyResource();
    Result SendFrame(acldvppPicDesc *encodeInputDesc_);
    Result SendFrameEnd();
    Result setChannel(acldvppStreamFormat video_type, uint32_t inputWidth, uint32_t inputHeight);

    Result Restrister(aclvencCallback func);
//    aclvdecChannelDesc* getVdecChannelDesc(){return vdecChannelDesc_;}
//    acldvppStreamDesc* getVdecStreamDesc(){return streamInputDesc_;}
    aclrtStream getRTStreamDesc(){return stream_;}
//    VideoDecode *getVDObject(){return vd;}
    aclrtContext getContext(){return context_;}
    void Wait();
//    Result SaveDvppOutputData(const char *fileName, void *devPtr, uint32_t dataSize);
    //    void *ThreadFunc(void *arg);

protected:
    int isInitOk_;
    int32_t deviceId_;
    aclrtContext context_;
    aclrtStream stream_;
    /*picture decode*/
//    acldvppChannelDesc *dvppChannelDesc_;
    /*video decode */
    aclvencChannelDesc *vencChannelDesc_;
    aclvencFrameConfig *vencFrameConfig_;
    acldvppPicDesc *picOutputDesc_;
//    VideoDecode *vd;
    pthread_t threadId_;
//    pthread_t encId_;
    bool isGlobalContext_;
    E_DVENC_STATE state_;
//    CallBack *cb;
//    CallBack *encCb;
//    ReadRTSPbyFFmpeg rrf;
};
#endif //objectdetection_dvpp_venc_H
