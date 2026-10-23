#ifndef XL6_CODEC_ABI_H
#define XL6_CODEC_ABI_H
/* Included after the common resident ABI types. All results own their bytes. */
XL6_TEXT_API xstrview xl6CodecHexTextV1(xstrview,int64_t,int64_t);
XL6_TEXT_API xstrview xl6CodecHexBytesV1(const xvalue*,int64_t,int64_t);
XL6_TEXT_API xstrview xl6CodecHexBytesSpanV1(xbytesview,int64_t,int64_t);
XL6_TEXT_API xvalue* xl6CodecHexDecodeV1(xstrview,int64_t,int64_t);
XL6_TEXT_API xstrview xl6CodecBase64TextV1(xstrview,int64_t,int64_t,xstrview,int64_t);
XL6_TEXT_API xstrview xl6CodecBase64BytesV1(const xvalue*,int64_t,int64_t,xstrview,int64_t);
XL6_TEXT_API xstrview xl6CodecBase64BytesSpanV1(xbytesview,int64_t,int64_t,xstrview,int64_t);
XL6_TEXT_API xvalue* xl6CodecBase64DecodeV1(xstrview,int64_t,int64_t,xstrview,int64_t);
XL6_TEXT_API xstrview xl6CodecPercentTextV1(xstrview,int64_t,xstrview,bool,bool,int64_t);
XL6_TEXT_API xstrview xl6CodecPercentBytesV1(const xvalue*,int64_t,xstrview,bool,bool,int64_t);
XL6_TEXT_API xstrview xl6CodecPercentBytesSpanV1(xbytesview,int64_t,xstrview,bool,bool,int64_t);
XL6_TEXT_API xvalue* xl6CodecPercentDecodeV1(xstrview,int64_t,bool);
#endif
