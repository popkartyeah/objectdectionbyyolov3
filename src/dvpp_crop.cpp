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
#include "utils.h"
#include "dvpp_crop.h"
using namespace std;



DvppCrop::DvppCrop(aclrtStream& stream, acldvppChannelDesc *dvppChannelDesc,
vector<Rect>& rects)
: stream_(stream), dvppChannelDesc_(dvppChannelDesc), vpcInputDesc_(nullptr),
vpcOutputDesc_(nullptr), inDevBuffer_(nullptr),vpcOutBufferDev_(nullptr),
vpcOutBufferSize_(0), vpcInputBatchDesc_(nullptr), vpctmpBatchDesc_(nullptr){
    for(int i = 0; i < rects.size(); i++)
    {
        this->rects.push_back(rects[i]);
    }
}

DvppCrop::~DvppCrop()
{
    DestroyCropResource();
}

Result DvppCrop::InitCropInputDesc(ImageData& inputImage)
{
    uint32_t alignWidth = ALIGN_UP16(inputImage.width);
    uint32_t alignHeight = ALIGN_UP2(inputImage.height);
    if (alignWidth == 0 || alignHeight == 0) {
        ERROR_LOG("InitCropInputDesc AlignmentHelper failed. image w %d, h %d, align w%d, h%d",
        inputImage.width, inputImage.height, alignWidth, alignHeight);
        return FAILED;
    }
    uint32_t inputBufferSize = YUV420SP_SIZE(alignWidth, alignHeight);

    vpcInputBatchDesc_ = acldvppCreateBatchPicDesc(1);
    vpcInputDesc_ = acldvppGetPicDesc(vpcInputBatchDesc_, 0);
    if (vpcInputDesc_ == nullptr) {
        ERROR_LOG("acldvppGetPicDesc vpcInputDesc_ failed");
        return FAILED;
    }
    acldvppSetPicDescData(vpcInputDesc_, inputImage.data.get()); //  JpegD . vpcCrop
    acldvppSetPicDescFormat(vpcInputDesc_, format_);
    acldvppSetPicDescWidth(vpcInputDesc_, inputImage.width);
    acldvppSetPicDescHeight(vpcInputDesc_, inputImage.height);
    acldvppSetPicDescWidthStride(vpcInputDesc_, alignWidth);
    acldvppSetPicDescHeightStride(vpcInputDesc_, alignHeight);
    acldvppSetPicDescSize(vpcInputDesc_, inputBufferSize);

    vpctmpBatchDesc_ = acldvppCreateBatchPicDesc(rects.size());
    if (vpctmpBatchDesc_ == nullptr) {
        ERROR_LOG("acldvppCreateBatchPicDesc vpctmpBatchDesc_ failed");
        return FAILED;
    }

    vpcOutputBatchDesc_ = acldvppCreateBatchPicDesc(rects.size());
    if (vpcOutputBatchDesc_ == nullptr) {
        ERROR_LOG("acldvppCreateBatchPicDesc vpcOutputBatchDesc_ failed");
        return FAILED;
    }
    for (int i = 0;i < rects.size(); i++)
    {
        //create background
        uint32_t alignWidth1 = ALIGN_UP16(rects[i].rbX - ALIGN_DOWN16(rects[i].ltX) + BORDER + BORDER) + 32;
        uint32_t alignHeight1 = ALIGN_UP2(rects[i].rbY - rects[i].ltY + BORDER + BORDER)+2;
        uint32_t inputBufferSize1 = YUV420SP_SIZE(alignWidth1, alignHeight1);

        acldvppMalloc(&(vpcTmpBufferDev_[i]), inputBufferSize1);
        //set rect color
        //Y
        /*aclrtMemsetAsync(vpcTmpBufferDev_[i], 1, 0xf, inputBufferSize1, stream_);
        //uv
//        aclrtMemsetAsync(vpcTmpBufferDev_[i] + alignWidth * alignHeight, 1, 0x0, alignWidth * alignHeight / 2, stream_);
        auto aclRet = aclrtSynchronizeStream(stream_);
        if (aclRet != ACL_ERROR_NONE) {
            ERROR_LOG("Crop aclrtSynchronizeStream failed, aclRet = %d", aclRet);
            return FAILED;
        }*/
        vpcInputDesc_ = acldvppGetPicDesc(vpctmpBatchDesc_, i);
        if (vpcInputDesc_ == nullptr) {
            ERROR_LOG("acldvppGetPicDesc vpcInputDesc_ %d failed", i);
            return FAILED;
        }
        acldvppSetPicDescData(vpcInputDesc_, vpcTmpBufferDev_[i]); //  JpegD . vpcCrop
        acldvppSetPicDescFormat(vpcInputDesc_, format_);
        acldvppSetPicDescWidth(vpcInputDesc_, alignWidth1);
        acldvppSetPicDescHeight(vpcInputDesc_, alignHeight1);
        acldvppSetPicDescWidthStride(vpcInputDesc_, alignWidth1);
        acldvppSetPicDescHeightStride(vpcInputDesc_, alignHeight1);
        acldvppSetPicDescSize(vpcInputDesc_, inputBufferSize1);


        vpcOutputDesc_ = acldvppGetPicDesc(vpcOutputBatchDesc_, i);
        if (vpcOutputDesc_ == nullptr) {
            ERROR_LOG("acldvppGetPicDesc vpcOutputDesc_ %d failed", i);
            return FAILED;
        }
        acldvppSetPicDescData(vpcOutputDesc_, inputImage.data.get()); //  JpegD . vpcCrop
        acldvppSetPicDescFormat(vpcOutputDesc_, format_);
        acldvppSetPicDescWidth(vpcOutputDesc_, inputImage.width);
        acldvppSetPicDescHeight(vpcOutputDesc_, inputImage.height);
        acldvppSetPicDescWidthStride(vpcOutputDesc_, alignWidth);
        acldvppSetPicDescHeightStride(vpcOutputDesc_, alignHeight);
        acldvppSetPicDescSize(vpcOutputDesc_, inputBufferSize);
    }
    return SUCCESS;
}

Result DvppCrop::InitCropOutputDesc()
{

    return SUCCESS;
}

Result DvppCrop::InitCropResource(ImageData& inputImage) {
    format_ = static_cast<acldvppPixelFormat>(PIXEL_FORMAT_YVU_SEMIPLANAR_420);

    if (SUCCESS != InitCropInputDesc(inputImage)) {
        ERROR_LOG("InitCropInputDesc failed");
        return FAILED;
    }

    if (SUCCESS != InitCropOutputDesc()) {
        ERROR_LOG("InitCropOutputDesc failed");
        return FAILED;
    }

    return SUCCESS;
}


Result DvppCrop::DrawRect(ImageData& srcImage, std::vector<Rect> rects)
{
    if (SUCCESS != InitCropResource(srcImage)) {
        ERROR_LOG("Dvpp Crop failed for init error");
        return FAILED;
    }

//    ImageData yuvImage;
    //set batch rect area
    vector<uint32_t>nRoi1;
    for (int i = 0; i < rects.size(); i++)
    {
        uint32_t ltX = ALIGN_DOWN16(rects[i].ltX) + BORDER;
        uint32_t ltY = ALIGN_UP2(rects[i].ltY) + BORDER;
        uint32_t rbX = ALIGN_UP16(rects[i].rbX) - BORDER - 1;
        uint32_t rbY = ALIGN_UP2(rects[i].rbY) - BORDER - 1;
        INFO_LOG("rect ltx:%d,lty:%d,rbx:%d,rby:%d", ltX, ltY,rbX, rbY);
        acldvppRoiConfig* cropArea = acldvppCreateRoiConfig(ltX, rbX, ltY, rbY);
        cropAreas.push_back(cropArea);
        acldvppRoiConfig* pasteArea = acldvppCreateRoiConfig(16, rbX - ltX + 16 , BORDER,  rbY - ltY + BORDER);
        pasteAreas.push_back(pasteArea);
        acldvppRoiConfig* wholeArea = acldvppCreateRoiConfig(16 - BORDER, rbX - ltX + BORDER + 16, 0, rbY - ltY + BORDER + BORDER);
        wholeAreas.push_back(wholeArea);
        acldvppRoiConfig* replaceArea = acldvppCreateRoiConfig(ltX - BORDER, rbX + BORDER, ltY - BORDER, rbY + BORDER);
        replaceAreas.push_back(replaceArea);
        nRoi1.push_back(1);
    }



    // Crop pic
    uint32_t nRoi = rects.size();
    aclError aclRet = acldvppVpcBatchCropAndPasteAsync(dvppChannelDesc_, vpcInputBatchDesc_, &nRoi, 1,
    vpctmpBatchDesc_, &cropAreas[0], &pasteAreas[0], stream_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppVpcBatchCropAndPasteAsync 1 failed, aclRet = %d", aclRet);
        return FAILED;
    }
    aclRet = aclrtSynchronizeStream(stream_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("Crop aclrtSynchronizeStream failed, aclRet = %d", aclRet);
        return FAILED;
    }

    nRoi = 1;
    aclRet = acldvppVpcBatchCropAndPasteAsync(dvppChannelDesc_, vpctmpBatchDesc_, &nRoi1[0],rects.size(), vpcOutputBatchDesc_,
    &wholeAreas[0], &replaceAreas[0], stream_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("acldvppVpcBatchCropAndPasteAsync 2 failed, aclRet = %d", aclRet);
        return FAILED;
    }
    aclRet = aclrtSynchronizeStream(stream_);
    if (aclRet != ACL_ERROR_NONE) {
        ERROR_LOG("Crop aclrtSynchronizeStream failed, aclRet = %d", aclRet);
        return FAILED;
    }

    DestroyCropResource();

    return SUCCESS;
}

void DvppCrop::DestroyCropResource()
{
    for (int i = 0;i < rects.size(); i++)
    {
        if (cropAreas[i] != nullptr) {
            (void)acldvppDestroyRoiConfig(cropAreas[i]);
            cropAreas[i] = nullptr;
        }
        if (pasteAreas[i] != nullptr) {
            (void)acldvppDestroyRoiConfig(pasteAreas[i]);
            pasteAreas[i] = nullptr;
        }

        if (wholeAreas[i] != nullptr) {
            (void)acldvppDestroyRoiConfig(wholeAreas[i]);
            wholeAreas[i] = nullptr;
        }
        if (replaceAreas[i] != nullptr) {
            (void)acldvppDestroyRoiConfig(replaceAreas[i]);
            replaceAreas[i] = nullptr;
        }

        if (vpcTmpBufferDev_[i] != nullptr) {
            acldvppFree(vpcTmpBufferDev_[i]);
            vpcTmpBufferDev_[i] = nullptr;
        }
    }
    if (vpctmpBatchDesc_ != nullptr) {
        (void)acldvppDestroyBatchPicDesc(vpctmpBatchDesc_);
        vpctmpBatchDesc_ = nullptr;
    }
    if (vpcOutputBatchDesc_ != nullptr) {
        (void)acldvppDestroyBatchPicDesc(vpcOutputBatchDesc_);
        vpcOutputBatchDesc_ = nullptr;
    }
    if (vpcInputBatchDesc_ != nullptr) {
        (void)acldvppDestroyBatchPicDesc(vpcInputBatchDesc_);
        vpcInputBatchDesc_ = nullptr;
    }

}
