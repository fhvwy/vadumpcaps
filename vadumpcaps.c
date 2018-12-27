/*
 * vadumpcaps - Show all VAAPI capabilities.
 * Copyright (C) 2016-2018 Mark Thompson <sw@jkqxz.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include <va/va.h>

#if !VA_CHECK_VERSION(0, 34, 0)
#warning "This program will not work with libva versions below 1.2.0"
#endif

#include <va/va_vpp.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#define LIBVA_1_3_0 VA_CHECK_VERSION(0, 35, 0)
#define LIBVA_1_3_1 VA_CHECK_VERSION(0, 35, 1)
#define LIBVA_1_4_0 VA_CHECK_VERSION(0, 36, 0)
#define LIBVA_1_5_0 VA_CHECK_VERSION(0, 37, 0)
#define LIBVA_1_6_0 VA_CHECK_VERSION(0, 38, 0)
#define LIBVA_1_6_2 VA_CHECK_VERSION(0, 38, 1)
#define LIBVA_1_7_0 VA_CHECK_VERSION(0, 39, 0)
#define LIBVA_1_7_1 VA_CHECK_VERSION(0, 39, 2)
#define LIBVA_1_7_2 VA_CHECK_VERSION(0, 39, 3)
#define LIBVA_1_7_3 VA_CHECK_VERSION(0, 39, 4)
#define LIBVA_1_8_0 VA_CHECK_VERSION(0, 40, 0)
#define LIBVA_2_0_0 VA_CHECK_VERSION(1,  0, 0)
#define LIBVA_2_1_0 VA_CHECK_VERSION(1,  1, 0)
#define LIBVA_2_2_0 VA_CHECK_VERSION(1,  2, 0)
#define LIBVA_2_3_0 VA_CHECK_VERSION(1,  3, 0)
#define LIBVA(major, minor, micro) \
       (LIBVA_ ## major ## _ ## minor ## _ ## micro)

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof((a)[0]))

#define CHECK_VAS(...) do {              \
        if (vas != VA_STATUS_SUCCESS) {  \
            error_vas(vas, __VA_ARGS__); \
            return;                      \
        }                                \
    } while (0)

static void error_vas(VAStatus vas, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, ": %d (%s)\n", vas, vaErrorStr(vas));
}

static int indent_depth  = 0;
static int indent_size   = 4;
static bool pretty_print = true;

static void print_indent(void)
{
    if (!pretty_print)
        return;
    int i, j;
    for (i = 0; i < indent_depth; i++)
        for (j = 0; j < indent_size; j++)
            putchar(' ');
}

static void print_newline(void)
{
    if (pretty_print)
        printf("\n");
}

static void print_tag(const char *tag)
{
    if (tag) {
        printf("\"%s\":", tag);
        if (pretty_print)
            printf(" ");
    }
}

static void start_array(const char *tag)
{
    print_indent();
    print_tag(tag);
    printf("[");
    print_newline();
    ++indent_depth;
}

static void end_array(void)
{
    --indent_depth;
    print_indent();
    printf("],");
    print_newline();
}

static void start_object(const char *tag)
{
    print_indent();
    print_tag(tag);
    printf("{");
    print_newline();
    ++indent_depth;
}

static void end_object(void)
{
    --indent_depth;
    print_indent();
    printf("},");
    print_newline();
}

static void print_boolean(const char *tag, bool value)
{
    print_indent();
    print_tag(tag);
    printf("%s,", value ? "true" : "false");
    print_newline();
}

static void print_integer(const char *tag, int64_t value)
{
    print_indent();
    print_tag(tag);
    printf("%"PRId64",", value);
    print_newline();
}

static void print_double(const char *tag, double value)
{
    print_indent();
    print_tag(tag);
    printf("%lg,", value);
    print_newline();
}

static void print_string(const char *tag, const char *format, ...)
{
    print_indent();
    print_tag(tag);
    printf("\"");

    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);

    printf("\",");
    print_newline();
}

static struct {
    VAEntrypoint entrypoint;
    const char *name;
    const char *description;
} entrypoints[] = {
#define E(name, desc) { VAEntrypoint ## name, #name, desc }
    E(VLD,        "Decode Slice"),
    E(IZZ,        "(Legacy) ZigZag Scan"),
    E(IDCT,       "(Legacy) Inverse DCT"),
    E(MoComp,     "(Legacy) Motion Compensation"),
    E(Deblocking, "(Legacy) Deblocking"),
    E(EncSlice,   "Encode Slice"),
    E(EncPicture, "Encode Picture"),
#if LIBVA(1, 7, 1)
    E(EncSliceLP, "Encode Slice (Low Power)"),
#endif
    E(VideoProc,  "Video Processing"),
#if LIBVA(2, 0, 0)
    E(FEI,        "Flexible Encode"),
#endif
#if LIBVA(2, 1, 0)
    E(Stats,      "Stats"),
#endif
#undef E
};

static struct {
    VAProfile profile;
    const char *name;
    const char *description;
} profiles[] = {
#define P(name, desc) { VAProfile ## name,  #name, desc }
    P(None,                "Video Processing"),
    P(MPEG2Simple,         "MPEG-2 Simple Profile"),
    P(MPEG2Main,           "MPEG-2 Main Profile"),
    P(MPEG4Simple,         "MPEG-4 part 2 Simple Profile"),
    P(MPEG4AdvancedSimple, "MPEG-4 part 2 Advanced Simple Profile"),
    P(MPEG4Main,           "MPEG-4 part 2 Main Profile"),
    P(H264Baseline,        "H.264 / MPEG-4 part 10 (AVC) Baseline Profile"),
    P(H264Main,            "H.264 / MPEG-4 part 10 (AVC) Main Profile"),
    P(H264High,            "H.264 / MPEG-4 part 10 (AVC) High Profile"),
    P(VC1Simple,           "VC-1 / SMPTE 421M / WMV 9 / WMV3 Simple Profile"),
    P(VC1Main,             "VC-1 / SMPTE 421M / WMV 9 / WMV3 Main Profile"),
    P(VC1Advanced,         "VC-1 / SMPTE 421M / WMV 9 / WMV3 Advanced Profile"),
    P(H263Baseline,        "H.263"),
    P(JPEGBaseline,        "JPEG"),
    P(H264ConstrainedBaseline,
      "H.264 / MPEG-4 part 10 (AVC) Constrained Baseline Profile"),
#if LIBVA(1, 3, 0)
    P(VP8Version0_3,       "VP8 profile versions 0-3"),
#endif
#if LIBVA(1, 4, 0)
    P(H264MultiviewHigh,   "H.264 / MPEG-4 part 10 (AVC) Multiview High Profile"),
    P(H264StereoHigh,      "H.264 / MPEG-4 part 10 (AVC) Stereo High Profile"),
#endif
#if LIBVA(1, 5, 0)
    P(HEVCMain,            "H.265 / MPEG-H part 2 (HEVC) Main Profile"),
    P(HEVCMain10,          "H.265 / MPEG-H part 2 (HEVC) Main 10 Profile"),
#endif
#if LIBVA(1, 6, 0)
    P(VP9Profile0,         "VP9 profile 0"),
#endif
#if LIBVA(1, 7, 0)
    P(VP9Profile1,         "VP9 profile 1"),
    P(VP9Profile2,         "VP9 profile 2"),
    P(VP9Profile3,         "VP9 profile 3"),
#endif
#if LIBVA(2, 2, 0)
    P(HEVCMain12,          "H.265 / MPEG-H part 2 (HEVC) RExt Main 12 Profile"),
    P(HEVCMain422_10,      "H.265 / MPEG-H part 2 (HEVC) RExt Main 4:2:2 10 Profile"),
    P(HEVCMain422_12,      "H.265 / MPEG-H part 2 (HEVC) RExt Main 4:2:2 12 Profile"),
    P(HEVCMain444,         "H.265 / MPEG-H part 2 (HEVC) RExt Main 4:4:4 Profile"),
    P(HEVCMain444_10,      "H.265 / MPEG-H part 2 (HEVC) RExt Main 4:4:4 10 Profile"),
    P(HEVCMain444_12,      "H.265 / MPEG-H part 2 (HEVC) RExt Main 4:4:4 12 Profile"),
    P(HEVCSccMain,         "H.265 / MPEG-H part 2 (HEVC) SCC Screen-Extended Main Profile"),
    P(HEVCSccMain10,       "H.265 / MPEG-H part 2 (HEVC) SCC Screen-Extended Main 10 Profile"),
    P(HEVCSccMain444,      "H.265 / MPEG-H part 2 (HEVC) SCC Screen-Extended Main 4:4:4 Profile"),
#endif
#undef P
};

static struct {
    unsigned int value;
    const char *name;
} rt_format_types[] = {
#define R(name) { VA_RT_FORMAT_ ## name, #name }
    R(YUV420),
    R(YUV422),
    R(YUV444),
    R(YUV411),
    R(YUV400),
#if LIBVA(1, 6, 2)
    R(YUV420_10BPP),
#endif
    R(RGB16),
    R(RGB32),
    R(RGBP),
#if LIBVA(2, 1, 0)
    R(RGB32_10BPP),
#endif
#undef R
};

static struct {
    VAProcFilterType filter;
    const char *name;
} filters[] = {
#define F(name) { VAProcFilter ## name, #name }
    F(None),
    F(NoiseReduction),
    F(Deinterlacing),
    F(Sharpening),
    F(ColorBalance),
#if LIBVA(1, 3, 1)
    F(SkinToneEnhancement),
#endif
#if LIBVA(2, 1, 0)
    F(TotalColorCorrection),
#endif
#undef F
};

static struct {
    VAProcDeinterlacingType type;
    const char *name;
} deinterlacer_types[] = {
#define D(name) { VAProcDeinterlacing ## name, #name }
    D(None),
    D(Bob),
    D(Weave),
    D(MotionAdaptive),
    D(MotionCompensated),
#undef D
};

static struct {
    VAProcColorBalanceType type;
    const char *name;
} colour_balance_types[] = {
#define C(name) { VAProcColorBalance ## name, #name }
    C(None),
    C(Hue),
    C(Saturation),
    C(Brightness),
    C(Contrast),
    C(AutoSaturation),
    C(AutoBrightness),
    C(AutoContrast),
#undef C
};

#if LIBVA(2, 1, 0)
static struct {
    VAProcTotalColorCorrectionType type;
    const char *name;
} total_colour_correction_types[] = {
#define C(name) { VAProcTotalColorCorrection ## name, #name }
    C(None),
    C(Red),
    C(Green),
    C(Blue),
    C(Cyan),
    C(Magenta),
    C(Yellow),
#undef C
};
#endif

static struct {
    VAProcColorStandardType type;
    const char *name;
} colour_types[] = {
#define C(name) { VAProcColorStandard ## name, #name }
    C(None),
    C(BT601),
    C(BT709),
    C(BT470M),
    C(BT470BG),
    C(SMPTE170M),
    C(SMPTE240M),
    C(GenericFilm),
#if LIBVA(2, 1, 0)
    C(SRGB),
    C(STRGB),
    C(XVYCC601),
    C(XVYCC709),
    C(BT2020),
#endif
#undef C
};

static struct {
    int type;
    const char *name;
} rotation_types[] = {
#define R(name) { VA_ROTATION_ ## name, #name }
    R(NONE),
    R(90),
    R(180),
    R(270),
#undef R
};

#if LIBVA(2, 1, 0)
static struct {
    int type;
    const char *name;
} blend_types[] = {
#define B(name) { VA_BLEND_ ## name, #name }
    B(GLOBAL_ALPHA),
    B(PREMULTIPLIED_ALPHA),
    B(LUMA_KEY),
#undef B
};

static struct {
    int type;
    const char *name;
} mirror_types[] = {
#define M(name) { VA_MIRROR_ ## name, #name }
    M(NONE),
    M(HORIZONTAL),
    M(VERTICAL),
#undef M
};
#endif

static void dump_config_attributes(VADisplay display,
                                   VAProfile profile, VAEntrypoint entrypoint,
                                   unsigned int *rt_formats)
{
    VAConfigAttrib attr_list[VAConfigAttribTypeMax];
    int i, j;
    for (i = 0; i < VAConfigAttribTypeMax; i++)
        attr_list[i].type = i;

    VAStatus vas = vaGetConfigAttributes(display, profile, entrypoint,
                                         attr_list, VAConfigAttribTypeMax);
    CHECK_VAS("Unable to get config attributes");

    for (i = 0; i < VAConfigAttribTypeMax; i++) {
        uint32_t value = attr_list[i].value;
        if (value == VA_ATTRIB_NOT_SUPPORTED)
            continue;

#define AV(type, bit) do { \
            if (value & VA_ ## type ## _ ## bit) \
                print_string(NULL, #bit); \
        } while (0)

        switch(attr_list[i].type) {
        case VAConfigAttribRTFormat:
            {
                *rt_formats = value;

                start_array("rt_formats");
                for (j = 0; j < ARRAY_LENGTH(rt_format_types); j++) {
                    if (value & rt_format_types[j].value)
                        print_string(NULL, rt_format_types[j].name);
                }
                end_array();
            }
            break;
        case VAConfigAttribRateControl:
            {
                start_array("rate_control_modes");
                AV(RC, NONE);
                AV(RC, CBR);
                AV(RC, VBR);
                AV(RC, VCM);
                AV(RC, CQP);
                AV(RC, VBR_CONSTRAINED);
#if LIBVA(2, 1, 0)
                AV(RC, ICQ);
#endif
#if LIBVA(1, 7, 1)
                AV(RC, MB);
#endif
#if LIBVA(2, 1, 0)
                AV(RC, CFS);
                AV(RC, PARALLEL);
#endif
#if LIBVA(2, 3, 0)
                AV(RC, QVBR);
                AV(RC, AVBR);
#endif
                end_array();
            }
            break;
#if LIBVA(1, 6, 0)
        case VAConfigAttribDecSliceMode:
            {
                start_array("decode_slice_modes");
                AV(DEC_SLICE_MODE, NORMAL);
                AV(DEC_SLICE_MODE, BASE);
                end_array();
            }
            break;
#endif
#if LIBVA(2, 1, 0)
        case VAConfigAttribDecJPEG:
            {
                VAConfigAttribValDecJPEG jpeg = { .value = value };
                start_object("decode_jpeg");
                start_array("rotation");
                for (j = 0; j < ARRAY_LENGTH(rotation_types); j++) {
                    if (jpeg.bits.rotation & 1 << rotation_types[j].type)
                        print_string(NULL, rotation_types[j].name);
                }
                end_array();
                end_object();
            }
            break;
#endif
#if LIBVA(2, 1, 0)
        case VAConfigAttribDecProcessing:
            {
                print_boolean("decode_processing",
                              value == VA_DEC_PROCESSING);
            }
            break;
#endif
        case VAConfigAttribEncPackedHeaders:
            {
                start_array("packed_headers");
                AV(ENC_PACKED_HEADER, SEQUENCE);
                AV(ENC_PACKED_HEADER, PICTURE);
                AV(ENC_PACKED_HEADER, SLICE);
                AV(ENC_PACKED_HEADER, MISC);
                AV(ENC_PACKED_HEADER, RAW_DATA);
                end_array();
            }
            break;
        case VAConfigAttribEncInterlaced:
            {
                start_array("interlace_modes");
                AV(ENC_INTERLACED, FRAME);
                AV(ENC_INTERLACED, FIELD);
                AV(ENC_INTERLACED, MBAFF);
                AV(ENC_INTERLACED, PAFF);
                end_array();
            }
            break;
        case VAConfigAttribEncMaxRefFrames:
            {
                start_object("max_ref_frames");
                print_integer("list0", value & 0xffff);
                if (value >> 16)
                    print_integer("list1", value >> 16);
                end_object();
            }
            break;
        case VAConfigAttribEncMaxSlices:
            {
                print_integer("max_slices", value);
            }
            break;
        case VAConfigAttribEncSliceStructure:
            {
                start_array("slice_structure_modes");
                AV(ENC_SLICE_STRUCTURE, ARBITRARY_ROWS);
                AV(ENC_SLICE_STRUCTURE, POWER_OF_TWO_ROWS);
                AV(ENC_SLICE_STRUCTURE, ARBITRARY_MACROBLOCKS);
#if LIBVA(2, 0, 0)
                AV(ENC_SLICE_STRUCTURE, EQUAL_ROWS);
                AV(ENC_SLICE_STRUCTURE, MAX_SLICE_SIZE);
#endif
                end_array();
            }
            break;
        case VAConfigAttribEncMacroblockInfo:
            {
                print_integer("macroblock_info", value);
            }
            break;
#if LIBVA(2, 1, 0)
        case VAConfigAttribMaxPictureWidth:
            {
                print_integer("max_picture_width", value);
            }
            break;
        case VAConfigAttribMaxPictureHeight:
            {
                print_integer("max_picture_height", value);
            }
            break;
#endif
#if LIBVA(1, 5, 0)
        case VAConfigAttribEncJPEG:
            {
                VAConfigAttribValEncJPEG jpeg = { .value = value };
                start_object("encode_jpeg");
#define JA(name) do { print_integer(#name, jpeg.bits.name); } while (0)
                JA(arithmatic_coding_mode);
                JA(progressive_dct_mode);
                JA(non_interleaved_mode);
                JA(differential_mode);
                JA(max_num_components);
                JA(max_num_scans);
                JA(max_num_huffman_tables);
                JA(max_num_quantization_tables);
#undef JA
                end_object();
            }
            break;
#endif
#if LIBVA(1, 4, 0)
        case VAConfigAttribEncQualityRange:
            {
                print_integer("quality_range", value);
            }
            break;
#endif
#if LIBVA(2, 1, 0)
        case VAConfigAttribEncQuantization:
            {
                start_array("quantization");
                AV(ENC_QUANTIZATION, TRELLIS_SUPPORTED);
                end_object();
            }
            break;
#endif
#if LIBVA(2, 1, 0)
        case VAConfigAttribEncIntraRefresh:
            {
                start_array("intra_refresh");
                AV(ENC_INTRA_REFRESH, ROLLING_COLUMN);
                AV(ENC_INTRA_REFRESH, ROLLING_ROW);
                AV(ENC_INTRA_REFRESH, ADAPTIVE);
                AV(ENC_INTRA_REFRESH, CYCLIC);
                AV(ENC_INTRA_REFRESH, P_FRAME);
                AV(ENC_INTRA_REFRESH, B_FRAME);
                AV(ENC_INTRA_REFRESH, MULTI_REF);
                end_object();
            }
            break;
#endif
#if LIBVA(1, 6, 0)
        case VAConfigAttribEncSkipFrame:
            {
                print_integer("skip_frame", value);
            }
            break;
#endif
#if LIBVA(1, 7, 1)
        case VAConfigAttribEncROI:
            {
                VAConfigAttribValEncROI roi = { .value = value };
                start_object("roi");
                print_integer("num_regions", roi.bits.num_roi_regions);
                print_integer("rc_priority_support",
                              roi.bits.roi_rc_priority_support);
#if LIBVA(2, 0, 0)
                print_integer("rc_qp_delta_support",
                              roi.bits.roi_rc_qp_delta_support);
#endif
                end_object();
            }
            break;
#endif
#if LIBVA(1, 7, 3)
        case VAConfigAttribEncRateControlExt:
            {
                VAConfigAttribValEncRateControlExt rce = { .value = value };
                start_object("rate_control_ext");
                print_integer("max_num_temporal_layers_minus1",
                              rce.bits.max_num_temporal_layers_minus1);
                print_integer("temporal_layer_bitrate_control_flag",
                              rce.bits.temporal_layer_bitrate_control_flag);
                end_object();
            }
            break;
#endif
#if LIBVA(2, 1, 0)
        case VAConfigAttribProcessingRate:
            {
                start_array("processing_rate");
                AV(PROCESSING_RATE, ENCODE);
                AV(PROCESSING_RATE, DECODE);
                end_array();
            }
            break;
        case VAConfigAttribEncDirtyRect:
            {
                print_boolean("encode_dirty_rectangle", value);
            }
            break;
        case VAConfigAttribEncParallelRateControl:
            {
                print_integer("encode_parallel_rate_control_layers", value);
            }
            break;
        case VAConfigAttribEncDynamicScaling:
            {
                print_boolean("encode_dynamic_scaling", value);
            }
            break;
        case VAConfigAttribFrameSizeToleranceSupport:
            {
                print_boolean("encode_frame_size_tolerance", value);
            }
            break;
#endif
#if LIBVA(2, 0, 0)
        case VAConfigAttribFEIFunctionType:
            {
                start_array("fei_function_type");
                AV(FEI_FUNCTION, ENC);
                AV(FEI_FUNCTION, PAK);
                AV(FEI_FUNCTION, ENC_PAK);
                end_array();
            }
            break;
        case VAConfigAttribFEIMVPredictors:
            {
                print_integer("fei_mv_predictors", value);
            }
            break;
#endif
#if LIBVA(2, 1, 0)
        case VAConfigAttribStats:
            {
                VAConfigAttribValStats stats = { .value = value };
                start_object("stats");
#define SA(name) do { print_integer(#name, stats.bits.name); } while (0)
                SA(max_num_past_references);
                SA(max_num_future_references);
                SA(num_outputs);
                SA(interlaced);
#undef SA
                end_object();
            }
            break;
        case VAConfigAttribEncTileSupport:
            {
                print_boolean("encode_tile_support", value);
            }
            break;
        case VAConfigAttribCustomRoundingControl:
            {
                print_boolean("custom_rounding_control", value);
            }
            break;
        case VAConfigAttribQPBlockSize:
            {
                print_integer("qp_block_size", value);
            }
            break;
#endif
        default:
            {
                start_object("unknown");
                print_integer("type", attr_list[i].type);
                print_integer("value", value);
                end_object();
            }
            break;
        }
#undef AV
    }
}

static void dump_surface_attributes(VADisplay display,
                                    VAProfile profile, VAEntrypoint entrypoint,
                                    unsigned int rt_formats)
{
    unsigned int rt_format;
    for (rt_format = 1; rt_format; rt_format <<= 1) {
        if (!(rt_format & rt_formats))
            continue;

        VAConfigAttrib attr_rt_format = {
            .type  = VAConfigAttribRTFormat,
            .value = rt_format,
        };

        VAConfigID config;
        VAStatus vas = vaCreateConfig(display, profile, entrypoint,
                                      &attr_rt_format, 1, &config);
        CHECK_VAS("Unable to create config to test surface attributes");

        VASurfaceAttrib *attr_list;
        unsigned int attr_count;

        vas = vaQuerySurfaceAttributes(display, config, 0, &attr_count);
        CHECK_VAS("Unable to query surface attributes");

        attr_list = calloc(attr_count, sizeof(*attr_list));

        vas = vaQuerySurfaceAttributes(display, config,
                                       attr_list, &attr_count);
        CHECK_VAS("Unable to query surface attributes");

        start_object(NULL);

        int i;
        for (i = 0; i < ARRAY_LENGTH(rt_format_types); i++) {
            if (rt_format & rt_format_types[i].value) {
                print_string("rt_format", rt_format_types[i].name);
                break;
            }
        }
        if (!(i < ARRAY_LENGTH(rt_format_types)))
            print_string("rt_format", "unknown");

        bool has_formats = false;

        for (i = 0; i < attr_count; i++) {
#define AV(type, bit) do { \
                if (attr_list[i].value.value.i & VA_ ## type ## _ ## bit) \
                    print_string(NULL, #bit); \
            } while (0)

            switch (attr_list[i].type) {
            case VASurfaceAttribPixelFormat:
                has_formats = true;
                break;
            case VASurfaceAttribMinWidth:
                print_integer("min_width", attr_list[i].value.value.i);
                break;
            case VASurfaceAttribMaxWidth:
                print_integer("max_width", attr_list[i].value.value.i);
                break;
            case VASurfaceAttribMinHeight:
                print_integer("min_height", attr_list[i].value.value.i);
                break;
            case VASurfaceAttribMaxHeight:
                print_integer("max_height", attr_list[i].value.value.i);
                break;
            case VASurfaceAttribMemoryType:
                {
                    start_array("memory_types");
                    AV(SURFACE_ATTRIB_MEM_TYPE, VA);
                    AV(SURFACE_ATTRIB_MEM_TYPE, V4L2);
                    AV(SURFACE_ATTRIB_MEM_TYPE, USER_PTR);
                    AV(SURFACE_ATTRIB_MEM_TYPE, KERNEL_DRM);
                    AV(SURFACE_ATTRIB_MEM_TYPE, DRM_PRIME);
#if LIBVA(2, 1, 0)
                    AV(SURFACE_ATTRIB_MEM_TYPE, DRM_PRIME_2);
#endif
                    end_array();
                }
                break;
            case VASurfaceAttribExternalBufferDescriptor:
                // Ignored (write-only).
                break;
#if LIBVA(1, 4, 0)
            case VASurfaceAttribUsageHint:
                {
                    start_array("usage_hints");
                    AV(SURFACE_ATTRIB_USAGE_HINT, DECODER);
                    AV(SURFACE_ATTRIB_USAGE_HINT, ENCODER);
                    AV(SURFACE_ATTRIB_USAGE_HINT, VPP_READ);
                    AV(SURFACE_ATTRIB_USAGE_HINT, VPP_WRITE);
                    AV(SURFACE_ATTRIB_USAGE_HINT, DISPLAY);
                    end_array();
                }
                break;
#endif
            default:
                {
                    start_object("unknown");
                    print_integer("type", attr_list[i].type);
                    print_integer("value", attr_list[i].value.value.i);
                    end_object();
                }
                break;
            }

#undef AV
        }

        if (has_formats) {
            start_array("pixel_formats");

            for (i = 0; i < attr_count; i++) {
                if (attr_list[i].type != VASurfaceAttribPixelFormat)
                    continue;

                print_string(NULL, "%.4s", &attr_list[i].value.value.i);
            }

            end_array();
        }

        end_object();

        free(attr_list);
    }
}

static void dump_colour_standards(VAProcColorStandardType *types, int num)
{
    int i, j;
    for (i = 0; i < num; i++) {
        for (j = 0; j < ARRAY_LENGTH(colour_types); j++) {
            if (types[i] == colour_types[j].type)
                break;
        }

        start_object(NULL);

        print_integer("type", types[i]);
        if (j < ARRAY_LENGTH(colour_types))
            print_string("name", colour_types[j].name);
        else
            print_string("name", "unknown");

        end_object();
    }
}

static void dump_filter_caps(VADisplay display, unsigned int rt_format)
{
    VAStatus vas;
    int i, j, k;

    VAConfigAttrib attr_rt_format = {
        .type  = VAConfigAttribRTFormat,
        .value = rt_format,
    };

    VAConfigID config;
    vas = vaCreateConfig(display, VAProfileNone,
                         VAEntrypointVideoProc,
                         &attr_rt_format, 1, &config);
    CHECK_VAS("Unable to create config to test filters");

    VAContextID context;
    vas = vaCreateContext(display, config, 1280, 720, 0,
                          NULL, 0, &context);
    CHECK_VAS("Unable to create context to test filters");

    VAProcFilterType filter_list[VAProcFilterCount];
    unsigned int filter_count = VAProcFilterCount;
    vas = vaQueryVideoProcFilters(display, context,
                                  filter_list, &filter_count);
    CHECK_VAS("Failed to query filters");

    start_array("filters");

    for (i = 0; i < filter_count; i++) {
        for (j = 0; j < ARRAY_LENGTH(filters); j++) {
            if (filters[j].filter == filter_list[i])
                break;
        }

        start_object(NULL);

        print_integer("filter", filter_list[i]);
        if (j < ARRAY_LENGTH(filters))
            print_string("name", filters[j].name);

        switch (filter_list[i]) {
        case VAProcFilterDeinterlacing:
            {
                VAProcFilterCapDeinterlacing deint[VAProcDeinterlacingCount];
                unsigned int deint_count = ARRAY_LENGTH(deint);
                memset(&deint, 0, sizeof(deint));

                vas = vaQueryVideoProcFilterCaps(display, context,
                                                 VAProcFilterDeinterlacing,
                                                 &deint, &deint_count);
                CHECK_VAS("Failed to query deinterlacing caps");

                start_array("types");

                for (j = 0; j < deint_count; j++) {
                    for (k = 0; k < ARRAY_LENGTH(deinterlacer_types); k++) {
                        if (deint[j].type == deinterlacer_types[k].type)
                            break;
                    }

                    start_object(NULL);

                    print_integer("type", deint[j].type);
                    if (k < ARRAY_LENGTH(deinterlacer_types))
                        print_string("name", deinterlacer_types[k].name);

                    end_object();
                }

                end_array();
            }
            break;
        case VAProcFilterColorBalance:
            {
                VAProcFilterCapColorBalance colour[VAProcColorBalanceCount];
                unsigned int colour_count = ARRAY_LENGTH(colour);
                memset(&colour, 0, sizeof(colour));

                vas = vaQueryVideoProcFilterCaps(display, context,
                                                 VAProcFilterColorBalance,
                                                 &colour, &colour_count);
                CHECK_VAS("Failed to query colour balance caps");

                start_array("types");

                for (j = 0; j < colour_count; j++) {
                    for (k = 0; k < ARRAY_LENGTH(colour_balance_types); k++) {
                        if (colour[j].type == colour_balance_types[k].type)
                            break;
                    }

                    start_object(NULL);

                    print_integer("type", colour[j].type);
                    if (k < ARRAY_LENGTH(colour_balance_types))
                        print_string("name", colour_balance_types[k].name);

                    print_double("min_value",     colour[j].range.min_value);
                    print_double("max_value",     colour[j].range.max_value);
                    print_double("default_value", colour[j].range.default_value);
                    print_double("step",          colour[j].range.step);

                    end_object();
                }

                end_array();
            }
            break;
#if LIBVA(2, 1, 0)
        case VAProcFilterTotalColorCorrection:
            {
                VAProcFilterCapTotalColorCorrection
                    colour[VAProcTotalColorCorrectionCount];
                unsigned int colour_count = ARRAY_LENGTH(colour);
                memset(&colour, 0, sizeof(colour));

                vas = vaQueryVideoProcFilterCaps(display, context,
                                                 VAProcFilterTotalColorCorrection,
                                                 &colour, &colour_count);
                CHECK_VAS("Failed to query total colour correction caps");

                for (j = 0; j < colour_count; j++) {
                    for (k = 0; k < ARRAY_LENGTH(total_colour_correction_types); k++) {
                        if (colour[j].type == total_colour_correction_types[k].type)
                            break;
                    }

                    start_object(NULL);

                    print_integer("type", colour[j].type);
                    if (k < ARRAY_LENGTH(total_colour_correction_types))
                        print_string("name", total_colour_correction_types[k].name);

                    print_double("min_value",     colour[j].range.min_value);
                    print_double("max_value",     colour[j].range.max_value);
                    print_double("default_value", colour[j].range.default_value);
                    print_double("step",          colour[j].range.step);

                    end_object();
                }
            }
            break;
#endif
        default:
            {
                VAProcFilterCap cap;
                unsigned int cap_count = 1;
                memset(&cap, 0, sizeof(cap));

                vas = vaQueryVideoProcFilterCaps(display, context,
                                                 filter_list[i],
                                                 &cap, &cap_count);
                CHECK_VAS("Failed to query filter caps");

                if (cap_count > 0) {
                    print_double("min_value",     cap.range.min_value);
                    print_double("max_value",     cap.range.max_value);
                    print_double("default_value", cap.range.default_value);
                    print_double("step",          cap.range.step);
                }
            }
        }

        end_object();
    }

    end_array(); // filters array.

    VAProcPipelineCaps pipeline;
    memset(&pipeline, 0, sizeof(pipeline));

    vas = vaQueryVideoProcPipelineCaps(display, context,
                                       NULL, 0, &pipeline);
    CHECK_VAS("Failed to pipeline caps");

    start_object("pipeline");

    print_integer("pipeline_flags",      pipeline.pipeline_flags);
    print_integer("filter_flags",        pipeline.filter_flags);
    print_integer("num_forward_references",  pipeline.num_forward_references);
    print_integer("num_backward_references", pipeline.num_backward_references);

    start_array("input_colour_standards");
    dump_colour_standards(pipeline.input_color_standards,
                          pipeline.num_input_color_standards);
    end_array();

    start_array("output_colour_standards");
    dump_colour_standards(pipeline.output_color_standards,
                          pipeline.num_output_color_standards);
    end_array();

#if LIBVA(2, 1, 0)
    start_array("rotation_flags");
    for (i = 0; i < ARRAY_LENGTH(rotation_types); i++) {
        if (pipeline.rotation_flags & 1 << rotation_types[i].type)
            print_string(NULL, rotation_types[i].name);
    }
    end_array();

    start_array("blend_flags");
    for (i = 0; i < ARRAY_LENGTH(blend_types); i++) {
        if (pipeline.blend_flags & 1 << blend_types[i].type)
            print_string(NULL, blend_types[i].name);
    }
    end_array();

    start_array("mirror_flags");
    for (i = 0; i < ARRAY_LENGTH(mirror_types); i++) {
        if (pipeline.mirror_flags & 1 << mirror_types[i].type)
            print_string(NULL, mirror_types[i].name);
    }
    end_array();

    print_integer("num_additional_outputs", pipeline.num_additional_outputs);

    start_array("input_pixel_formats");
    for (i = 0; i < pipeline.num_input_pixel_formats; i++)
        print_string(NULL, "%.4s", pipeline.input_pixel_format[i]);
    end_array();

    start_array("output_pixel_formats");
    for (i = 0; i < pipeline.num_output_pixel_formats; i++)
        print_string(NULL, "%.4s", pipeline.output_pixel_format[i]);
    end_array();

#define ATTR(name) do { print_integer(#name, pipeline.name); } while (0)
    ATTR(max_input_width);
    ATTR(max_input_height);
    ATTR(min_input_width);
    ATTR(min_input_height);

    ATTR(max_output_width);
    ATTR(max_output_height);
    ATTR(min_output_width);
    ATTR(min_output_height);
#undef ATTR
#endif

    end_object(); // pipeline object.

    vaDestroyContext(display, context);
    vaDestroyConfig(display, config);
}

static void dump_entrypoints(VADisplay display, VAProfile profile)
{
    int entrypoint_count = vaMaxNumEntrypoints(display);
    VAEntrypoint *entrypoint_list = calloc(entrypoint_count,
                                           sizeof(*entrypoint_list));

    VAStatus vas = vaQueryConfigEntrypoints(display, profile,
                                            entrypoint_list, &entrypoint_count);
    CHECK_VAS("Unable to query entrypoints");

    int i, j;
    for (i = 0; i < entrypoint_count; i++) {
        for (j = 0; j < ARRAY_LENGTH(entrypoints); j++) {
            if (entrypoints[j].entrypoint == entrypoint_list[i])
                break;
        }

        start_object(NULL);

        print_integer("entrypoint", entrypoint_list[i]);
        if (j < ARRAY_LENGTH(entrypoints)) {
            print_string("name", "%s", entrypoints[j].name);
            print_string("description", "%s", entrypoints[j].description);
        }

        unsigned int rt_formats = 0;

        start_object("attributes");
        dump_config_attributes(display, profile, entrypoint_list[i],
                               &rt_formats);
        end_object();

        if (rt_formats) {
            start_array("surface_formats");
            dump_surface_attributes(display, profile, entrypoint_list[i],
                                    rt_formats);
            end_array();

            if (entrypoint_list[i] == VAEntrypointVideoProc)
                dump_filter_caps(display, rt_formats);
        }

        end_object();
    }

    free(entrypoint_list);
}

static void dump_profiles(VADisplay display)
{
    int profile_count = vaMaxNumProfiles(display);
    VAProfile *profile_list = calloc(profile_count, sizeof(*profile_list));

    VAStatus vas = vaQueryConfigProfiles(display,
                                         profile_list, &profile_count);
    CHECK_VAS("Unable to query profiles");

    int i, j;
    for (i = 0; i < profile_count; i++) {
        for (j = 0; j < ARRAY_LENGTH(profiles); j++) {
            if (profiles[j].profile == profile_list[i])
                break;
        }

        start_object(NULL);

        print_integer("profile", profile_list[i]);
        if (j < ARRAY_LENGTH(profiles)) {
            print_string("name", "%s", profiles[j].name);
            print_string("description", "%s", profiles[j].description);
        }

        start_array("entrypoints");
        dump_entrypoints(display, profile_list[i]);
        end_array();

        end_object();
    }

    free(profile_list);
}

static void dump_image_formats(VADisplay display)
{
    int format_count = vaMaxNumImageFormats(display);
    VAImageFormat *format_list = calloc(format_count, sizeof(*format_list));

    VAStatus vas = vaQueryImageFormats(display, format_list, &format_count);
    CHECK_VAS("Unable to query image formats");

    int i;
    for (i = 0; i < format_count; i++) {
        start_object(NULL);

        print_string("pixel_format", "%.4s", &format_list[i].fourcc);

        print_string("byte_order",
                     format_list[i].byte_order == VA_LSB_FIRST ? "LE" :
                     format_list[i].byte_order == VA_MSB_FIRST ? "BE" :
                     "unknown");

        print_integer("bits_per_pixel", format_list[i].bits_per_pixel);

        if (format_list[i].depth) {
            print_integer("depth", format_list[i].depth);

            print_integer("red_mask",   format_list[i].red_mask);
            print_integer("green_mask", format_list[i].green_mask);
            print_integer("blue_mask",  format_list[i].blue_mask);
            print_integer("alpha_mask", format_list[i].alpha_mask);
        }

        end_object();
    }

    free(format_list);
}

static void dump_subpicture_formats(VADisplay display)
{
    unsigned int format_count = vaMaxNumSubpictureFormats(display);
    VAImageFormat *format_list = calloc(format_count, sizeof(*format_list));
    unsigned int *flags_list = calloc(format_count, sizeof(*flags_list));

    VAStatus vas = vaQuerySubpictureFormats(display, format_list,
                                            flags_list, &format_count);
    CHECK_VAS("Unable to query subpicture formats");

    int i;
    for (i = 0; i < format_count; i++) {
        start_object(NULL);

        print_string("pixel_format", "%.4s", &format_list[i].fourcc);

        print_string("byte_order",
                     format_list[i].byte_order == VA_LSB_FIRST ? "LE" :
                     format_list[i].byte_order == VA_MSB_FIRST ? "BE" :
                     "unknown");

        print_integer("bits_per_pixel", format_list[i].bits_per_pixel);

        if (format_list[i].depth) {
            print_integer("depth", format_list[i].depth);

            print_integer("red_mask",   format_list[i].red_mask);
            print_integer("green_mask", format_list[i].green_mask);
            print_integer("blue_mask",  format_list[i].blue_mask);
            print_integer("alpha_mask", format_list[i].alpha_mask);
        }

        start_array("flags");
#define SF(type, bit) do { \
            if (flags_list[i] & VA_ ## type ## _ ## bit) \
                print_string(NULL, #bit); \
        } while (0)
        SF(SUBPICTURE, CHROMA_KEYING);
        SF(SUBPICTURE, GLOBAL_ALPHA);
        SF(SUBPICTURE, DESTINATION_IS_SCREEN_COORD);
#undef SF
        end_array();

        end_object();
    }

    free(format_list);
    free(flags_list);
}

static void die(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(1);
}

int main(int argc, char **argv)
{
    int option_index = 0;
    static struct option long_options[] = {
        { "indent",  required_argument, 0, 'i' },
        { "ugly",    no_argument,       0, 'u' },
        { "device",  required_argument, 0, 'd' },
    };

    const char *drm_device = NULL;

    while (1) {
        int c = getopt_long(argc, argv, "vi:ud:x", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'i':
            sscanf(optarg, "%d", &indent_size);
            break;
        case 'u':
            pretty_print = false;
            break;
        case 'd':
            drm_device = optarg;
            break;
        default:
            die("Unknown option.\n");
        }
    }

    if (!drm_device)
        drm_device = "/dev/dri/renderD128";

    int drm_fd = open(drm_device, O_RDWR);
    if (drm_fd < 0)
        die("Failed to open %s: %m.\n", drm_device);

    VADisplay display = vaGetDisplayDRM(drm_fd);
    if (!display)
        die("Failed to open VA display from DRM device.\n");

    VAStatus vas;
    int major = 0, minor = 0;
    vas = vaInitialize(display, &major, &minor);
    if (vas != VA_STATUS_SUCCESS)
        die("Failed to initialise: %d (%s).\n", vas, vaErrorStr(vas));

    start_object(NULL);

    start_object("build_version");
    print_integer("major", VA_MAJOR_VERSION);
    print_integer("minor", VA_MINOR_VERSION);
    print_integer("micro", VA_MICRO_VERSION);
    end_object();

    start_object("driver_version");
    print_integer("major", major);
    print_integer("minor", minor);
    end_object();

    const char *vendor_string = vaQueryVendorString(display);
    if (vendor_string)
        print_string("driver_vendor", "%s", vendor_string);
    else
        print_string("driver_vendor", "unknown");

    start_array("profiles");
    dump_profiles(display);
    end_array();

    start_array("image_formats");
    dump_image_formats(display);
    end_array();

    start_array("subpicture_formats");
    dump_subpicture_formats(display);
    end_array();

    printf("}\n");

    vaTerminate(display);

    return 0;
}
