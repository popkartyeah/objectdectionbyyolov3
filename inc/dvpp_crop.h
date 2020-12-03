//
// Created by media on 20-11-9.
//

#ifndef objectdetection_dvpp_crop_H
#define objectdetection_dvpp_crop_H
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

#define ODD_NUM 1
#define EVEN_NUM 2
#define BORDER 4
#define MAX_CROP_NUM 256
class DvppCrop {
    public:
    /**
    * @brief Constructor
    * @param [in] stream: stream
    */
    DvppCrop(aclrtStream &stream, acldvppChannelDesc *dvppChannelDesc,
    vector<Rect>& rects);

    /**
    * @brief Destructor
    */
    ~DvppCrop();

    /**
    * @brief dvpp global init
    * @return result
    */
    Result InitResource();

    /**
    * @brief init dvpp output para
    * @param [in] modelInputWidth: model input width
    * @param [in] modelInputHeight: model input height
    * @return result
    */
    Result InitOutputPara(int modelInputWidth, int modelInputHeight);


    /**
    * @brief gett dvpp output
    * @param [in] outputBuffer: pointer which points to dvpp output buffer
    * @param [out] outputSize: output size
    */
    void GetOutput(void **outputBuffer, int &outputSize);

    /**
    * @brief dvpp process
    * @return result
    */
//    Result Process(ImageData& cropedImage, ImageData& srcImage);
    Result DrawRect(ImageData& srcImage, std::vector<Rect> rects);

    private:
    Result InitCropResource(ImageData& inputImage);
    Result InitCropInputDesc(ImageData& inputImage);
    Result InitCropOutputDesc();

    void DestroyCropResource();
    void DestroyResource();
    void DestroyOutputPara();


    aclrtStream stream_;
    acldvppChannelDesc *dvppChannelDesc_;

//    acldvppCropConfig *resizeConfig_;

    acldvppPicDesc *vpcInputDesc_; // vpc input desc

    acldvppPicDesc *vpcOutputDesc_; // vpc output desc

    uint8_t *inDevBuffer_;  // input pic dev buffer
    void *vpcOutBufferDev_; // vpc output buffer
    uint32_t vpcOutBufferSize_;  // vpc output size
    Resolution size_;
    acldvppPixelFormat format_;

    acldvppBatchPicDesc *vpcInputBatchDesc_;
    acldvppBatchPicDesc *vpctmpBatchDesc_;
    acldvppBatchPicDesc *vpcOutputBatchDesc_;
    vector<Rect> rects;
    void *vpcTmpBufferDev_[MAX_CROP_NUM];

    std::vector<acldvppRoiConfig*> cropAreas;
    std::vector<acldvppRoiConfig*> pasteAreas;

    std::vector<acldvppRoiConfig*> wholeAreas;
    std::vector<acldvppRoiConfig*> replaceAreas;
};


#endif //objectdetection_dvpp_crop_H
