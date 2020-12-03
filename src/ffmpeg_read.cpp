/**
 * ============================================================================
 *
 * Copyright (C) 2018, Hisilicon Technologies Co., Ltd. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1 Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   2 Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *
 *   3 Neither the names of the copyright holders nor the names of the
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * ============================================================================
 */

#include "ffmpeg_read.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/prctl.h>

#include <fstream>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>

//#include "hiaiengine/log.h"
//#include "hiaiengine/data_type_reg.h"
#include "acl/acl.h"
//#include "dvpp_jpegd.h"
#include "utils.h"
using namespace std;

namespace {
    const int kWait10Milliseconds = 10000;  // wait 10ms

    const int kNoFlag = 0;  // no flag

    const int kHandleSuccessful = 0;  // the process handled successfully

    const string kStrChannelId1 = "channel1";  // channle id 1 string

    const string kStrChannelId2 = "channel2";  // channle id 2 string

    // standard: 4096 * 4096 * 4 = 67108864 (64M)
    const int kAllowedMaxImageMemory = 67108864;

    const string kNeedRemoveStr = " \r\n\t";  // the string need remove

    const string kVideoImageParaType = "VideoImageParaT";  // video image para type

    const int kInvalidVideoIndex = -1;  // invalid video index

    const string kRtspTransport = "rtsp_transport";  // rtsp transport

    const string kUdp = "udp";  // video format udp

    const string kBufferSize = "buffer_size";  // buffer size string

    const string kMaxBufferSize = "104857600";  // maximum buffer size:100MB

    const string kMaxDelayStr = "max_delay";  // maximum delay string

    const string kMaxDelayValue = "100000000";  // maximum delay time:100s

    const string kTimeoutStr = "stimeout";  // timeout string

    const string kTimeoutValue = "5000000";  // timeout:5s

    const string kPktSize = "pkt_size";  // ffmpeg pakect size string

    const string kPktSizeValue = "10485760";  // ffmpeg packet size value:10MB

    const string kReorderQueueSize = "reorder_queue_size";  // reorder queue size

    const string kReorderQueueSizeValue = "0";  // reorder queue size value

    const string kThreadNameHead = "handle_";  // thread name head string

    const int kErrorBufferSize = 1024;  // buffer size for error info

    const int kThreadNameLength = 32;  // thread name string length

    const string kRegexSpace = "^[ ]*$";  // regex for check string is empty

    // regex for verify .mp4 file name
    const string kRegexMp4File = "^/((?!\\.\\.).)*\\.(mp4)$";

    // regex for verify RTSP rtsp://ip:port/channelname
    //const string kRegexRtsp =
    //    "^rtsp://(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|[0-9])\\."
    //        "(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\."
    //        "(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\."
    //        "(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)"
    //        ":([1-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|"
    //        "6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])/"
    //        "(.{1,100})$";
    const string kRegexRtsp =
    "^rtsp://(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|[0-9])\\."
    "(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\."
    "(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)\\."
    "(1\\d{2}|2[0-4]\\d|25[0-5]|[1-9]\\d|\\d)"
    ":([1-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|"
    "6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])/"
    "([0-4])$";
}



VideoDecode::VideoDecode()
    : video_type(H264_HIGH_LEVEL), width(1280), height(720)
{
    channel1_ = "";  // initialize channel1 to empty string
    channel2_ = "";  // initialize channel2 to empty string
}

VideoDecode::~VideoDecode() {
}

void VideoDecode::SendFinishedData() {
//    VideoImageInfoT video_image_info;
//    video_image_info.is_finished = true;
//
//    hiai::ImageData<unsigned char> image_data;
//    shared_ptr<VideoImageParaT> video_image_para = make_shared<VideoImageParaT>();
//    video_image_para->img = image_data;
//    video_image_para->video_image_info = video_image_info;
//
//    Result hiai_ret = SUCCESS;
//
//    // send finished data to next engine, use output port:0
//    do {
//        hiai_ret = SendData(0, kVideoImageParaType,
//        static_pointer_cast<void>(video_image_para));
//        if (hiai_ret == HIAI_QUEUE_FULL) {  // check hiai queue is full
//            INFO_LOG("Queue full when send finished data, sleep 10ms");
//            usleep(kWait10Milliseconds);  // sleep 10 ms
//        }
//    } while (hiai_ret == HIAI_QUEUE_FULL);  // loop when hiai queue is full
//
//    if (hiai_ret != SUCCESS) {
//        ERROR_LOG("Send finished data failed! error code: %d", hiai_ret);
//    }
}

int VideoDecode::GetVideoIndex(AVFormatContext* av_format_context) {
    if (av_format_context == nullptr) {  // verify input pointer
        return kInvalidVideoIndex;
    }

    // get video index in streams
    for (int i = 0; i < av_format_context->nb_streams; i++) {
        if (av_format_context->streams[i]->codecpar->codec_type
        == AVMEDIA_TYPE_VIDEO) {  // check is media type is video
            return i;
        }
    }

    return kInvalidVideoIndex;
}

bool VideoDecode::CheckVideoType(int video_index,
AVFormatContext* av_format_context,
acldvppStreamFormat &video_type) {
    AVStream* in_stream = av_format_context->streams[video_index];
    INFO_LOG("Display video stream resolution, width:%d, height:%d",
    in_stream->codecpar->width, in_stream->codecpar->height);

    if (in_stream->codecpar->codec_id == AV_CODEC_ID_H264) {  // video type: h264
        switch(in_stream->codecpar->profile) {
            case 77:
            video_type = H264_MAIN_LEVEL;
            break;
            case 66:
            video_type = H264_BASELINE_LEVEL;
            break;
            case 100:
            video_type = H264_HIGH_LEVEL;
            break;
            default:
            video_type = H264_HIGH_LEVEL;
        }

        INFO_LOG("Video type:H264");
        return true;
    } else if (in_stream->codecpar->codec_id == AV_CODEC_ID_HEVC) {  // h265
        video_type = H265_MAIN_LEVEL;
        INFO_LOG("Video type:H265");
        return true;
    } else {  // the video type is invalid
        video_type = H264_HIGH_LEVEL;
        INFO_LOG(
        "The video type is invalid, should be h264 or h265"
        "AVCodecID:%d(detail type please to view enum AVCodecID in ffmpeg)",
        in_stream->codecpar->codec_id);
        return false;
    }
    return false;
}

void VideoDecode::InitVideoStreamFilter(
acldvppStreamFormat video_type, const AVBitStreamFilter* &video_filter) {
    if (video_type > H265_MAIN_LEVEL) {  // check video type is h264
        video_filter = av_bsf_get_by_name("h264_mp4toannexb");
    } else {  // the video type is h265
        video_filter = av_bsf_get_by_name("hevc_mp4toannexb");
    }
}

const string &VideoDecode::GetChannelValue(const string &channel_id) {
    if (channel_id == kStrChannelId1) {  // check channel is channel1
        return channel1_;
    } else {  // the channel is channel2
        return channel2_;
    }
}

void VideoDecode::SetDictForRtsp(const string& channel_value,
AVDictionary* &avdic) {
    if (IsValidRtsp(channel_value)) {  // check channel value is valid rtsp address
        INFO_LOG("Set parameters for %s", channel_value.c_str());
        avformat_network_init();

        av_dict_set(&avdic, kRtspTransport.c_str(), kUdp.c_str(), kNoFlag);
        av_dict_set(&avdic, kBufferSize.c_str(), kMaxBufferSize.c_str(), kNoFlag);
        av_dict_set(&avdic, kMaxDelayStr.c_str(), kMaxDelayValue.c_str(), kNoFlag);
        av_dict_set(&avdic, kTimeoutStr.c_str(), kTimeoutValue.c_str(), kNoFlag);
        av_dict_set(&avdic, kReorderQueueSize.c_str(),
        kReorderQueueSizeValue.c_str(), kNoFlag);
        av_dict_set(&avdic, kPktSize.c_str(), kPktSizeValue.c_str(), kNoFlag);
    }
}

bool VideoDecode::OpenVideoFromInputChannel(
const string &channel_value, AVFormatContext* &av_format_context) {
    AVDictionary* avdic = nullptr;
    SetDictForRtsp(channel_value, avdic);

    int ret_open_input_video = avformat_open_input(&av_format_context,
    channel_value.c_str(), nullptr,
    &avdic);

    if (ret_open_input_video < kHandleSuccessful) {  // check open video result
        char buf_error[kErrorBufferSize];
        av_strerror(ret_open_input_video, buf_error, kErrorBufferSize);

        ERROR_LOG("Could not open video:%s, "
        "result of avformat_open_input:%d, error info:%s",
        channel_value.c_str(), ret_open_input_video, buf_error);

        if (avdic != nullptr) {  // free AVDictionary
            av_dict_free(&avdic);
        }

        return false;
    }
    av_format_inject_global_side_data(av_format_context);
    avformat_find_stream_info(av_format_context, NULL);

//    this->Init(av_format_context.);
    if (avdic != nullptr) {  // free AVDictionary
        av_dict_free(&avdic);
    }

    return true;
}

bool VideoDecode::InitVideoParams(int videoindex, acldvppStreamFormat &video_type,
AVFormatContext* av_format_context,
AVBSFContext* &bsf_ctx) {
    // check video type, only support h264 and h265
    if (!CheckVideoType(videoindex, av_format_context, video_type)) {
        avformat_close_input(&av_format_context);

        return false;
    }

    const AVBitStreamFilter* video_filter;
    InitVideoStreamFilter(video_type, video_filter);
    if (video_filter == nullptr) {  // check video fileter is nullptr
        ERROR_LOG("Unkonw bitstream filter, video_filter is nullptr!");
        return false;
    }

    // checke alloc bsf context result
    if (av_bsf_alloc(video_filter, &bsf_ctx) < kHandleSuccessful) {
        ERROR_LOG("Fail to call av_bsf_alloc!");
        return false;
    }

    // check copy parameters result
    if (avcodec_parameters_copy(bsf_ctx->par_in,
    av_format_context->streams[videoindex]->codecpar)
    < kHandleSuccessful) {
        ERROR_LOG("Fail to call avcodec_parameters_copy!");
        return false;
    }

    bsf_ctx->time_base_in = av_format_context->streams[videoindex]->time_base;

    // check initialize bsf contextreult
    if (av_bsf_init(bsf_ctx) < kHandleSuccessful) {
        ERROR_LOG("Fail to call av_bsf_init!");
        return false;
    }

    return true;
}

void VideoDecode::UnpackVideo2Image(const string &channel_id) {
    char thread_name_log[kThreadNameLength];
    string thread_name = kThreadNameHead + channel_id;
    prctl(PR_SET_NAME, (unsigned long) thread_name.c_str());
    prctl(PR_GET_NAME, (unsigned long) thread_name_log);
    INFO_LOG("Unpack video to image from:%s, thread name:%s",
    channel_id.c_str(), thread_name_log);

    string channel_value = GetChannelValue(channel_id);
    AVFormatContext* av_format_context = avformat_alloc_context();

    // check open video result
    if (!OpenVideoFromInputChannel(channel_value, av_format_context)) {
        return;
    }

    int videoindex = GetVideoIndex(av_format_context);
    if (videoindex == kInvalidVideoIndex) {  // check video index is valid
        ERROR_LOG("Video index is -1, current media has no video info, channel id:%s",
        channel_id.c_str());
        return;
    }

    AVBSFContext* bsf_ctx;
//    acldvppStreamFormat video_type = H264_HIGH_LEVEL;

    // check initialize video parameters result
    if (!InitVideoParams(videoindex, video_type, av_format_context, bsf_ctx)) {
        return;
    }
    width = av_format_context->streams[videoindex]->codecpar->width;
    height = av_format_context->streams[videoindex]->codecpar->height;
    INFO_LOG("init call back");
    mCallback->CallBackInitFun(width, height, video_type);
    AVPacket av_packet;
    // loop to get every frame from video stream
    while (av_read_frame(av_format_context, &av_packet) == kHandleSuccessful) {
        if (av_packet.stream_index == videoindex) {  // check current stream is video
            // send video packet to ffmpeg
            if (av_bsf_send_packet(bsf_ctx, &av_packet) != kHandleSuccessful) {
                ERROR_LOG("Fail to call av_bsf_send_packet, channel id:%s",
                channel_id.c_str());
            }

            // receive single frame from ffmpeg
            while (av_bsf_receive_packet(bsf_ctx, &av_packet) == kHandleSuccessful) {
//                shared_ptr<VideoImageParaT> video_image_para = make_shared<
//                VideoImageParaT>();
//                video_image_para->video_image_info.channel_id = channel_id;
//                video_image_para->video_image_info.channel_name = channel_value;
//                video_image_para->video_image_info.is_finished = false;
//                video_image_para->video_image_info.video_type = video_type;

                if (av_packet.size <= 0 || av_packet.size > kAllowedMaxImageMemory) {
                    continue;
                }

                width = av_format_context->streams[videoindex]->codecpar->width;
                height = av_format_context->streams[videoindex]->codecpar->height;
                mCallback->CallBackFun(av_packet.data, av_packet.size);
//                uint8_t* vdec_in_buffer = new (nothrow) uint8_t[av_packet.size];
//                int memcpy_result = memcpy(vdec_in_buffer, av_packet.data, av_packet.size);
//                if (memcpy_result != EOK) {  // check memcpy_s result
//                    ERROR_LOG("Fail to copy av_packet data to vdec buffer, memcpy_s result:%d",
//                    memcpy_result);
//                    delete[] vdec_in_buffer;
//                    return;
//                }

//                video_image_para->img.data.reset(vdec_in_buffer,
//                default_delete<uint8_t[]>());
//                video_image_para->img.size = av_packet.size;

                av_packet_unref(&av_packet);

                // send image data to next engine
//                SendImageData(vdec_in_buffer, av_packet.size);
            }
        }
    }

    av_bsf_free(&bsf_ctx);  // free AVBSFContext pointer
    avformat_close_input(&av_format_context);  // close input video

    // send last yuv image data after call vdec
    //SendImageData(video_image_para);

    mCallback->CallBackFun(nullptr, 0);
    INFO_LOG("Ffmpeg read frame finished, channel id:%s",
    channel_id.c_str());
}

bool VideoDecode::VerifyVideoWithUnpack(const string &channel_value) {
    INFO_LOG("Start to verify unpack video file:%s",
    channel_value.c_str());

    AVFormatContext* av_format_context = avformat_alloc_context();
    AVDictionary* avdic = nullptr;
    SetDictForRtsp(channel_value, avdic);

    int ret_open_input_video = avformat_open_input(&av_format_context,
    channel_value.c_str(), nullptr,
    &avdic);
    if (ret_open_input_video < kHandleSuccessful) {  // check open video result
        char buf_error[kErrorBufferSize];
        av_strerror(ret_open_input_video, buf_error, kErrorBufferSize);

        ERROR_LOG("Could not open video:%s, "
        "result of avformat_open_input:%d, error info:%s",
        channel_value.c_str(), ret_open_input_video, buf_error);

        if (avdic != nullptr) {  // free AVDictionary
            av_dict_free(&avdic);
        }

        return false;
    }

    if (avdic != nullptr) {  // free AVDictionary
        av_dict_free(&avdic);
    }

    int video_index = GetVideoIndex(av_format_context);
    if (video_index == kInvalidVideoIndex) {  // check video index is valid
        ERROR_LOG("Video index is -1, current media stream has no video info:%s",
        channel_value.c_str());

        avformat_close_input(&av_format_context);
        return false;
    }

//    acldvppStreamFormat video_type = H264_HIGH_LEVEL;
    bool is_valid = CheckVideoType(video_index, av_format_context, video_type);
    avformat_close_input(&av_format_context);

    return is_valid;
}

bool VideoDecode::VerifyVideoType() {
    // check channel1 and channel2 is not empty
    if (!IsEmpty(channel1_, kStrChannelId1)
    && !IsEmpty(channel2_, kStrChannelId2)) {
        return (VerifyVideoWithUnpack(channel1_) && VerifyVideoWithUnpack(channel2_));
    } else if (!IsEmpty(channel1_, kStrChannelId1)) {  // channel1 is not empty
        return VerifyVideoWithUnpack(channel1_);
    } else {  // channel2 is not empty
        return VerifyVideoWithUnpack(channel2_);
    }
}

void VideoDecode::MultithreadHandleVideo() {
    // create two thread unpacke channel1 and channel2 video in same time
    if (!IsEmpty(channel1_, kStrChannelId1)
    && !IsEmpty(channel2_, kStrChannelId2)) {
        thread thread_channel_1(&VideoDecode::UnpackVideo2Image, this,
        kStrChannelId1);
        thread thread_channel_2(&VideoDecode::UnpackVideo2Image, this,
        kStrChannelId2);

        INFO_LOG("join thread");
        thread_channel_1.join();
        thread_channel_2.join();
    } else if (!IsEmpty(channel1_, kStrChannelId1)) {  // unpacke channel1 video
//        UnpackVideo2Image(kStrChannelId1);
        thread_channel = new thread(&VideoDecode::UnpackVideo2Image, this,
        kStrChannelId1);
    } else {  // unpacke channel2 video
//        UnpackVideo2Image(kStrChannelId2);
        thread_channel = new thread(&VideoDecode::UnpackVideo2Image, this,
        kStrChannelId2);
//        thread_channel->join();
//        thread_channel_2.join();
    }
}

Result VideoDecode::Init(std::string rtsp_path) {
    INFO_LOG("Start process!");

//    uint32_t decodeOutWidthStride = ALIGN_UP16(inputImage.width);
//    uint32_t decodeOutHeightStride = ALIGN_UP2(inputImage.height);
//    if (decodeOutWidthStride == 0 || decodeOutHeightStride == 0) {
//        ERROR_LOG("InitDecodeOutputDesc AlignmentHelper failed");
//        return FAILED;
//    }
//
//    /*预测接口要求aclrtMallocHost分配的内存,AI1上运行当前应用中会需要
//	多次拷贝图片内存,暂不用.
//    acldvppJpegPredictDecSize(inputImage.data.get(), inputImage.size,
//    PIXEL_FORMAT_YUV_SEMIPLANAR_420, &decodeOutBufferSize);*/
//
//    /*分配一块足够大的内存*/
//    uint32_t decodeOutBufferSize =
//    YUV420SP_SIZE(decodeOutWidthStride, decodeOutHeightStride) * 4;
//
//    aclError aclRet = acldvppMalloc(&decodeOutBufferDev_, decodeOutBufferSize);
//    if (aclRet != ACL_ERROR_NONE) {
//        ERROR_LOG("acldvppMalloc decodeOutBufferDev_ failed, aclRet = %d", aclRet);
//        return FAILED;
//    }
//
//    decodeOutputDesc_ = acldvppCreatePicDesc();
//    if (decodeOutputDesc_ == nullptr) {
//        ERROR_LOG("acldvppCreatePicDesc decodeOutputDesc_ failed");
//        return FAILED;
//    }
//
//    acldvppSetPicDescData(decodeOutputDesc_, decodeOutBufferDev_);
//    acldvppSetPicDescFormat(decodeOutputDesc_, PIXEL_FORMAT_YUV_SEMIPLANAR_420);
//    acldvppSetPicDescWidth(decodeOutputDesc_, inputImage.width);
//    acldvppSetPicDescHeight(decodeOutputDesc_, inputImage.height);
//    acldvppSetPicDescWidthStride(decodeOutputDesc_, decodeOutWidthStride);
//    acldvppSetPicDescHeightStride(decodeOutputDesc_, decodeOutHeightStride);
//    acldvppSetPicDescSize(decodeOutputDesc_, decodeOutBufferSize);
//    return SUCCESS;

    // get channel values from configs item
//    for (int index = 0; index < config.items_size(); ++index) {
//        const ::hiai::AIConfigItem &item = config.items(index);
//
//        // get channel1 value
//        if (item.name() == kStrChannelId1) {
//            channel1_ = item.value();
//            continue;
//        }
//
//        // get channel2 value
//        if (item.name() == kStrChannelId2) {
//            channel2_ = item.value();
//            continue;
//        }
//    }

    channel2_ = rtsp_path;
    // verify channel values are valid
    if (!VerifyChannelValues()) {
        return FAILED;
    }

    INFO_LOG("End process!");
    return SUCCESS;
}

bool VideoDecode::IsEmpty(const string &input_str, const string &channel_id) {
    regex regex_space(kRegexSpace.c_str());

    // check input string is empty or spaces
    if (regex_match(input_str, regex_space)) {
        INFO_LOG("The channel string is empty or all spaces, channel id:%s",
        channel_id.c_str());
        return true;
    }

    return false;
}

bool VideoDecode::VerifyVideoSourceName(const string &input_str) {
    // verify input string is valid mp4 file name or rtsp address
    if (!IsValidRtsp(input_str) && !IsValidMp4File(input_str)) {
        ERROR_LOG("Invalid mp4 file name or RTSP name:%s", input_str.c_str());
        return false;
    }

    return true;
}

bool VideoDecode::IsValidRtsp(const string &input_str) {
    regex regex_rtsp_address(kRegexRtsp.c_str());

    // verify input string is valid rtsp address
    if (regex_match(input_str, regex_rtsp_address)) {
        return true;
    }

    return false;
}

bool VideoDecode::IsValidMp4File(const string &input_str) {
    regex regex_mp4_file_name(kRegexMp4File.c_str());

    // verify input string is valid mp4 file name
    if (regex_match(input_str, regex_mp4_file_name)) {
        return true;
    }

    return false;
}

bool VideoDecode::VerifyChannelValues() {
    // check channel1 and channel2 are empty
    if (IsEmpty(channel1_, kStrChannelId1)
    && IsEmpty(channel2_, kStrChannelId2)) {
        ERROR_LOG("Both channel1 and channel2 are empty!");
        return false;
    }

    // verify channel1 value when channel1 is not empty
    if (!IsEmpty(channel1_, kStrChannelId1)) {
        // deletes the space at the head of the string
        channel1_.erase(0, channel1_.find_first_not_of(kNeedRemoveStr.c_str()));

        // deletes spaces at the end of the string
        channel1_.erase(channel1_.find_last_not_of(kNeedRemoveStr.c_str()) + 1);

        INFO_LOG("Display channel1:%s", channel1_.c_str());

        if (!VerifyVideoSourceName(channel1_)) {  // verify channel1
            return false;
        }
    }

    // verify channel2 value when channel1 is not empty
    if (!IsEmpty(channel2_, kStrChannelId2)) {
        // deletes the space at the head of the string
        channel2_.erase(0, channel2_.find_first_not_of(kNeedRemoveStr.c_str()));

        // deletes spaces at the end of the string
        channel2_.erase(channel2_.find_last_not_of(kNeedRemoveStr.c_str()) + 1);

        INFO_LOG("Display channel2:%s", channel2_.c_str());

        if (!VerifyVideoSourceName(channel2_)) {  // verify channel2
            return false;
        }
    }

    return true;
}

#if 0
void VideoDecode::SendImageData(uint8_t* inBufferDev, size_t inBufferSize/*shared_ptr<VideoImageParaT> &video_image_data*/) {
    Result ret = SUCCESS;

    //inBufferDev_表示Device存放输入视频数据的内存，inBufferSize_表示内存大小
    ret = acldvppSetStreamDescData(stream_, inBufferDev);
    ret = acldvppSetStreamDescSize(stream_, inBufferSize);

    //申请Device内存picOutBufferDev_，用于存放VDEC解码后的输出数据
//    ret = acldvppMalloc(&decodeOutputDesc_, DataSize);

    //创建输出图片描述信息，设置图片描述信息的属性
    //picOutputDesc_是acldvppPicDesc类型
//    picOutputDesc_ = acldvppCreatePicDesc();
//    ret = acldvppSetPicDescData(picOutputDesc_, picOutBufferDev_);
//    ret = acldvppSetPicDescSize(picOutputDesc_, DataSize);
//    ret = acldvppSetPicDescFormat(picOutputDesc_, static_cast<acldvppPixelFormat>(format_));

    // 执行视频码流解码，解码每帧数据后，系统自动调用callback回调函数将解码后的数据写入文件，再及时释放相关资源
    ret = aclvdecSendFrame(dvppChannelDesc_, stream_, decodeOutputDesc_, nullptr, nullptr);
//    if (video_image_data == nullptr) {  // the queue is empty and return
//        return;
//    }
//
//    // send image data
//    do {
//        hiai_ret = SendData(0, kVideoImageParaType,
//        static_pointer_cast<void>(video_image_data));
//        if (hiai_ret == HIAI_QUEUE_FULL) {  // check queue is full
//            INFO_LOG("The queue is full when send image data, sleep 10ms");
//            usleep(kWait10Milliseconds);  // sleep 10 ms
//        }
//    } while (hiai_ret == HIAI_QUEUE_FULL);  // loop while queue is full

    if (ret != SUCCESS) {  // check send data is failed
        ERROR_LOG("Send data failed! error code: %d", hiai_ret);
    }
}
#endif

Result VideoDecode::read() {
    av_log_set_level(AV_LOG_INFO);  // set ffmpeg log level

    // verify video type
    if (!VerifyVideoType()) {
        SendFinishedData();  // send the flag data when finished
        return FAILED;
    }

    MultithreadHandleVideo();  // handle video from file or RTSP with multi-thread

    INFO_LOG("multi thread read video end");
    SendFinishedData();// send the flag data when finished

return SUCCESS;
}

void VideoDecode::wait()
{
    thread_channel->join();
}

