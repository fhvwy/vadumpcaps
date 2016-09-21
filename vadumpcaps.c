/*
 * vadumpcaps - Show all VAAPI capabilities.
 * Copyright (C) 2016 Mark Thompson <sw@jkqxz.net>
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

#include <X11/Xlib.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_x11.h>

#define ARRAY_LENGTH(a) (sizeof(a)/sizeof((a)[0]))

static int verbosity;

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

static bool output_json = true;
static int  write_depth = 0;

static void print_depth(void)
{
    int i;
    for (i = 0; i < write_depth; i++)
        printf("    ");
}

static void start_array(const char *tag)
{
    if (output_json) {
        print_depth();
        if (tag)
            printf("\"%s\": ", tag);
        printf("[\n");
    } else {
        if (tag) {
            print_depth();
            printf("%s:\n", tag);
        }
    }
    ++write_depth;
}

static void end_array(void)
{
    --write_depth;
    if (output_json) {
        print_depth();
        printf("],\n");
    }
}

static void start_object(const char *tag)
{
    if (output_json) {
        print_depth();
        if (tag)
            printf("\"%s\": ", tag);
        printf("{\n");
    } else {
        if (tag) {
            print_depth();
            printf("%s:\n", tag);
        }
    }
    ++write_depth;
}

static void end_object(void)
{
    --write_depth;
    if (output_json) {
        print_depth();
        printf("},\n");
    }
}

static void print_integer(const char *tag, int64_t value)
{
    print_depth();

    if (output_json) {
        if (tag)
            printf("\"%s\": ", tag);
    } else {
        if (tag)
            printf("%s: ", tag);
    }
    printf("%"PRId64, value);

    if (output_json)
        printf(",");
    printf("\n");
}

static void print_double(const char *tag, double value)
{
    print_depth();

    if (output_json) {
        if (tag)
            printf("\"%s\": ", tag);
    } else {
        if (tag)
            printf("%s: ", tag);
    }
    printf("%lg", value);

    if (output_json)
        printf(",");
    printf("\n");
}

static void print_string(const char *tag, const char *format, ...)
{
    print_depth();

    if (output_json) {
        if (tag)
            printf("\"%s\": ", tag);
        printf("\"");
    } else {
        if (tag)
            printf("%s: ", tag);
    }

    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);

    if (output_json)
        printf("\",");
    printf("\n");
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
#if VA_CHECK_VERSION(0, 39, 2)
    E(EncSliceLP, "Encode Slice (Low Power)"),
#endif
    E(VideoProc,  "Video Processing"),
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
#if VA_CHECK_VERSION(0, 35, 0)
    P(VP8Version0_3,       "VP8 profile versions 0-3"),
#endif
#if VA_CHECK_VERSION(0, 36, 0)
    P(H264MultiviewHigh,   "H.264 / MPEG-4 part 10 (AVC) Multiview High Profile"),
    P(H264StereoHigh,      "H.264 / MPEG-4 part 10 (AVC) Stereo High Profile"),
#endif
#if VA_CHECK_VERSION(0, 37, 0)
    P(HEVCMain,            "H.265 / MPEG-H part 2 (HEVC) Main Profile"),
    P(HEVCMain10,          "H.265 / MPEG-H part 2 (HEVC) Main 10 Profile"),
#endif
#if VA_CHECK_VERSION(0, 38, 0)
    P(VP9Profile0,         "VP9 profile 0"),
#endif
#if VA_CHECK_VERSION(0, 39, 0)
    P(VP9Profile1,         "VP9 profile 1"),
    P(VP9Profile2,         "VP9 profile 2"),
    P(VP9Profile3,         "VP9 profile 3"),
#endif
#undef P
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
    F(SkinToneEnhancement),
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
#undef C
};

static void dump_config_attributes(VADisplay display,
                                   VAProfile profile, VAEntrypoint entrypoint,
                                   unsigned int *rt_formats)
{
    VAConfigAttrib attr_list[VAConfigAttribTypeMax];
    int i;
    for (i = 0; i < VAConfigAttribTypeMax; i++)
        attr_list[i].type = i;

    VAStatus vas = vaGetConfigAttributes(display, profile, entrypoint,
                                         attr_list, VAConfigAttribTypeMax);
    CHECK_VAS("Unable to get config attributes");

    for (i = 0; i < VAConfigAttribTypeMax; i++) {
        if (attr_list[i].value == VA_ATTRIB_NOT_SUPPORTED)
            continue;

        start_object(0);

#define AV(type, bit) do { \
            if (attr_list[i].value & VA_ ## type ## _ ## bit) \
                print_string(0, #bit); \
        } while (0)

        switch(attr_list[i].type) {
        case VAConfigAttribRTFormat:
            {
                *rt_formats = attr_list[i].value;

                start_array("rt_formats");
                AV(RT_FORMAT, YUV420);
                AV(RT_FORMAT, YUV422);
                AV(RT_FORMAT, YUV444);
                AV(RT_FORMAT, YUV411);
                AV(RT_FORMAT, YUV400);
                AV(RT_FORMAT, YUV420_10BPP);
                AV(RT_FORMAT, RGB16);
                AV(RT_FORMAT, RGB32);
                AV(RT_FORMAT, RGBP);
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
                end_array();
            }
            break;
#if VA_CHECK_VERSION(0, 38, 0)
        case VAConfigAttribDecSliceMode:
            {
                start_array("decode_slice_modes");
                AV(DEC_SLICE_MODE, NORMAL);
                AV(DEC_SLICE_MODE, BASE);
                end_array();
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
                print_integer("max_ref_frames_l0", attr_list[i].value & 0xffff);
                if (attr_list[i].value >> 16)
                    print_integer("max_ref_frames_l1", attr_list[i].value >> 16);
            }
            break;
        case VAConfigAttribEncMaxSlices:
            {
                print_integer("max_slices", attr_list[i].value);
            }
            break;
        case VAConfigAttribEncSliceStructure:
            {
                start_array("slice_structure_modes");
                AV(ENC_SLICE_STRUCTURE, ARBITRARY_ROWS);
                AV(ENC_SLICE_STRUCTURE, POWER_OF_TWO_ROWS);
                AV(ENC_SLICE_STRUCTURE, ARBITRARY_MACROBLOCKS);
                end_array();
            }
            break;
        case VAConfigAttribEncMacroblockInfo:
            {
                print_integer("macroblock_info", attr_list[i].value);
            }
            break;
#if VA_CHECK_VERSION(0, 37, 0)
        case VAConfigAttribEncJPEG:
            {
                VAConfigAttribValEncJPEG jpeg = { .value = attr_list[i].value };
                start_object("jpeg");
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
#if VA_CHECK_VERSION(0, 36, 0)
        case VAConfigAttribEncQualityRange:
            {
                print_integer("quality_range", attr_list[i].value);
            }
            break;
#endif
#if VA_CHECK_VERSION(0, 38, 0)
        case VAConfigAttribEncSkipFrame:
            {
                print_integer("skip_frame", attr_list[i].value);
            }
            break;
#endif
#if VA_CHECK_VERSION(0, 39, 2)
        case VAConfigAttribEncROI:
            {
                VAConfigAttribValEncROI roi = { .value = attr_list[i].value };
                start_object("roi");
                print_integer("num_regions", roi.bits.num_roi_regions);
                print_integer("rc_priority_support", roi.bits.roi_rc_priority_support);
                end_object();
            }
            break;
#endif
        default:
            {
                start_object("unknown");
                print_integer("type", attr_list[i].type);
                print_integer("value", attr_list[i].value);
                end_object();
            }
            break;
        }
#undef AV

        end_object();
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

        start_object(0);
        print_integer("rt_format", rt_format);

        bool has_formats = false;

        int i;
        for (i = 0; i < attr_count; i++) {

#define AV(type, bit) do { \
                if (attr_list[i].value.value.i & VA_ ## type ## _ ## bit) \
                    print_string(0, #bit); \
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
                    end_array();
                }
                break;
            case VASurfaceAttribExternalBufferDescriptor:
                // Ignored (write-only).
                break;
#if VA_CHECK_VERSION(0, 36, 0)
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

                print_string(0, "%.4s", &attr_list[i].value.value.i);
            }

            end_array();
        }

        end_object();

        free(attr_list);
    }
}

static void dump_filters(VADisplay display, unsigned int rt_formats)
{
    VAStatus vas;
    unsigned int rt_format;

    for (rt_format = 1; rt_format; rt_format <<= 1) {
        if (!(rt_format & rt_formats))
            continue;

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

        int i, j, k;
        for (i = 0; i < filter_count; i++) {
            for (j = 0; j < ARRAY_LENGTH(filters); j++) {
                if (filters[j].filter == filter_list[i])
                    break;
            }

            start_object(0);

            print_integer("filter", filter_list[i]);
            if (j < ARRAY_LENGTH(filters))
                print_string("name", filters[j].name);

            switch (filter_list[i]) {
            case VAProcFilterDeinterlacing:
                {
                    VAProcFilterCapDeinterlacing deint[VAProcDeinterlacingCount];
                    unsigned int deint_count = ARRAY_LENGTH(deint);

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

                        start_object(0);

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

                        start_object(0);

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
            default:
                {
                    VAProcFilterCap cap;
                    unsigned int cap_count = 1;

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

        VAProcPipelineCaps pipeline;
        vas = vaQueryVideoProcPipelineCaps(display, context,
                                           NULL, 0, &pipeline);
        CHECK_VAS("Failed to pipeline caps");

        start_object("pipeline");

        print_integer("pipeline_flags",      pipeline.pipeline_flags);
        print_integer("filter_flags",        pipeline.filter_flags);
        print_integer("forward_references",  pipeline.num_forward_references);
        print_integer("backward_references", pipeline.num_backward_references);

        start_array("input_colour_standards");

        for (i = 0; i < pipeline.num_input_color_standards; i++) {
            for (j = 0; j < ARRAY_LENGTH(colour_types); j++) {
                if (pipeline.input_color_standards[i] ==
                    colour_types[j].type)
                    break;
            }

            start_object(0);

            print_integer("type", pipeline.input_color_standards[i]);
            if (j < ARRAY_LENGTH(colour_types))
                print_string("name", colour_types[j].name);

            end_object();
        }

        end_array();

        start_array("output_colour_standards");

        for (i = 0; i < pipeline.num_output_color_standards; i++) {
            for (j = 0; j < ARRAY_LENGTH(colour_types); j++) {
                if (pipeline.output_color_standards[i] ==
                    colour_types[j].type)
                    break;
            }

            start_object(0);

            print_integer("type", pipeline.output_color_standards[i]);
            if (j < ARRAY_LENGTH(colour_types))
                print_string("name", colour_types[j].name);

            end_object();
        }

        end_array();

        end_object();

        vaDestroyContext(display, context);
        vaDestroyConfig(display, config);
    }
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

        start_object(0);

        print_integer("entrypoint", entrypoint_list[i]);
        if (j < ARRAY_LENGTH(entrypoints)) {
            print_string("name", "%s", entrypoints[j].name);
            print_string("description", "%s", entrypoints[j].description);
        }

        unsigned int rt_formats = 0;

        start_array("attributes");
        dump_config_attributes(display, profile, entrypoint_list[i],
                               &rt_formats);
        end_array();

        if (rt_formats) {
            start_array("surface_formats");
            dump_surface_attributes(display, profile, entrypoint_list[i],
                                    rt_formats);
            end_array();

            if (entrypoint_list[i] == VAEntrypointVideoProc) {
              start_array("filters");
              dump_filters(display, rt_formats);
              end_array();
            }
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

        start_object(0);

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
        start_object(0);

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
        start_object(0);

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

        start_object("flags");
#define SF(type, bit) do { \
            if (flags_list[i] & VA_ ## type ## _ ## bit) \
                print_string(0, #bit); \
        } while (0)
        SF(SUBPICTURE, CHROMA_KEYING);
        SF(SUBPICTURE, GLOBAL_ALPHA);
        SF(SUBPICTURE, DESTINATION_IS_SCREEN_COORD);
#undef SF
        end_object();

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

static VADisplay open_device_drm(const char *drm_device)
{
    int drm_fd = open(drm_device, O_RDWR);
    if (drm_fd < 0)
        die("Failed to open %s: %m.\n", drm_device);

    VADisplay display = vaGetDisplayDRM(drm_fd);
    if (!display)
        die("Failed to open VA display from DRM device.\n");

    return display;
}

static VADisplay open_device_x11(const char *x11_display_name)
{
    Display *x11_display = XOpenDisplay(x11_display_name);
    if (!x11_display)
        die("Failed to open X11 display %s.\n", x11_display_name);

    VADisplay display = vaGetDisplay(x11_display);
    if (!display)
        die("Failed to open VA Display from X11 display.\n");

    return display;
}

int main(int argc, char **argv)
{
    int option_index = 0;
    static struct option long_options[] = {
        { "verbose", no_argument,       0, 'v' },
        { "json",    no_argument,       0, 'j' },
        { "device",  required_argument, 0, 'd' },
        { "x11",     optional_argument, 0, 'x' },
    };

    VADisplay display;
    const char *drm_device  = 0;
    const char *x11_display = 0;

    while (1) {
        int c = getopt_long(argc, argv, "vjd:x", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'v':
            ++verbosity;
            break;
        case 'p':
            output_json = false;
            break;
        case 'd':
            drm_device = strdup(optarg);
            break;
        case 'x':
            if (optarg) {
                x11_display = strdup(optarg);
            } else {
                const char *x = getenv("DISPLAY");
                if (!x)
                    die("DISPLAY not set.\n");
                x11_display = strdup(x);
            }
            break;
        default:
            die("Unknown option.\n");
        }
    }

    if (drm_device) {
        display = open_device_drm(drm_device);
    } else {
        display = open_device_x11(x11_display);
    }

    VAStatus vas;
    int major = 0, minor = 0;
    vas = vaInitialize(display, &major, &minor);
    if (vas != VA_STATUS_SUCCESS)
        die("Failed to initialise: %d (%s).\n", vas, vaErrorStr(vas));

    start_object(0);

    start_object("headers");
    print_integer("major", VA_MAJOR_VERSION);
    print_integer("minor", VA_MINOR_VERSION);
    print_integer("micro", VA_MICRO_VERSION);
    end_object();

    start_object("runtime");
    print_integer("major", major);
    print_integer("minor", minor);
    end_object();

    const char *vendor_string = vaQueryVendorString(display);
    if (vendor_string)
        print_string("vendor", "%s", vendor_string);
    else
        print_string("vendor", "unknown");

    start_array("profiles");
    dump_profiles(display);
    end_array();

    start_array("image_formats");
    dump_image_formats(display);
    end_array();

    start_array("subpicture_formats");
    dump_subpicture_formats(display);
    end_array();

    end_object();

    return 0;
}
