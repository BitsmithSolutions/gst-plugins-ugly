/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "string.h"
#include "gstlame.h"

#ifdef lame_set_preset
#define GST_LAME_PRESET
#endif

GST_DEBUG_CATEGORY_STATIC (debug);
#define GST_CAT_DEFAULT debug

/* elementfactory information */
static GstElementDetails gst_lame_details = {
  "L.A.M.E. mp3 encoder",
  "Codec/Encoder/Audio",
  "High-quality free MP3 encoder",
  "Erik Walthinsen <omega@cse.ogi.edu>",
};

/* LAME can do MPEG-1, MPEG-2, and MPEG-2.5, so it has 9 possible
 * sample rates it supports */
static GstStaticPadTemplate gst_lame_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "endianness = (int) " G_STRINGIFY (G_BYTE_ORDER) ", "
        "signed = (boolean) true, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ]")
    );

static GstStaticPadTemplate gst_lame_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, "
        "mpegversion = (int) 1, "
        "layer = (int) 3, "
        "rate = (int) { 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ]")
    );

/********** Define useful types for non-programmatic interfaces **********/
#define GST_TYPE_LAME_MODE (gst_lame_mode_get_type())
static GType
gst_lame_mode_get_type (void)
{
  static GType lame_mode_type = 0;
  static GEnumValue lame_modes[] = {
    {0, "0", "Stereo"},
    {1, "1", "Joint-Stereo"},
    {2, "2", "Dual channel"},
    {3, "3", "Mono"},
    {4, "4", "Auto"},
    {0, NULL, NULL}
  };

  if (!lame_mode_type) {
    lame_mode_type = g_enum_register_static ("GstLameMode", lame_modes);
  }
  return lame_mode_type;
}

#define GST_TYPE_LAME_QUALITY (gst_lame_quality_get_type())
static GType
gst_lame_quality_get_type (void)
{
  static GType lame_quality_type = 0;
  static GEnumValue lame_quality[] = {
    {0, "0", "0 - Best"},
    {1, "1", "1"},
    {2, "2", "2"},
    {3, "3", "3"},
    {4, "4", "4"},
    {5, "5", "5 - Default"},
    {6, "6", "6"},
    {7, "7", "7"},
    {8, "8", "8"},
    {9, "9", "9 - Worst"},
    {0, NULL, NULL}
  };

  if (!lame_quality_type) {
    lame_quality_type = g_enum_register_static ("GstLameQuality", lame_quality);
  }
  return lame_quality_type;
}

#define GST_TYPE_LAME_PADDING (gst_lame_padding_get_type())
static GType
gst_lame_padding_get_type (void)
{
  static GType lame_padding_type = 0;
  static GEnumValue lame_padding[] = {
    {0, "0", "No Padding"},
    {1, "1", "Always Pad"},
    {2, "2", "Adjust Padding"},
    {0, NULL, NULL}
  };

  if (!lame_padding_type) {
    lame_padding_type = g_enum_register_static ("GstLamePadding", lame_padding);
  }
  return lame_padding_type;
}

#define GST_TYPE_LAME_VBRMODE (gst_lame_vbrmode_get_type())
static GType
gst_lame_vbrmode_get_type (void)
{
  static GType lame_vbrmode_type = 0;
  static GEnumValue lame_vbrmode[] = {
    {vbr_off, "cbr", "No VBR (Constant Bitrate)"},
    {vbr_rh, "old", "Lame's old VBR algorithm"},
    {vbr_abr, "abr", "VBR Average Bitrate"},
    {vbr_mtrh, "new", "Lame's new VBR algorithm"},
    {0, NULL, NULL}
  };

  if (!lame_vbrmode_type) {
    lame_vbrmode_type = g_enum_register_static ("GstLameVbrmode", lame_vbrmode);
  }

  return lame_vbrmode_type;
}

#ifdef GSTLAME_PRESET
#define GST_TYPE_LAME_PRESET (gst_lame_preset_get_type())
static GType
gst_lame_preset_get_type (void)
{
  static GType gst_lame_preset = 0;
  static GEnumValue gst_lame_presets[] = {
    {0, "none", "None"},
    {MEDIUM, "medium", "Medium"},
    {STANDARD, "standard", "Standard"},
    {EXTREME, "extreme", "Extreme"},
    {INSANE, "insane", "Insane"},
    {0, NULL, NULL}
  };

  if (!gst_lame_preset) {
    gst_lame_preset =
        g_enum_register_static ("GstLamePreset", gst_lame_presets);
  }

  return gst_lame_preset;
}
#endif

/********** Standard stuff for signals and arguments **********/
/* GstLame signals and args */
enum
{
  /* FILL_ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_BITRATE,
  ARG_COMPRESSION_RATIO,
  ARG_QUALITY,
  ARG_MODE,
  ARG_FORCE_MS,
  ARG_FREE_FORMAT,
  ARG_COPYRIGHT,
  ARG_ORIGINAL,
  ARG_ERROR_PROTECTION,
  ARG_PADDING_TYPE,
  ARG_EXTENSION,
  ARG_STRICT_ISO,
  ARG_DISABLE_RESERVOIR,
  ARG_VBR,
  ARG_VBR_MEAN_BITRATE,
  ARG_VBR_MIN_BITRATE,
  ARG_VBR_MAX_BITRATE,
  ARG_VBR_HARD_MIN,
  ARG_LOWPASS_FREQ,
  ARG_LOWPASS_WIDTH,
  ARG_HIGHPASS_FREQ,
  ARG_HIGHPASS_WIDTH,
  ARG_ATH_ONLY,
  ARG_ATH_SHORT,
  ARG_NO_ATH,
  ARG_ATH_LOWER,
  ARG_CWLIMIT,
  ARG_ALLOW_DIFF_SHORT,
  ARG_NO_SHORT_BLOCKS,
  ARG_EMPHASIS,
  ARG_VBR_QUALITY,
#ifdef GSTLAME_PRESET
  ARG_XINGHEADER,
  ARG_PRESET
#else
  ARG_XINGHEADER
#endif
};

static void gst_lame_base_init (gpointer g_class);
static void gst_lame_class_init (GstLameClass * klass);
static void gst_lame_init (GstLame * gst_lame);

static void gst_lame_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_lame_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_lame_chain (GstPad * pad, GstData * _data);
static gboolean gst_lame_setup (GstLame * lame);
static GstElementStateReturn gst_lame_change_state (GstElement * element);

static GstElementClass *parent_class = NULL;

/* static guint gst_lame_signals[LAST_SIGNAL] = { 0 }; */

GType
gst_lame_get_type (void)
{
  static GType gst_lame_type = 0;

  if (!gst_lame_type) {
    static const GTypeInfo gst_lame_info = {
      sizeof (GstLameClass),
      gst_lame_base_init,
      NULL,
      (GClassInitFunc) gst_lame_class_init,
      NULL,
      NULL,
      sizeof (GstLame),
      0,
      (GInstanceInitFunc) gst_lame_init,
    };

    static const GInterfaceInfo tag_setter_info = {
      NULL,
      NULL,
      NULL
    };

    gst_lame_type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstLame", &gst_lame_info, 0);
    g_type_add_interface_static (gst_lame_type, GST_TYPE_TAG_SETTER,
        &tag_setter_info);

  }
  return gst_lame_type;
}

static void
gst_lame_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_lame_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_lame_sink_template));
  gst_element_class_set_details (element_class, &gst_lame_details);
}

static void
gst_lame_class_init (GstLameClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_ref (GST_TYPE_ELEMENT);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_BITRATE,
      g_param_spec_int ("bitrate", "Bitrate (kb/s)", "Bitrate in kbit/sec",
          8, 320, 128, G_PARAM_READWRITE));
  /* compression ratio set to 0.0 by default otherwise it overrides the bitrate setting */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_COMPRESSION_RATIO, g_param_spec_float ("compression_ratio",
          "Compression Ratio",
          "let lame choose bitrate to achieve selected compression ratio", 0.0,
          200.0, 0.0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_QUALITY,
      g_param_spec_enum ("quality", "Quality",
          "Quality of algorithm used for encoding", GST_TYPE_LAME_QUALITY, 5,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MODE,
      g_param_spec_enum ("mode", "Mode", "Encoding mode", GST_TYPE_LAME_MODE, 0,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_FORCE_MS,
      g_param_spec_boolean ("force_ms", "Force ms",
          "Force ms_stereo on all frames", TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_FREE_FORMAT,
      g_param_spec_boolean ("free_format", "Free format",
          "Produce a free format bitstream", TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_COPYRIGHT,
      g_param_spec_boolean ("copyright", "Copyright", "Mark as copyright", TRUE,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ORIGINAL,
      g_param_spec_boolean ("original", "Original", "Mark as non-original",
          TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ERROR_PROTECTION,
      g_param_spec_boolean ("error_protection", "Error protection",
          "Adds 16 bit checksum to every frame", TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_PADDING_TYPE,
      g_param_spec_enum ("padding_type", "Padding type", "Padding type",
          GST_TYPE_LAME_PADDING, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_EXTENSION,
      g_param_spec_boolean ("extension", "Extension", "Extension", TRUE,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_STRICT_ISO,
      g_param_spec_boolean ("strict_iso", "Strict ISO",
          "Comply as much as possible to ISO MPEG spec", TRUE,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      ARG_DISABLE_RESERVOIR, g_param_spec_boolean ("disable_reservoir",
          "Disable reservoir", "Disable the bit reservoir", TRUE,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_VBR,
      g_param_spec_enum ("vbr", "VBR", "Specify bitrate mode",
          GST_TYPE_LAME_VBRMODE, vbr_off, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_VBR_QUALITY,
      g_param_spec_enum ("vbr_quality", "VBR Quality", "VBR Quality",
          GST_TYPE_LAME_QUALITY, 5, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_VBR_MEAN_BITRATE,
      g_param_spec_int ("vbr_mean_bitrate", "VBR mean bitrate",
          "Specify mean bitrate", 0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_VBR_MIN_BITRATE,
      g_param_spec_int ("vbr_min_bitrate", "VBR min bitrate",
          "Specify min bitrate", 0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_VBR_MAX_BITRATE,
      g_param_spec_int ("vbr_max_bitrate", "VBR max bitrate",
          "Specify max bitrate", 0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_VBR_HARD_MIN,
      g_param_spec_int ("vbr_hard_min", "VBR hard min",
          "Specify hard min bitrate", 0, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_LOWPASS_FREQ,
      g_param_spec_int ("lowpass_freq", "Lowpass freq",
          "frequency(kHz), lowpass filter cutoff above freq", 0, 50000, 0,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_LOWPASS_WIDTH,
      g_param_spec_int ("lowpass_width", "Lowpass width",
          "frequency(kHz) - default 15% of lowpass freq", 0, G_MAXINT, 0,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_HIGHPASS_FREQ,
      g_param_spec_int ("highpass_freq", "Highpass freq",
          "frequency(kHz), highpass filter cutoff below freq", 0, 50000, 0,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_HIGHPASS_WIDTH,
      g_param_spec_int ("highpass_width", "Highpass width",
          "frequency(kHz) - default 15% of highpass freq", 0, G_MAXINT, 0,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ATH_ONLY,
      g_param_spec_boolean ("ath_only", "ATH only",
          "Ignore GPSYCHO completely, use ATH only", TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ATH_SHORT,
      g_param_spec_boolean ("ath_short", "ATH short",
          "Ignore GPSYCHO for short blocks, use ATH only", TRUE,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_NO_ATH,
      g_param_spec_boolean ("no_ath", "No ath",
          "turns ATH down to a flat noise floor", TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ATH_LOWER,
      g_param_spec_int ("ath_lower", "ATH lower", "lowers ATH by x dB",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_CWLIMIT,
      g_param_spec_int ("cwlimit", "Cwlimit",
          "Compute tonality up to freq (in kHz) default 8.8717", 0, 50000, 0,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_ALLOW_DIFF_SHORT,
      g_param_spec_boolean ("allow_diff_short", "Allow diff short",
          "Allow diff short", TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_NO_SHORT_BLOCKS,
      g_param_spec_boolean ("no_short_blocks", "No short blocks",
          "Do not use short blocks", TRUE, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_EMPHASIS,
      g_param_spec_boolean ("emphasis", "Emphasis", "Emphasis", TRUE,
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_XINGHEADER,
      g_param_spec_boolean ("xingheader", "Output Xing Header",
          "Output Xing Header", FALSE, G_PARAM_READWRITE));
#ifdef GSTLAME_PRESET
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_PRESET,
      g_param_spec_enum ("preset", "Lame Preset", "Lame Preset",
          GST_TYPE_LAME_PRESET, 0, G_PARAM_READWRITE));
#endif
  gobject_class->set_property = gst_lame_set_property;
  gobject_class->get_property = gst_lame_get_property;

  gstelement_class->change_state = gst_lame_change_state;
}

static GstCaps *
gst_lame_src_getcaps (GstPad * pad)
{
  GstLame *lame;
  GstCaps *caps;

  lame = GST_LAME (gst_pad_get_parent (pad));

  if (!gst_lame_setup (lame)) {
    GST_DEBUG_OBJECT (lame, "problem doing lame setup");
    return
        gst_caps_copy (gst_pad_template_get_caps (gst_static_pad_template_get
            (&gst_lame_src_template)));
  }

  caps = gst_caps_new_simple ("audio/mpeg",
      "mpegversion", G_TYPE_INT, 1,
      "layer", G_TYPE_INT, 3,
      "rate", G_TYPE_INT, lame_get_out_samplerate (lame->lgf),
      "channels", G_TYPE_INT, lame->num_channels, NULL);

  return caps;
}

static GstPadLinkReturn
gst_lame_src_link (GstPad * pad, const GstCaps * caps)
{
  GstLame *lame;
  gint out_samplerate;
  GstStructure *structure;
  GstCaps *othercaps, *channelcaps;
  GstPadLinkReturn result;

  lame = GST_LAME (gst_pad_get_parent (pad));
  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "rate", &out_samplerate) ||
      !gst_structure_get_int (structure, "channels", &lame->num_channels))
    g_return_val_if_reached (GST_PAD_LINK_REFUSED);

  if (lame_set_out_samplerate (lame->lgf, out_samplerate) != 0)
    return GST_PAD_LINK_REFUSED;

  /* we don't do channel conversion */
  channelcaps = gst_caps_new_simple ("audio/x-raw-int", "channels", G_TYPE_INT,
      lame->num_channels, NULL);
  othercaps = gst_caps_intersect (gst_pad_get_pad_template_caps (lame->sinkpad),
      channelcaps);
  gst_caps_free (channelcaps);

  result = gst_pad_try_set_caps_nonfixed (lame->sinkpad, othercaps);

  if (GST_PAD_LINK_FAILED (result))
    return result;

  caps = gst_pad_get_negotiated_caps (lame->sinkpad);
  structure = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (structure, "rate", &lame->samplerate))
    g_return_val_if_reached (GST_PAD_LINK_REFUSED);

  if (!gst_lame_setup (lame)) {
    GST_ELEMENT_ERROR (lame, CORE, NEGOTIATION, (NULL),
        ("could not initialize encoder (wrong parameters?)"));
    return GST_PAD_LINK_REFUSED;
  }

  return result;
}

static GstPadLinkReturn
gst_lame_sink_link (GstPad * pad, const GstCaps * caps)
{
  GstLame *lame;
  gint out_samplerate;
  GstStructure *structure;
  GstCaps *othercaps;

  lame = GST_LAME (gst_pad_get_parent (pad));
  structure = gst_caps_get_structure (caps, 0);

  if (!gst_structure_get_int (structure, "rate", &lame->samplerate) ||
      !gst_structure_get_int (structure, "channels", &lame->num_channels))
    g_return_val_if_reached (GST_PAD_LINK_REFUSED);

  lame_set_out_samplerate (lame->lgf, 0);
  if (!gst_lame_setup (lame)) {
    GST_ELEMENT_ERROR (lame, CORE, NEGOTIATION, (NULL),
        ("could not initialize encoder (wrong parameters?)"));
    return GST_PAD_LINK_REFUSED;
  }

  out_samplerate = lame_get_out_samplerate (lame->lgf);
  othercaps =
      gst_caps_new_simple ("audio/mpeg",
      "mpegversion", G_TYPE_INT, 1,
      "layer", G_TYPE_INT, 3,
      "channels", G_TYPE_INT, lame->num_channels,
      "rate", G_TYPE_INT, out_samplerate, NULL);

  return gst_pad_try_set_caps (lame->srcpad, othercaps);
}

static void
gst_lame_init (GstLame * lame)
{
  GST_DEBUG_OBJECT (lame, "starting initialization");

  lame->sinkpad =
      gst_pad_new_from_template (gst_static_pad_template_get
      (&gst_lame_sink_template), "sink");
  gst_element_add_pad (GST_ELEMENT (lame), lame->sinkpad);
  gst_pad_set_chain_function (lame->sinkpad, gst_lame_chain);
  gst_pad_set_link_function (lame->sinkpad, gst_lame_sink_link);

  lame->srcpad =
      gst_pad_new_from_template (gst_static_pad_template_get
      (&gst_lame_src_template), "src");
  gst_element_add_pad (GST_ELEMENT (lame), lame->srcpad);
  gst_pad_set_link_function (lame->srcpad, gst_lame_src_link);
  gst_pad_set_getcaps_function (lame->srcpad, gst_lame_src_getcaps);
  GST_FLAG_SET (lame, GST_ELEMENT_EVENT_AWARE);

  GST_DEBUG ("setting up lame encoder");
  lame->lgf = lame_init ();

  lame->samplerate = 44100;
  lame->num_channels = 2;
  lame->initialized = FALSE;

  lame->bitrate = 128;          /* lame_get_brate (lame->lgf); => 0/out of range */
  lame->compression_ratio = 0.0;        /* lame_get_compression_ratio (lame->lgf); => 0/out of range ... NOTE: 0.0 makes bitrate take precedence */
  lame->quality = 5;            /* lame_get_quality (lame->lgf); => -1/out of range */
  lame->mode = lame_get_mode (lame->lgf);
  lame->force_ms = lame_get_force_ms (lame->lgf);
  lame->free_format = lame_get_free_format (lame->lgf);
  lame->copyright = lame_get_copyright (lame->lgf);
  lame->original = lame_get_original (lame->lgf);
  lame->error_protection = lame_get_error_protection (lame->lgf);
  lame->padding_type = lame_get_padding_type (lame->lgf);
  lame->extension = lame_get_extension (lame->lgf);
  lame->strict_iso = lame_get_strict_ISO (lame->lgf);
  lame->disable_reservoir = lame_get_disable_reservoir (lame->lgf);
  lame->vbr = vbr_off;          /* lame_get_VBR (lame->lgf); */
  lame->vbr_quality = 5;
  lame->vbr_mean_bitrate = lame_get_VBR_mean_bitrate_kbps (lame->lgf);
  lame->vbr_min_bitrate = lame_get_VBR_min_bitrate_kbps (lame->lgf);
  lame->vbr_max_bitrate = 320;  /* lame_get_VBR_max_bitrate_kbps (lame->lgf); => 0/no vbr possible */
  lame->vbr_hard_min = lame_get_VBR_hard_min (lame->lgf);
  //lame->lowpass_freq = 50000;   /* lame_get_lowpassfreq (lame->lgf); => 0/lowpass on everything ? */
  lame->lowpass_freq = 0;
  lame->lowpass_width = 0;      /* lame_get_lowpasswidth (lame->lgf); => -1/out of range */
  lame->highpass_freq = lame_get_highpassfreq (lame->lgf);
  lame->highpass_width = 0;     /* lame_get_highpasswidth (lame->lgf); => -1/out of range */
  lame->ath_only = lame_get_ATHonly (lame->lgf);
  lame->ath_short = lame_get_ATHshort (lame->lgf);
  lame->no_ath = lame_get_noATH (lame->lgf);
  /*  lame->ath_type = lame_get_ATHtype (lame->lgf); */
  lame->ath_lower = lame_get_ATHlower (lame->lgf);
  lame->cwlimit = 8.8717;       /* lame_get_cwlimit (lame->lgf); => 0 */
  lame->allow_diff_short = lame_get_allow_diff_short (lame->lgf);
  lame->no_short_blocks = TRUE; /* lame_get_no_short_blocks (lame->lgf); */
  lame->emphasis = lame_get_emphasis (lame->lgf);
  lame->xingheader = FALSE;
  lame->preset = 0;
  lame->tags = gst_tag_list_new ();

  id3tag_init (lame->lgf);

  lame->newmediacount = 0;
  GST_DEBUG_OBJECT (lame, "done initializing");
}

typedef struct _GstLameTagMatch GstLameTagMatch;
typedef void (*GstLameTagFunc) (lame_global_flags * gfp, const char *value);

struct _GstLameTagMatch
{
  gchar *gstreamer_tag;
  GstLameTagFunc tag_func;
};

static GstLameTagMatch tag_matches[] = {
  {GST_TAG_TITLE, id3tag_set_title},
  {GST_TAG_DATE, id3tag_set_year},
  {GST_TAG_TRACK_NUMBER, id3tag_set_track},
  {GST_TAG_COMMENT, id3tag_set_comment},
  {GST_TAG_ARTIST, id3tag_set_artist},
  {GST_TAG_ALBUM, id3tag_set_album},
  {GST_TAG_GENRE, (GstLameTagFunc) id3tag_set_genre},
  {NULL, NULL}
};

static void
add_one_tag (const GstTagList * list, const gchar * tag, gpointer user_data)
{
  GstLame *lame;
  gchar *value;
  int i = 0;

  lame = GST_LAME (user_data);
  g_return_if_fail (lame != NULL);

  while (tag_matches[i].gstreamer_tag != NULL) {
    if (strcmp (tag, tag_matches[i].gstreamer_tag) == 0) {
      break;
    }
    i++;
  }

  if (tag_matches[i].tag_func == NULL) {
    g_print ("Couldn't find matching gstreamer tag for %s\n", tag);
    return;
  }

  switch (gst_tag_get_type (tag)) {
    case G_TYPE_UINT:{
      guint ivalue;

      if (!gst_tag_list_get_uint (list, tag, &ivalue)) {
        GST_DEBUG ("Error reading \"%s\" tag value\n", tag);
        return;
      }

      if (strcmp (tag, GST_TAG_DATE) == 0) {
        GDate *date = g_date_new_julian ((guint32) ivalue);

        ivalue = g_date_get_year (date);
        g_date_free (date);
      }
      value = g_strdup_printf ("%u", ivalue);
      break;
    }
    case G_TYPE_STRING:
      if (!gst_tag_list_get_string (list, tag, &value)) {
        GST_DEBUG ("Error reading \"%s\" tag value\n", tag);
        return;
      };
      break;
    default:
      GST_DEBUG ("Couldn't write tag %s", tag);
      break;
  }

  tag_matches[i].tag_func (lame->lgf, value);

  if (gst_tag_get_type (tag) == G_TYPE_UINT) {
    g_free (value);
  }
}

static void
gst_lame_set_metadata (GstLame * lame)
{
  const GstTagList *user_tags;
  GstTagList *copy;

  g_return_if_fail (lame != NULL);
  user_tags = gst_tag_setter_get_list (GST_TAG_SETTER (lame));
  if ((lame->tags == NULL) && (user_tags == NULL)) {
    return;
  }
  copy = gst_tag_list_merge (user_tags, lame->tags,
      gst_tag_setter_get_merge_mode (GST_TAG_SETTER (lame)));
  gst_tag_list_foreach ((GstTagList *) copy, add_one_tag, lame);

  gst_tag_list_free (copy);
}



static void
gst_lame_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstLame *lame;

  /* it's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_LAME (object));

  lame = GST_LAME (object);

  switch (prop_id) {
    case ARG_BITRATE:
      lame->bitrate = g_value_get_int (value);
      break;
    case ARG_COMPRESSION_RATIO:
      lame->compression_ratio = g_value_get_float (value);
      break;
    case ARG_QUALITY:
      lame->quality = g_value_get_enum (value);
      break;
    case ARG_MODE:
      lame->mode = g_value_get_enum (value);
      break;
    case ARG_FORCE_MS:
      lame->force_ms = g_value_get_boolean (value);
      break;
    case ARG_FREE_FORMAT:
      lame->free_format = g_value_get_boolean (value);
      break;
    case ARG_COPYRIGHT:
      lame->copyright = g_value_get_boolean (value);
      break;
    case ARG_ORIGINAL:
      lame->original = g_value_get_boolean (value);
      break;
    case ARG_ERROR_PROTECTION:
      lame->error_protection = g_value_get_boolean (value);
      break;
    case ARG_PADDING_TYPE:
      lame->padding_type = g_value_get_int (value);
      break;
    case ARG_EXTENSION:
      lame->extension = g_value_get_boolean (value);
      break;
    case ARG_STRICT_ISO:
      lame->strict_iso = g_value_get_boolean (value);
      break;
    case ARG_DISABLE_RESERVOIR:
      lame->disable_reservoir = g_value_get_boolean (value);
      break;
    case ARG_VBR:
      lame->vbr = g_value_get_enum (value);
      break;
    case ARG_VBR_QUALITY:
      lame->vbr_quality = g_value_get_enum (value);
      break;
    case ARG_VBR_MEAN_BITRATE:
      lame->vbr_mean_bitrate = g_value_get_int (value);
      break;
    case ARG_VBR_MIN_BITRATE:
      lame->vbr_min_bitrate = g_value_get_int (value);
      break;
    case ARG_VBR_MAX_BITRATE:
      lame->vbr_max_bitrate = g_value_get_int (value);
      break;
    case ARG_VBR_HARD_MIN:
      lame->vbr_hard_min = g_value_get_int (value);
      break;
    case ARG_LOWPASS_FREQ:
      lame->lowpass_freq = g_value_get_int (value);
      break;
    case ARG_LOWPASS_WIDTH:
      lame->lowpass_width = g_value_get_int (value);
      break;
    case ARG_HIGHPASS_FREQ:
      lame->highpass_freq = g_value_get_int (value);
      break;
    case ARG_HIGHPASS_WIDTH:
      lame->highpass_width = g_value_get_int (value);
      break;
    case ARG_ATH_ONLY:
      lame->ath_only = g_value_get_boolean (value);
      break;
    case ARG_ATH_SHORT:
      lame->ath_short = g_value_get_boolean (value);
      break;
    case ARG_NO_ATH:
      lame->no_ath = g_value_get_boolean (value);
      break;
    case ARG_ATH_LOWER:
      lame->ath_lower = g_value_get_int (value);
      break;
    case ARG_CWLIMIT:
      lame->cwlimit = g_value_get_int (value);
      break;
    case ARG_ALLOW_DIFF_SHORT:
      lame->allow_diff_short = g_value_get_boolean (value);
      break;
    case ARG_NO_SHORT_BLOCKS:
      lame->no_short_blocks = g_value_get_boolean (value);
      break;
    case ARG_EMPHASIS:
      lame->emphasis = g_value_get_boolean (value);
      break;
    case ARG_XINGHEADER:
      lame->xingheader = g_value_get_boolean (value);
      break;
#ifdef GSTLAME_PRESET
    case ARG_PRESET:
      lame->preset = g_value_get_enum (value);
      break;
#endif
    default:
      break;
  }

}

static void
gst_lame_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstLame *lame;

  /* it's not null if we got it, but it might not be ours */
  g_return_if_fail (GST_IS_LAME (object));

  lame = GST_LAME (object);

  switch (prop_id) {
    case ARG_BITRATE:
      g_value_set_int (value, lame->bitrate);
      break;
    case ARG_COMPRESSION_RATIO:
      g_value_set_float (value, lame->compression_ratio);
      break;
    case ARG_QUALITY:
      g_value_set_enum (value, lame->quality);
      break;
    case ARG_MODE:
      g_value_set_enum (value, lame->mode);
      break;
    case ARG_FORCE_MS:
      g_value_set_boolean (value, lame->force_ms);
      break;
    case ARG_FREE_FORMAT:
      g_value_set_boolean (value, lame->free_format);
      break;
    case ARG_COPYRIGHT:
      g_value_set_boolean (value, lame->copyright);
      break;
    case ARG_ORIGINAL:
      g_value_set_boolean (value, lame->original);
      break;
    case ARG_ERROR_PROTECTION:
      g_value_set_boolean (value, lame->error_protection);
      break;
    case ARG_PADDING_TYPE:
      g_value_set_enum (value, lame->padding_type);
      break;
    case ARG_EXTENSION:
      g_value_set_boolean (value, lame->extension);
      break;
    case ARG_STRICT_ISO:
      g_value_set_boolean (value, lame->strict_iso);
      break;
    case ARG_DISABLE_RESERVOIR:
      g_value_set_boolean (value, lame->disable_reservoir);
      break;
    case ARG_VBR:
      g_value_set_enum (value, lame->vbr);
      break;
    case ARG_VBR_QUALITY:
      g_value_set_enum (value, lame->vbr_quality);
      break;
    case ARG_VBR_MEAN_BITRATE:
      g_value_set_int (value, lame->vbr_mean_bitrate);
      break;
    case ARG_VBR_MIN_BITRATE:
      g_value_set_int (value, lame->vbr_min_bitrate);
      break;
    case ARG_VBR_MAX_BITRATE:
      g_value_set_int (value, lame->vbr_max_bitrate);
      break;
    case ARG_VBR_HARD_MIN:
      g_value_set_int (value, lame->vbr_hard_min);
      break;
    case ARG_LOWPASS_FREQ:
      g_value_set_int (value, lame->lowpass_freq);
      break;
    case ARG_LOWPASS_WIDTH:
      g_value_set_int (value, lame->lowpass_width);
      break;
    case ARG_HIGHPASS_FREQ:
      g_value_set_int (value, lame->highpass_freq);
      break;
    case ARG_HIGHPASS_WIDTH:
      g_value_set_int (value, lame->highpass_width);
      break;
    case ARG_ATH_ONLY:
      g_value_set_boolean (value, lame->ath_only);
      break;
    case ARG_ATH_SHORT:
      g_value_set_boolean (value, lame->ath_short);
      break;
    case ARG_NO_ATH:
      g_value_set_boolean (value, lame->no_ath);
      break;
    case ARG_ATH_LOWER:
      g_value_set_int (value, lame->ath_lower);
      break;
    case ARG_CWLIMIT:
      g_value_set_int (value, lame->cwlimit);
      break;
    case ARG_ALLOW_DIFF_SHORT:
      g_value_set_boolean (value, lame->allow_diff_short);
      break;
    case ARG_NO_SHORT_BLOCKS:
      g_value_set_boolean (value, lame->no_short_blocks);
      break;
    case ARG_EMPHASIS:
      g_value_set_boolean (value, lame->emphasis);
      break;
    case ARG_XINGHEADER:
      g_value_set_boolean (value, lame->xingheader);
      break;
#ifdef GSTLAME_PRESET
    case ARG_PRESET:
      g_value_set_enum (value, lame->preset);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_lame_chain (GstPad * pad, GstData * _data)
{
  GstBuffer *buf = GST_BUFFER (_data);
  GstLame *lame;
  GstBuffer *outbuf;
  gchar *mp3_data = NULL;
  gint mp3_buffer_size, mp3_size = 0;
  gboolean eos = FALSE;

  lame = GST_LAME (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (lame, "entered chain");

  if (GST_IS_EVENT (buf)) {
    switch (GST_EVENT_TYPE (buf)) {
      case GST_EVENT_EOS:
        GST_DEBUG_OBJECT (lame, "handling EOS event");
        eos = TRUE;
      case GST_EVENT_FLUSH:
        GST_DEBUG_OBJECT (lame, "handling FLUSH event");
        mp3_buffer_size = 7200;
        mp3_data = g_malloc (mp3_buffer_size);

        mp3_size = lame_encode_flush (lame->lgf, mp3_data, mp3_buffer_size);
        gst_event_unref (GST_EVENT (buf));
        break;
      case GST_EVENT_TAG:
        GST_DEBUG_OBJECT (lame, "handling TAG event");
        if (lame->tags) {
          gst_tag_list_insert (lame->tags,
              gst_event_tag_get_list (GST_EVENT (buf)),
              gst_tag_setter_get_merge_mode (GST_TAG_SETTER (lame)));
        } else {
          g_assert_not_reached ();
        }

        gst_pad_event_default (pad, GST_EVENT (buf));
        break;
      case GST_EVENT_DISCONTINUOUS:
        if (GST_EVENT_DISCONT_NEW_MEDIA (GST_EVENT (buf))) {
          /* do not re-initialise if it is first new media discont */
          if (lame->newmediacount++ > 0) {
            lame_close (lame->lgf);
            lame->lgf = lame_init ();
            lame->initialized = FALSE;
            lame->last_ts = GST_CLOCK_TIME_NONE;

            gst_lame_setup (lame);
          }
        }
        gst_pad_event_default (pad, GST_EVENT (buf));

        break;
      default:
        gst_pad_event_default (pad, GST_EVENT (buf));
        break;
    }
  } else {
    gint64 duration;

    if (!lame->initialized) {
      gst_buffer_unref (buf);
      GST_ELEMENT_ERROR (lame, CORE, NEGOTIATION, (NULL),
          ("encoder not initialized (input is not audio?)"));
      return;
    }

    /* allocate space for output */
    mp3_buffer_size =
        ((GST_BUFFER_SIZE (buf) / (2 + lame->num_channels)) * 1.25) + 7200;
    mp3_data = g_malloc (mp3_buffer_size);

    /* lame seems to be too stupid to get mono interleaved going */
    if (lame->num_channels == 1) {
      mp3_size = lame_encode_buffer (lame->lgf,
          (short int *) (GST_BUFFER_DATA (buf)),
          (short int *) (GST_BUFFER_DATA (buf)),
          GST_BUFFER_SIZE (buf) / 2, mp3_data, mp3_buffer_size);
    } else {
      mp3_size = lame_encode_buffer_interleaved (lame->lgf,
          (short int *) (GST_BUFFER_DATA (buf)),
          GST_BUFFER_SIZE (buf) / 2 / lame->num_channels,
          mp3_data, mp3_buffer_size);
    }

    GST_LOG_OBJECT (lame, "encoded %d bytes of audio to %d bytes of mp3",
        GST_BUFFER_SIZE (buf), mp3_size);

    duration = (GST_SECOND * GST_BUFFER_SIZE (buf) /
        (2 * lame->samplerate * lame->num_channels));

    if (GST_BUFFER_DURATION (buf) != GST_CLOCK_TIME_NONE &&
        GST_BUFFER_DURATION (buf) != duration)
      GST_DEBUG_OBJECT (lame, "incoming buffer had incorrect duration "
          GST_TIME_FORMAT "outgoing buffer will have correct duration "
          GST_TIME_FORMAT,
          GST_TIME_ARGS (GST_BUFFER_DURATION (buf)), GST_TIME_ARGS (duration));

    if (lame->last_ts == GST_CLOCK_TIME_NONE) {
      lame->last_ts = GST_BUFFER_TIMESTAMP (buf);
      lame->last_offs = GST_BUFFER_OFFSET (buf);
      lame->last_duration = duration;
    } else {
      lame->last_duration += duration;
    }

    gst_buffer_unref (buf);
  }

  if (mp3_size > 0) {
    outbuf = gst_buffer_new ();
    GST_BUFFER_DATA (outbuf) = mp3_data;
    GST_BUFFER_SIZE (outbuf) = mp3_size;
    GST_BUFFER_TIMESTAMP (outbuf) = lame->last_ts;
    GST_BUFFER_OFFSET (outbuf) = lame->last_offs;
    GST_BUFFER_DURATION (outbuf) = lame->last_duration;

    gst_pad_push (lame->srcpad, GST_DATA (outbuf));

    lame->last_ts = GST_CLOCK_TIME_NONE;
  } else {
    g_free (mp3_data);
  }

  if (eos) {
    gst_pad_push (lame->srcpad, GST_DATA (gst_event_new (GST_EVENT_EOS)));
    gst_element_set_eos (GST_ELEMENT (lame));
  }
}

/* transition to the READY state by configuring the gst_lame encoder */
static gboolean
gst_lame_setup (GstLame * lame)
{
#define CHECK_ERROR(command) G_STMT_START {\
  if ((command) < 0) { \
    GST_ERROR_OBJECT (lame, "setup failed: " G_STRINGIFY (command)); \
    return FALSE; \
  } \
}G_STMT_END
  int retval;

  GST_DEBUG_OBJECT (lame, "starting setup");

  /* check if we're already initialized; if we are, we might want to check
   * if this initialization is compatible with the previous one */
  /* FIXME: do this */
  if (lame->initialized) {
    GST_WARNING_OBJECT (lame, "already initialized");
    lame->initialized = FALSE;
  }

  /* copy the parameters over */
  lame_set_in_samplerate (lame->lgf, lame->samplerate);

  /* force mono encoding if we only have one channel */
  if (lame->num_channels == 1)
    lame->mode = 3;

  CHECK_ERROR (lame_set_num_channels (lame->lgf, lame->num_channels));
  CHECK_ERROR (lame_set_brate (lame->lgf, lame->bitrate));
  CHECK_ERROR (lame_set_compression_ratio (lame->lgf, lame->compression_ratio));
  CHECK_ERROR (lame_set_quality (lame->lgf, lame->quality));
  CHECK_ERROR (lame_set_mode (lame->lgf, lame->mode));
  CHECK_ERROR (lame_set_force_ms (lame->lgf, lame->force_ms));
  CHECK_ERROR (lame_set_free_format (lame->lgf, lame->free_format));
  CHECK_ERROR (lame_set_copyright (lame->lgf, lame->copyright));
  CHECK_ERROR (lame_set_original (lame->lgf, lame->original));
  CHECK_ERROR (lame_set_error_protection (lame->lgf, lame->error_protection));
  CHECK_ERROR (lame_set_padding_type (lame->lgf, lame->padding_type));
  CHECK_ERROR (lame_set_extension (lame->lgf, lame->extension));
  CHECK_ERROR (lame_set_strict_ISO (lame->lgf, lame->strict_iso));
  CHECK_ERROR (lame_set_disable_reservoir (lame->lgf, lame->disable_reservoir));
  CHECK_ERROR (lame_set_VBR (lame->lgf, lame->vbr));
  CHECK_ERROR (lame_set_VBR_q (lame->lgf, lame->vbr_quality));
  CHECK_ERROR (lame_set_VBR_mean_bitrate_kbps (lame->lgf,
          lame->vbr_mean_bitrate));
  CHECK_ERROR (lame_set_VBR_min_bitrate_kbps (lame->lgf,
          lame->vbr_min_bitrate));
  CHECK_ERROR (lame_set_VBR_max_bitrate_kbps (lame->lgf,
          lame->vbr_max_bitrate));
  CHECK_ERROR (lame_set_VBR_hard_min (lame->lgf, lame->vbr_hard_min));
  CHECK_ERROR (lame_set_lowpassfreq (lame->lgf, lame->lowpass_freq));
  CHECK_ERROR (lame_set_lowpasswidth (lame->lgf, lame->lowpass_width));
  CHECK_ERROR (lame_set_highpassfreq (lame->lgf, lame->highpass_freq));
  CHECK_ERROR (lame_set_highpasswidth (lame->lgf, lame->highpass_width));
  CHECK_ERROR (lame_set_ATHonly (lame->lgf, lame->ath_only));
  CHECK_ERROR (lame_set_ATHshort (lame->lgf, lame->ath_short));
  CHECK_ERROR (lame_set_noATH (lame->lgf, lame->no_ath));
  CHECK_ERROR (lame_set_ATHlower (lame->lgf, lame->ath_lower));
  CHECK_ERROR (lame_set_cwlimit (lame->lgf, lame->cwlimit));
  CHECK_ERROR (lame_set_allow_diff_short (lame->lgf, lame->allow_diff_short));
  CHECK_ERROR (lame_set_no_short_blocks (lame->lgf, lame->no_short_blocks));
  CHECK_ERROR (lame_set_emphasis (lame->lgf, lame->emphasis));
  CHECK_ERROR (lame_set_bWriteVbrTag (lame->lgf, lame->xingheader ? 1 : 0));
#ifdef GSTLAME_PRESET
  if (lame->preset > 0) {
    CHECK_ERROR (lame_set_preset (lame->lgf, lame->preset));
  }
#endif
  gst_lame_set_metadata (lame);

  /* initialize the lame encoder */
  if ((retval = lame_init_params (lame->lgf)) >= 0) {
    lame->initialized = TRUE;
    /* FIXME: it would be nice to print out the mode here */
    GST_INFO ("lame encoder initialized (%d kbit/s, %d Hz, %d channels)",
        lame->bitrate, lame->samplerate, lame->num_channels);
  } else {
    GST_ERROR_OBJECT (lame, "lame_init_params returned %d", retval);
  }

  GST_DEBUG_OBJECT (lame, "done with setup");

  return lame->initialized;
#undef CHECK_ERROR
}

static GstElementStateReturn
gst_lame_change_state (GstElement * element)
{
  GstLame *lame;

  g_return_val_if_fail (GST_IS_LAME (element), GST_STATE_FAILURE);

  lame = GST_LAME (element);

  GST_DEBUG ("state pending %d", GST_STATE_PENDING (element));

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_READY_TO_PAUSED:
      lame->last_ts = GST_CLOCK_TIME_NONE;
      break;
    case GST_STATE_READY_TO_NULL:
      if (lame->initialized) {
        lame_close (lame->lgf);
        lame->lgf = lame_init ();
        lame->initialized = FALSE;
      }
      break;
    default:
      break;
  }

  /* if we haven't failed already, give the parent class a chance to ;-) */
  if (GST_ELEMENT_CLASS (parent_class)->change_state)
    return GST_ELEMENT_CLASS (parent_class)->change_state (element);

  return GST_STATE_SUCCESS;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "lame", GST_RANK_NONE, GST_TYPE_LAME))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (debug, "lame", 0, "lame mp3 encoder");
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "lame",
    "Encode MP3's with LAME",
    plugin_init, VERSION, "LGPL", GST_PACKAGE, GST_ORIGIN)
