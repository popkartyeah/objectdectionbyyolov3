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

* File main.cpp
* Description: dvpp sample main func
*/

#include <iostream>
#include <stdlib.h>
#include <dirent.h>

#include "object_detect.h"
#include "utils.h"
using namespace std;

namespace {
    uint32_t kModelWidth = 416;
    uint32_t kModelHeight = 416;
    const char* kModelPath = "../model/yolov3_nv21.om";
}

static ObjectDetect* ptrDetect;


FILE *outFileFp_;
static bool WriteToFile(FILE *outFileFp_, const void *dataDev, uint32_t dataSize)
{

    bool ret = true;
    size_t writeRet = fwrite(dataDev, 1, dataSize, outFileFp_);
    if (writeRet != dataSize) {
        ret = false;
    }
    fflush(outFileFp_);

    return ret;
}
//3.创建回调函数
static void callback(acldvppStreamDesc *input, acldvppPicDesc *output, void *userdata)
{
    //获取VDEC解码的输出内存，调用自定义函数WriteToFile将输出内存中的数据写入文件后，再调用acldvppFree接口释放输出内存
    void *vdecOutBufferDev = acldvppGetPicDescData(output);
    uint32_t size = acldvppGetPicDescSize(output);
    static int count = 1;
//    INFO_LOG("decode frame size %d", size);
//    INFO_LOG("acldvppGetStreamDescEos(input) : %d", acldvppGetStreamDescEos(input));
//    if(acldvppGetStreamDescEos(input) == 1 && output == nullptr)
//    {
//        ptrDetect->SendVencFrameEnd();
//        INFO_LOG("decode frame finished");
//    }

    ImageData resizedImage, image;
    image.width = acldvppGetPicDescWidth(output);
    image.height = acldvppGetPicDescHeight(output);
    image.alignWidth = ALIGN_UP16(image.width);
    image.alignHeight = ALIGN_UP2(image.height);
    image.size = size;
    image.data = SHARED_PRT_DVPP_BUF(vdecOutBufferDev);
    Result ret = ptrDetect->Preprocess_Video(resizedImage, image);
    if (ret != SUCCESS) {
        ERROR_LOG("Preprocess_Video failed");
        //        imageFile.c_str());
        //        continue;
    }
//    Utils::WriteImageFile(resizedImage, "/home/HwHiAiUser/test_infference.nv12");
//    INFO_LOG("resize image w:%d,h:%d,size %d", resizedImage.width, resizedImage.height, resizedImage.size);
    //将预处理的图片送入模型推理,并获取推理结果
    aclmdlDataset* inferenceOutput = nullptr;
    ret = ptrDetect->Inference(inferenceOutput, resizedImage);
    if ((ret != SUCCESS) || (inferenceOutput == nullptr)) {
        ERROR_LOG("Inference model inference output data failed");
        return;
    }
    //解析推理输出,并将推理得到的物体类别和位置标记到图片上
    ret = ptrDetect->Postprocess_Video(image, inferenceOutput);
    if (ret != SUCCESS) {
        ERROR_LOG("Process model inference output data failed");
        return;
    }

    ptrDetect->SendVencFrame(output);
//    ret = acldvppFree(reinterpret_cast<void *>(vdecOutBufferDev));
//     释放acldvppPicDesc类型的数据，表示解码后输出图片描述数据
//        aclret = acldvppDestroyPicDesc(input);
//    INFO_LOG("decode frame num %d", count);
    //......
    count++;
}


static void save_callback(acldvppPicDesc *input, acldvppStreamDesc *output, void *userdata)
{
    //获取VENC编码的输出内存，调用自定义函数WriteToFile将输出内存中的数据写入文件
    void *vdecOutBufferDev = acldvppGetStreamDescData(output);
    uint32_t size = acldvppGetStreamDescSize(output);

    // void *vdecInBufferDev = acldvppGetPicDescData(input);
    // aclError aclret = acldvppFree(reinterpret_cast<void *>(vdecInBufferDev));
    //    aclret = acldvppFree(reinterpret_cast<void *>(resizedImage.data.get()));
    // 释放acldvppPicDesc类型的数据，表示解码后输出图片描述数据
    aclError aclret = acldvppDestroyPicDesc(input);

//    INFO_LOG("Write a picture size %d ", size);
    if (!WriteToFile(outFileFp_, vdecOutBufferDev, size)) {
        ERROR_LOG("write file failed.");
    }


    //    INFO_LOG("decode frame num %d", count);
    //......
//    count++;
}

int main(int argc, char *argv[]) {
    //检查应用程序执行时的输入,程序执行要求输入图片目录参数
    //    if((argc < 2) || (argv[1] == nullptr)){
    //        ERROR_LOG("Please input: ./main <image_dir>");
    //        return FAILED;
    //    }
    //实例化目标检测对象,参数为分类模型路径,模型输入要求的宽和高
    //    ObjectDetect detect(kModelPath, kModelWidth, kModelHeight);
    ptrDetect = new ObjectDetect(kModelPath, kModelWidth, kModelHeight);
    //初始化分类推理的acl资源, 模型和内存
    //    Result ret = detect.Init();
//    string rtsp_url = "rtsp://192.168.5.121/0";
    string rtsp_url = "/home/HwHiAiUser/ncs_720.mp4";
    string fileName = "/home/HwHiAiUser/test.h264";
    outFileFp_ = fopen(fileName.c_str(), "wb");
    if(outFileFp_ == nullptr)
    {
        ERROR_LOG("Failed to open  file %s.", fileName.c_str());
        return FAILED;
    }
    Result ret = ptrDetect->Init(rtsp_url, callback, save_callback);
    if (ret != SUCCESS) {
        ERROR_LOG("Classification Init resource failed");
        return FAILED;
    }

    ptrDetect->Wait();
    //    getchar();
    //    getchar();
    //获取图片目录下所有的图片文件名
    //    string inputImageDir = "../data/boat.jpg";//string(argv[1]);
    //
    //    vector<string> fileVec;
    //    Utils::GetAllFiles(inputImageDir, fileVec);
    //    if (fileVec.empty()) {
    //        ERROR_LOG("Failed to deal all empty path=%s.", inputImageDir.c_str());
    //        return FAILED;
    //    }
    //
    //    //逐张图片推理
    //    ImageData image;
    //    for (string imageFile : fileVec) {
    //        Utils::ReadImageFile(image, imageFile);
    //        if (image.data == nullptr) {
    //            ERROR_LOG("Read image %s failed", imageFile.c_str());
    //            return FAILED;
    //        }
    //
    //        //预处理图片:读取图片,讲图片缩放到模型输入要求的尺寸
    //        ImageData resizedImage;
    //        Result ret = detect.Preprocess(resizedImage, image);
    //        if (ret != SUCCESS) {
    //            ERROR_LOG("Read file %s failed, continue to read next",
    //                      imageFile.c_str());
    //            continue;
    //        }
    //        //将预处理的图片送入模型推理,并获取推理结果
    //        aclmdlDataset* inferenceOutput = nullptr;
    //        ret = detect.Inference(inferenceOutput, resizedImage);
    //        if ((ret != SUCCESS) || (inferenceOutput == nullptr)) {
    //            ERROR_LOG("Inference model inference output data failed");
    //            return FAILED;
    //        }
    //        //解析推理输出,并将推理得到的物体类别和位置标记到图片上
    //        ret = detect.Postprocess(image, inferenceOutput, imageFile);
    //        if (ret != SUCCESS) {
    //            ERROR_LOG("Process model inference output data failed");
    //            return FAILED;
    //        }
    //    }

    INFO_LOG("Execute sample success");
    getchar();
    fclose(outFileFp_);
    return SUCCESS;
}
