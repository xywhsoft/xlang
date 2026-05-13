#include "../include/opencv_xlang_c_api.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <exception>
#include <sstream>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#define XLCV_EXPORT __declspec(dllexport)
#else
#define XLCV_EXPORT
#endif

struct XlCvImageImpl {
	cv::Mat mat;
};

static thread_local std::string g_error;

static void xlcv_set_error(const char* text)
{
	g_error = text != nullptr ? text : "";
}

static XlCvImage xlcv_wrap(cv::Mat&& mat)
{
	if (mat.empty()) {
		xlcv_set_error("OpenCV returned an empty image");
		return nullptr;
	}
	XlCvImageImpl* image = new XlCvImageImpl();
	image->mat = std::move(mat);
	xlcv_set_error("");
	return image;
}

static double xlcv_channel_value(int channel, double value0, double value1, double value2)
{
	if (channel == 1) return value1;
	if (channel >= 2) return value2;
	return value0;
}

static cv::Mat xlcv_normalized_mat(const cv::Mat& input, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	cv::Mat out;
	input.convertTo(out, CV_MAKETYPE(CV_32F, input.channels()), scale);
	const int channels = out.channels();
	for (int y = 0; y < out.rows; ++y) {
		float* row = out.ptr<float>(y);
		for (int x = 0; x < out.cols; ++x) {
			for (int c = 0; c < channels; ++c) {
				const double mean = xlcv_channel_value(c, mean0, mean1, mean2);
				double stddev = xlcv_channel_value(c, std0, std1, std2);
				if (stddev == 0.0) stddev = 1.0;
				const int offset = x * channels + c;
				row[offset] = static_cast<float>((row[offset] - mean) / stddev);
			}
		}
	}
	return out;
}

extern "C" {

XLCV_EXPORT int opencv_xl_available(void)
{
	return 1;
}

XLCV_EXPORT const char* opencv_xl_version(void)
{
	return CV_VERSION;
}

XLCV_EXPORT const char* opencv_xl_last_error(void)
{
	return g_error.c_str();
}

XLCV_EXPORT XlCvImage opencv_xl_imread(const char* path, int flags)
{
	try {
		if (path == nullptr || path[0] == '\0') {
			xlcv_set_error("empty image path");
			return nullptr;
		}
		return xlcv_wrap(cv::imread(path, flags));
	} catch (const std::exception& e) {
		xlcv_set_error(e.what());
		return nullptr;
	}
}

XLCV_EXPORT int opencv_xl_imwrite(const char* path, XlCvImage image)
{
	try {
		XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
		if (path == nullptr || path[0] == '\0' || img == nullptr || img->mat.empty()) {
			xlcv_set_error("invalid image or output path");
			return 0;
		}
		bool ok = cv::imwrite(path, img->mat);
		xlcv_set_error(ok ? "" : "cv::imwrite returned false");
		return ok ? 1 : 0;
	} catch (const std::exception& e) {
		xlcv_set_error(e.what());
		return 0;
	}
}

XLCV_EXPORT XlCvImage opencv_xl_resize(XlCvImage image, int width, int height)
{
	try {
		XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
		if (img == nullptr || img->mat.empty() || width <= 0 || height <= 0) {
			xlcv_set_error("invalid image or resize dimensions");
			return nullptr;
		}
		cv::Mat out;
		cv::resize(img->mat, out, cv::Size(width, height));
		return xlcv_wrap(std::move(out));
	} catch (const std::exception& e) {
		xlcv_set_error(e.what());
		return nullptr;
	}
}

XLCV_EXPORT XlCvImage opencv_xl_cvt_color(XlCvImage image, int code)
{
	try {
		XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
		if (img == nullptr || img->mat.empty()) {
			xlcv_set_error("invalid image");
			return nullptr;
		}
		cv::Mat out;
		cv::cvtColor(img->mat, out, code);
		return xlcv_wrap(std::move(out));
	} catch (const std::exception& e) {
		xlcv_set_error(e.what());
		return nullptr;
	}
}

XLCV_EXPORT XlCvImage opencv_xl_copy_make_border(XlCvImage image, int top, int bottom, int left, int right, int border_type, double value0, double value1, double value2, double value3)
{
	try {
		XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
		if (img == nullptr || img->mat.empty()) {
			xlcv_set_error("invalid image");
			return nullptr;
		}
		cv::Mat out;
		cv::copyMakeBorder(img->mat, out, top, bottom, left, right, border_type, cv::Scalar(value0, value1, value2, value3));
		return xlcv_wrap(std::move(out));
	} catch (const std::exception& e) {
		xlcv_set_error(e.what());
		return nullptr;
	}
}

XLCV_EXPORT XlCvImage opencv_xl_normalize(XlCvImage image, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	try {
		XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
		if (img == nullptr || img->mat.empty()) {
			xlcv_set_error("invalid image");
			return nullptr;
		}
		return xlcv_wrap(xlcv_normalized_mat(img->mat, scale, mean0, mean1, mean2, std0, std1, std2));
	} catch (const std::exception& e) {
		xlcv_set_error(e.what());
		return nullptr;
	}
}

XLCV_EXPORT const char* opencv_xl_to_chw_tensor_json(XlCvImage image, double scale, double mean0, double mean1, double mean2, double std0, double std1, double std2)
{
	static thread_local std::string json;
	try {
		XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
		if (img == nullptr || img->mat.empty()) {
			xlcv_set_error("invalid image");
			json.clear();
			return json.c_str();
		}
		cv::Mat out = xlcv_normalized_mat(img->mat, scale, mean0, mean1, mean2, std0, std1, std2);
		const int channels = out.channels();
		std::ostringstream ss;
		ss << "{\"dtype\":\"float32\",\"shape\":[1," << channels << "," << out.rows << "," << out.cols << "],\"data\":[";
		bool first = true;
		for (int c = 0; c < channels; ++c) {
			for (int y = 0; y < out.rows; ++y) {
				const float* row = out.ptr<float>(y);
				for (int x = 0; x < out.cols; ++x) {
					if (!first) ss << ",";
					first = false;
					ss << row[x * channels + c];
				}
			}
		}
		ss << "]}";
		json = ss.str();
		xlcv_set_error("");
		return json.c_str();
	} catch (const std::exception& e) {
		xlcv_set_error(e.what());
		json.clear();
		return json.c_str();
	}
}

XLCV_EXPORT int opencv_xl_width(XlCvImage image)
{
	XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
	return img != nullptr ? img->mat.cols : 0;
}

XLCV_EXPORT int opencv_xl_height(XlCvImage image)
{
	XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
	return img != nullptr ? img->mat.rows : 0;
}

XLCV_EXPORT int opencv_xl_channels(XlCvImage image)
{
	XlCvImageImpl* img = static_cast<XlCvImageImpl*>(image);
	return img != nullptr ? img->mat.channels() : 0;
}

XLCV_EXPORT void opencv_xl_release(XlCvImage image)
{
	delete static_cast<XlCvImageImpl*>(image);
}

}
