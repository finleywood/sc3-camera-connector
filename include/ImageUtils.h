#pragma once

#include <aixlog/aixlog.hpp>
#include <k4a/k4a.h>
#include <opencv2/opencv.hpp>

#include "EncodedImage.h"

namespace ImageUtils {

cv::Mat K4ARGBToCV(k4a_image_t* img);
EncodedImage K4AToJpeg(k4a_image_t* image, int quality = 90);
EncodedImage K4AToPng(k4a_image_t* image);
EncodedImage JpegToThumbnail(const EncodedImage& orig_img, int target_width = 640, int out_quality = 90);

}