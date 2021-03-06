#ifndef FFMPEG_CONFIG_H
#define FFMPEG_CONFIG_H

#if defined (__GNUC__)
  #define HAVE_INLINE_ASM 1
  #define HAVE_MMX 1
  #define HAVE_MMX2 1
  #define HAVE_SSE 1
  #define HAVE_SSSE3 1
  #define HAVE_AMD3DNOW 1
  #define HAVE_AMD3DNOWEXT 1

  #define ARCH_X86 1

  #ifdef ARCH_X86_64
    #define ARCH_X86_32 0
    #define ARCH_X86_64 1
    #define HAVE_FAST_64BIT 1
    #define HAVE_STRUCT_TIMESPEC 1
  #else
    #define ARCH_X86_32 1
    #define ARCH_X86_64 0
    #define HAVE_FAST_64BIT 0
  #endif

  #define PTW32_STATIC_LIB 1
  #define restrict restrict
#else
  #define __ICC __INTEL_COMPILER
  #define _ICC __INTEL_COMPILER
  #define ARCH_X86 0
  #define HAVE_INLINE_ASM 0
  #define HAVE_MMX 0
  #define HAVE_MMX2 0
  #define HAVE_SSE 0
  #define HAVE_SSSE3 0
  #define HAVE_AMD3DNOW 0
  #define HAVE_AMD3DNOWEXT 0
  #define snprintf _snprintf // not secure. Only for testing.
  #define __mingw_aligned_malloc _aligned_malloc
  #define __mingw_aligned_realloc _aligned_realloc
  #define __mingw_aligned_free _aligned_free
  #include "libavutil/mathematics.h"
#endif

// Use DPRINTF instead of av_log. To be used for debug purpose because DPRINTF will be always called (the
// registry switch is not read)
#define USE_DPRINTF 0

#define LIBAV_CONFIGURATION "ffdshow custom"
#define LIBAV_LICENSE "GPL version 2 or later"

#define CC_TYPE "gcc"
#define CC_VERSION __VERSION__

#define ASMALIGN(ZEROBITS) ".p2align " #ZEROBITS "\n\t"

#if ARCH_X86_64
  #define EXTERN_PREFIX ""
  #define EXTERN_ASM
#else
  #define EXTERN_PREFIX "_"
  #define EXTERN_ASM _
#endif

#define ARCH_ALPHA 0
#define ARCH_ARM 0
#define ARCH_AVR32 0
#define ARCH_AVR32_AP 0
#define ARCH_AVR32_UC 0
#define ARCH_BFIN 0
#define ARCH_IA64 0
#define ARCH_M68K 0
#define ARCH_MIPS 0
#define ARCH_MIPS64 0
#define ARCH_PARISC 0
#define ARCH_PPC 0
#define ARCH_PPC64 0
#define ARCH_S390 0
#define ARCH_SH4 0
#define ARCH_SPARC 0
#define ARCH_SPARC64 0
#define ARCH_TOMI 0

#define HAVE_ALTIVEC 0
#define HAVE_ARMV5TE 0
#define HAVE_ARMV6 0
#define HAVE_ARMV6T2 0
#define HAVE_ARMVFP 0
#define HAVE_AVX 0
#define HAVE_IWMMXT 0
#define HAVE_MMI 0
#define HAVE_NEON 0
#define HAVE_PPC4XX 0
#define HAVE_VIS 0

#define HAVE_ALIGNED_STACK 0
#define HAVE_ALTIVEC_H 0
#define HAVE_BIGENDIAN 0
#define HAVE_BSWAP 1
#define HAVE_CMOV 1
#define HAVE_EBP_AVAILABLE 1
#define HAVE_EBX_AVAILABLE 1
#define HAVE_FAST_CLZ 0
#define HAVE_FAST_CMOV 1
#define HAVE_FAST_UNALIGNED 1
#define HAVE_GETSYSTEMINFO 1
#define HAVE_ISATTY 0
#define HAVE_LOCAL_ALIGNED_16 1
#define HAVE_LOCAL_ALIGNED_8 1
#define HAVE_MALLOC_H 1
#define HAVE_MEMALIGN 1
#define HAVE_PTHREADS 0
#define HAVE_SCHED_GETAFFINITY 0
#define HAVE_SYMVER 1
#define HAVE_SYMVER_GNU_ASM 0
#define HAVE_SYMVER_ASM_LABEL 1
#define HAVE_SYSCTL 0
#define HAVE_TEN_OPERANDS 1
#define HAVE_THREADS 1
#define HAVE_VIRTUALALLOC 1
#define HAVE_W32THREADS 1
#define HAVE_XMM_CLOBBERS 1
#define HAVE_YASM 1

#ifdef __GNUC__
  #define HAVE_ATTRIBUTE_PACKED 1
  #define HAVE_ATTRIBUTE_MAY_ALIAS 1
#else
  #define HAVE_ATTRIBUTE_PACKED 0
  #define HAVE_ATTRIBUTE_MAY_ALIAS 0
  #define EMULATE_FAST_INT
#endif

#ifdef __GNUC__
  #define HAVE_EXP2 1
  #define HAVE_EXP2F 1
  #define HAVE_LLRINT 1
  #define HAVE_LLRINTF 1
  #define HAVE_LOG2 1
  #define HAVE_LOG2F 1
  #define HAVE_LRINT 1
  #define HAVE_LRINTF 1
  #define HAVE_ROUND 1
  #define HAVE_ROUNDF 1
  #define HAVE_TRUNC 1
  #define HAVE_TRUNCF 1
#else
  #define HAVE_EXP2 1
  #define HAVE_EXP2F 1
  #define HAVE_LLRINT 0
  #define HAVE_LLRINTF 0
  #define HAVE_LOG2 1
  #define HAVE_LOG2F 1
  #define HAVE_LRINT 0
  #define HAVE_LRINTF 0
  #define HAVE_ROUND 0
  #define HAVE_ROUNDF 1
  #define HAVE_TRUNC 1
  #define HAVE_TRUNCF 1
  #define rint(x) (int)(x+0.5)
  #define cbrtf(x) pow((float)x, (float)1.0/3)
#endif

#define CONFIG_AC3ENC_FLOAT 0
#define CONFIG_AUDIO_FLOAT 1
#define CONFIG_DCT 1
#define CONFIG_DWT 0
#define CONFIG_GPL 1
#define CONFIG_GRAY 1
#define CONFIG_H264CHROMA 1
#define CONFIG_H264DSP 1
#define CONFIG_H264PRED 1
#define CONFIG_HARDCODED_TABLES 0
#define CONFIG_HUFFMAN 1
#define CONFIG_LIBAMR_NB 1
#define CONFIG_LIBXVID 0
#define CONFIG_LPC 0
#define CONFIG_MDCT 1
#define CONFIG_MLIB 0
#define CONFIG_MPEGAUDIO_HP 1
#define CONFIG_RDFT 1
#define CONFIG_RUNTIME_CPUDETECT 1
#define CONFIG_SMALL 0
#define CONFIG_ZLIB 1

#define CONFIG_DECODERS 1
#define CONFIG_ENCODERS 1
#define CONFIG_SWSCALE 1
#define CONFIG_SWSCALE_ALPHA 1
#define CONFIG_POSTPROC 1

/*
Note: when adding a new codec, you have to:
1)  Add a
    #define CONFIG_<codec suffix>_<ENCODER|DECODER|PARSER>
    depending on the type of codec you are adding
2)  Add a
    REGISTER_<ENCODER|DECODER|PARSER> (<codec suffix>, <codec suffix lowercase>);
    line to libavcodec/allcodecs.c
3)  Define the codec into ffcodecs.h :
    CODEC_OP(CODEC_ID_<codec suffix>, <unique id>, "<codec description">)
*/

#define CONFIG_AASC_DECODER 1
#define CONFIG_AMV_DECODER 1
#define CONFIG_ASV1_DECODER 1
#define CONFIG_ASV2_DECODER 1
#define CONFIG_AVS_DECODER 1
#define CONFIG_CAVS_DECODER 1
#define CONFIG_CINEPAK_DECODER 1
#define CONFIG_CSCD_DECODER 1
#define CONFIG_CYUV_DECODER 1
#define CONFIG_DVVIDEO_DECODER 1
#define CONFIG_EIGHTBPS_DECODER 1
#define CONFIG_FFV1_DECODER 1
#define CONFIG_FFVHUFF_DECODER 1
#define CONFIG_FLV_DECODER 1
#define CONFIG_FRAPS_DECODER 1
#define CONFIG_H261_DECODER 1
#define CONFIG_H263_DECODER 1
#define CONFIG_H263I_DECODER 1
#define CONFIG_H264_DECODER 1
#define CONFIG_HUFFYUV_DECODER 1
#define CONFIG_INDEO2_DECODER 1

#if defined (__INTEL_COMPILER)
#define CONFIG_INDEO3_DECODER 0
#else
#define CONFIG_INDEO3_DECODER 1
#endif

#define CONFIG_INDEO4_DECODER 1
#define CONFIG_INDEO5_DECODER 1
#define CONFIG_JPEGLS_DECODER 1
#define CONFIG_LOCO_DECODER 1
#define CONFIG_MJPEG_DECODER 1
#define CONFIG_MJPEGB_DECODER 0
#define CONFIG_MPEG1VIDEO_DECODER 1
#define CONFIG_MPEG2VIDEO_DECODER 1
#define CONFIG_MPEG4_DECODER 1
#define CONFIG_MSMPEG4V1_DECODER 1
#define CONFIG_MSMPEG4V2_DECODER 1
#define CONFIG_MSMPEG4V3_DECODER 1
#define CONFIG_MSRLE_DECODER 1
#define CONFIG_MSVIDEO1_DECODER 1
#define CONFIG_MSZH_DECODER 1
#define CONFIG_PNG_DECODER 1
#define CONFIG_QPEG_DECODER 1
#define CONFIG_QTRLE_DECODER 1
#define CONFIG_RPZA_DECODER 1
#define CONFIG_RV10_DECODER 1
#define CONFIG_RV20_DECODER 1
#define CONFIG_RV30_DECODER 1
#define CONFIG_RV40_DECODER 1
#define CONFIG_SNOW_DECODER 0
#define CONFIG_SP5X_DECODER 1
#define CONFIG_SVQ1_DECODER 1
#define CONFIG_SVQ3_DECODER 1
#define CONFIG_THEORA_DECODER 1
#define CONFIG_TRUEMOTION1_DECODER 1
#define CONFIG_TRUEMOTION2_DECODER 1
#define CONFIG_TSCC_DECODER 1
#define CONFIG_ULTI_DECODER 1
#define CONFIG_VC1_DECODER 1
#define CONFIG_VC1IMAGE_DECODER 1
#define CONFIG_VCR1_DECODER 1
#define CONFIG_VP3_DECODER 1
#define CONFIG_VP5_DECODER 1
#define CONFIG_VP6_DECODER 1
#define CONFIG_VP6A_DECODER 1
#define CONFIG_VP6F_DECODER 1
#define CONFIG_VP8_DECODER 1
#define CONFIG_WMV1_DECODER 1
#define CONFIG_WMV2_DECODER 1
#define CONFIG_WMV3_DECODER 1
#define CONFIG_WMV3IMAGE_DECODER 1
#define CONFIG_WNV1_DECODER 1
#define CONFIG_XL_DECODER 1
#define CONFIG_ZLIB_DECODER 1
#define CONFIG_ZMBV_DECODER 1

#define CONFIG_AAC_DECODER 1
#define CONFIG_AAC_LATM_DECODER 1
#define CONFIG_AC3_DECODER 1
#define CONFIG_AMR_NB_DECODER 1
#define CONFIG_AMR_WB_DECODER 1
#define CONFIG_ATRAC3_DECODER 1
#define CONFIG_COOK_DECODER 1
#define CONFIG_DCA_DECODER 1
#define CONFIG_EAC3_DECODER 1
#define CONFIG_FLAC_DECODER 1
#define CONFIG_GSM_DECODER 1
#define CONFIG_GSM_MS_DECODER 1
#define CONFIG_IMC_DECODER 1
#define CONFIG_MACE3_DECODER 1
#define CONFIG_MACE6_DECODER 1
#define CONFIG_MLP_DECODER 1
#define CONFIG_MP1_DECODER 0
#define CONFIG_MP1FLOAT_DECODER 1
#define CONFIG_MP2_DECODER 0
#define CONFIG_MP2FLOAT_DECODER 1
#define CONFIG_MP3_DECODER 0
#define CONFIG_MP3FLOAT_DECODER 1
#define CONFIG_NELLYMOSER_DECODER 1
#define CONFIG_QDM2_DECODER 1
#define CONFIG_RA_144_DECODER 1
#define CONFIG_RA_288_DECODER 1
#define CONFIG_TRUEHD_DECODER 1
#define CONFIG_TRUESPEECH_DECODER 1
#define CONFIG_TTA_DECODER 1
#define CONFIG_VORBIS_DECODER 1
#define CONFIG_WAVPACK_DECODER 1
#define CONFIG_WMAV1_DECODER 1
#define CONFIG_WMAV2_DECODER 1
#define CONFIG_PCM_ALAW_DECODER 1
#define CONFIG_PCM_MULAW_DECODER 1
#define CONFIG_ADPCM_4XM_DECODER 1
#define CONFIG_ADPCM_ADX_DECODER 1
#define CONFIG_ADPCM_CT_DECODER 1
#define CONFIG_ADPCM_EA_DECODER 1
#define CONFIG_ADPCM_G726_DECODER 1
#define CONFIG_ADPCM_IMA_AMV_DECODER 1
#define CONFIG_ADPCM_IMA_DK3_DECODER 1
#define CONFIG_ADPCM_IMA_DK4_DECODER 1
#define CONFIG_ADPCM_IMA_QT_DECODER 1
#define CONFIG_ADPCM_IMA_SMJPEG_DECODER 1
#define CONFIG_ADPCM_IMA_WAV_DECODER 1
#define CONFIG_ADPCM_IMA_WS_DECODER 1
#define CONFIG_ADPCM_MS_DECODER 1
#define CONFIG_ADPCM_SBPRO_2_DECODER 1
#define CONFIG_ADPCM_SBPRO_3_DECODER 1
#define CONFIG_ADPCM_SBPRO_4_DECODER 1
#define CONFIG_ADPCM_SWF_DECODER 1
#define CONFIG_ADPCM_XA_DECODER 1
#define CONFIG_ADPCM_YAMAHA_DECODER 1

#define CONFIG_LIBAMR_NB_DECODER 1

#define CONFIG_DVVIDEO_ENCODER 1
#define CONFIG_FFV1_ENCODER 1
#define CONFIG_FFVHUFF_ENCODER 1
#define CONFIG_FLV_ENCODER 0
#define CONFIG_H261_ENCODER 0
#define CONFIG_H263_ENCODER 0
#define CONFIG_H263P_ENCODER 0
#define CONFIG_H264_ENCODER 0
#define CONFIG_HUFFYUV_ENCODER 0
#define CONFIG_LJPEG_ENCODER 0
#define CONFIG_MJPEG_ENCODER 1
#define CONFIG_MPEG1VIDEO_ENCODER 0
#define CONFIG_MPEG2VIDEO_ENCODER 0
#define CONFIG_MPEG4_ENCODER 0
#define CONFIG_MSMPEG4V1_ENCODER 0
#define CONFIG_MSMPEG4V2_ENCODER 0
#define CONFIG_MSMPEG4V3_ENCODER 0
#define CONFIG_PNG_ENCODER 1
#define CONFIG_RV10_ENCODER 0
#define CONFIG_RV20_ENCODER 0
#define CONFIG_SNOW_ENCODER 0
#define CONFIG_WMV1_ENCODER 0
#define CONFIG_WMV2_ENCODER 0

#define CONFIG_AC3_ENCODER 0
#define CONFIG_AC3_FIXED_ENCODER 1
#define CONFIG_EAC3_ENCODER 0

#define CONFIG_AAC_PARSER 0
#define CONFIG_AC3_PARSER 1
#define CONFIG_DCA_PARSER 1
#define CONFIG_H263_PARSER 1
#define CONFIG_H264_PARSER 1
#define CONFIG_MJPEG_PARSER 1
#define CONFIG_MPEGAUDIO_PARSER 1
#define CONFIG_MPEG4VIDEO_PARSER 1
#define CONFIG_MLP_PARSER 1
#define CONFIG_MPEGVIDEO_PARSER 1

#define CONFIG_MPEG_XVMC_DECODER 0
#define CONFIG_VC1_VDPAU_DECODER 0

#define CONFIG_CUSTOM_FFDSHOW_H264 1
#define CONFIG_CUSTOM_FFMPEG_H264 1

#endif /* FFMPEG_CONFIG_H */
