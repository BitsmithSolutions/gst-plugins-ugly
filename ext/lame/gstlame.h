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


#ifndef __GST_LAME_H__
#define __GST_LAME_H__


#include <config.h>
#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <lame/lame.h>

#define GST_TYPE_LAME \
  (gst_lame_get_type())
#define GST_LAME(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LAME,GstLame))
#define GST_LAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_LAME,GstLameClass))
#define GST_IS_LAME(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_LAME))
#define GST_IS_LAME_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_LAME))

typedef enum {
  GST_LAME_OPEN		= GST_ELEMENT_FLAG_LAST,

  GST_LAME_FLAG_LAST	= GST_ELEMENT_FLAG_LAST+2,
} GstLameFlags;

typedef struct _GstLame GstLame;
typedef struct _GstLameClass GstLameClass;

struct _GstLame {
  GstElement element;
  /* pads */
  GstPad *srcpad, *sinkpad;

  gint samplerate;
  gint num_channels;
  gboolean initialized;

  gint bitrate;
  gfloat compression_ratio;
  gint quality;
  gint mode;
  gboolean force_ms;
  gboolean free_format;
  gboolean copyright;
  gboolean original;
  gboolean error_protection;
  gint padding_type;
  gboolean extension;
  gboolean strict_iso;
  gboolean disable_reservoir;
  gboolean vbr;
  gint vbr_mean_bitrate;
  gint vbr_min_bitrate;
  gint vbr_max_bitrate;
  gint vbr_hard_min;
  gint lowpass_freq;
  gint lowpass_width;
  gint highpass_freq;
  gint highpass_width;
  gboolean ath_only;
  gboolean ath_short;
  gboolean no_ath;
  gint ath_type;
  gint ath_lower;
  gint cwlimit;
  gboolean allow_diff_short;
  gboolean no_short_blocks;
  gboolean emphasis;

  lame_global_flags *lgf;

  /* time tracker */
  guint64 last_ts;
};

struct _GstLameClass {
  GstElementClass parent_class;
};

GType gst_lame_get_type(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_LAME_H__ */
