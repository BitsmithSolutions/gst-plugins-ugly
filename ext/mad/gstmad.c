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

#include <string.h>
#include "gstmad.h"

#define GST_TYPE_MAD \
  (gst_mad_get_type())
#define GST_MAD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MAD,GstMad))
#define GST_MAD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MAD,GstMad))
#define GST_IS_MAD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MAD))
#define GST_IS_MAD_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MAD))

typedef struct _GstMad GstMad;
typedef struct _GstMadClass GstMadClass;

struct _GstMad {
  GstElement	 element;

  /* pads */
  GstPad	*sinkpad, *srcpad;

  /* state */
  struct mad_stream stream;
  struct mad_frame frame;
  struct mad_synth synth;
  guchar	*tempbuffer;
  glong		tempsize;	/* used to keep track of partial buffers */
  gboolean	need_sync;
  GstClockTime  last_ts;
  guint64       base_byte_offset;
  guint64       bytes_consumed;   /* since the base_byte_offset */
  guint64	total_samples;  /* the number of samples since the sync point */

  gboolean	restart;
  guint64       segment_start;

  /* info */
  struct mad_header header;
  gboolean	new_header;
  guint		framecount;
  gint		vbr_average; /* average bitrate */
  guint64	vbr_rate; /* average * framecount */

  gboolean	half;
  gboolean	ignore_crc;

  GstTagList *	tags;

  /* negotiated format */
  gint		rate;
  gint		channels;

  GstIndex	*index;
  gint		 index_id;
};

struct _GstMadClass {
  GstElementClass parent_class;
};

/* elementfactory information */
static GstElementDetails gst_mad_details = GST_ELEMENT_DETAILS (
  "mad mp3 decoder",
  "Codec/Decoder/Audio",
  "Uses mad code to decode mp3 streams",
  "Wim Taymans <wim.taymans@chello.be>"
);


/* Mad signals and args */
enum {
  /* FILL ME */
  LAST_SIGNAL
};

enum {
  ARG_0,
  ARG_HALF,
  ARG_IGNORE_CRC,
  ARG_METADATA,
  ARG_STREAMINFO
  /* FILL ME */
};

static GstStaticPadTemplate mad_src_template_factory =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("audio/x-raw-int, "
      "endianness = (int) " G_STRINGIFY (G_BYTE_ORDER) ", "
      "signed = (boolean) true, "
      "width = (int) 16, "
      "depth = (int) 16, "
      "rate = (int) [ 11025, 48000 ], "
      "channels = (int) [ 1, 2 ]"
  )
);

static GstStaticPadTemplate mad_sink_template_factory =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS ("audio/mpeg, "
      "mpegversion = (int) 1, "
      "layer = (int) [ 1, 3 ]"
  )
);

static void		gst_mad_base_init	(gpointer g_class);
static void		gst_mad_class_init	(GstMadClass *klass);
static void		gst_mad_init		(GstMad *mad);
static void		gst_mad_dispose 	(GObject *object);

static void		gst_mad_set_property	(GObject *object, guint prop_id, 
						 const GValue *value, GParamSpec *pspec);
static void		gst_mad_get_property	(GObject *object, guint prop_id, 
						 GValue *value, GParamSpec *pspec);

static gboolean	gst_mad_src_event	(GstPad *pad, GstEvent *event);
static const GstFormat*
		gst_mad_get_formats	(GstPad *pad);
static const GstEventMask*
		gst_mad_get_event_masks (GstPad *pad);
static const GstQueryType*
		gst_mad_get_query_types (GstPad *pad);

static gboolean	gst_mad_src_query	(GstPad *pad, GstQueryType type,
					 GstFormat *format, gint64 *value);
static gboolean	gst_mad_convert_sink	(GstPad *pad, GstFormat src_format,
		                         gint64 src_value, GstFormat
					 *dest_format, gint64 *dest_value);
static gboolean	gst_mad_convert_src	(GstPad *pad, GstFormat src_format,
					 gint64 src_value, GstFormat
					 *dest_format, gint64 *dest_value);

static void	gst_mad_chain		(GstPad *pad, GstData *_data);

static GstElementStateReturn
		gst_mad_change_state (GstElement *element);

static void      gst_mad_set_index (GstElement *element, GstIndex *index);
static GstIndex* gst_mad_get_index (GstElement *element);


static GstElementClass *parent_class = NULL;
/* static guint gst_mad_signals[LAST_SIGNAL] = { 0 }; */

GType
gst_mad_get_type (void)
{
  static GType mad_type = 0;

  if (!mad_type) {
    static const GTypeInfo mad_info = {
      sizeof (GstMadClass),
      gst_mad_base_init,
      NULL,
      (GClassInitFunc) gst_mad_class_init,
      NULL,
      NULL,
      sizeof (GstMad),
      0,
      (GInstanceInitFunc) gst_mad_init,
    };
    mad_type = g_type_register_static(GST_TYPE_ELEMENT, "GstMad", &mad_info, 0);
  }
  return mad_type;
}

#define GST_TYPE_MAD_LAYER (gst_mad_layer_get_type())
G_GNUC_UNUSED static GType
gst_mad_layer_get_type(void) {
  static GType mad_layer_type = 0;
  static GEnumValue mad_layer[] = {
    {0,             "0", "Unknown"},
    {MAD_LAYER_I,   "1", "I"},
    {MAD_LAYER_II,  "2", "II"},
    {MAD_LAYER_III, "3", "III"},
    {0, NULL, NULL},
  };
  if (!mad_layer_type) {
    mad_layer_type = g_enum_register_static("GstMadLayer", mad_layer);
  }
  return mad_layer_type;
}

#define GST_TYPE_MAD_MODE (gst_mad_mode_get_type())
G_GNUC_UNUSED static GType
gst_mad_mode_get_type(void) {
  static GType mad_mode_type = 0;
  static GEnumValue mad_mode[] = {
    {-1,                     "-1", "Unknown"},
    {MAD_MODE_SINGLE_CHANNEL, "0", "Single Channel"},
    {MAD_MODE_DUAL_CHANNEL  , "1", "Dual Channel"},
    {MAD_MODE_JOINT_STEREO  , "2", "Joint Stereo"},
    {MAD_MODE_STEREO        , "3", "Stereo"},
    { 0, NULL, NULL},
  };
  if (!mad_mode_type) {
    mad_mode_type = g_enum_register_static("GstMadMode", mad_mode);
  }
  return mad_mode_type;
}

#define GST_TYPE_MAD_EMPHASIS (gst_mad_emphasis_get_type())
G_GNUC_UNUSED static GType
gst_mad_emphasis_get_type(void) {
  static GType mad_emphasis_type = 0;
  static GEnumValue mad_emphasis[] = {
    {-1,                     "-1", "Unknown"},
    {MAD_EMPHASIS_NONE,       "0", "None"},
    {MAD_EMPHASIS_50_15_US,   "1", "50/15 Microseconds"},
    {MAD_EMPHASIS_CCITT_J_17, "2", "CCITT J.17"},
    {MAD_EMPHASIS_RESERVED,   "3", "Reserved"},
    { 0, NULL, NULL},
  };
  if (!mad_emphasis_type) {
    mad_emphasis_type = g_enum_register_static("GstMadEmphasis", mad_emphasis);
  }
  return mad_emphasis_type;
}

static void
gst_mad_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_add_pad_template (element_class,
		  gst_static_pad_template_get (&mad_sink_template_factory));
  gst_element_class_add_pad_template (element_class,
		  gst_static_pad_template_get (&mad_src_template_factory));
  gst_element_class_set_details (element_class, &gst_mad_details);
}
static void
gst_mad_class_init (GstMadClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass*) klass;
  gstelement_class = (GstElementClass*) klass;

  parent_class = g_type_class_ref(GST_TYPE_ELEMENT);

  gobject_class->set_property = gst_mad_set_property;
  gobject_class->get_property = gst_mad_get_property;
  gobject_class->dispose = gst_mad_dispose;

  gstelement_class->change_state = gst_mad_change_state;
  gstelement_class->set_index    = gst_mad_set_index;
  gstelement_class->get_index    = gst_mad_get_index;

  /* init properties */
  /* currently, string representations are used, we might want to change that */
  /* FIXME: descriptions need to be more technical,
   * default values and ranges need to be selected right */
  g_object_class_install_property (gobject_class, ARG_HALF,
    g_param_spec_boolean ("half", "Half", "Generate PCM at 1/2 sample rate",
		          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, ARG_IGNORE_CRC,
    g_param_spec_boolean ("ignore_crc", "Ignore CRC", "Ignore CRC errors",
		          FALSE, G_PARAM_READWRITE));
}

static void
gst_mad_init (GstMad *mad)
{
  /* create the sink and src pads */
  mad->sinkpad = gst_pad_new_from_template(
		  gst_static_pad_template_get (&mad_sink_template_factory), "sink");
  gst_element_add_pad (GST_ELEMENT (mad), mad->sinkpad);
  gst_pad_set_chain_function (mad->sinkpad, GST_DEBUG_FUNCPTR (gst_mad_chain));
  gst_pad_set_convert_function (mad->sinkpad, GST_DEBUG_FUNCPTR (gst_mad_convert_sink));
  gst_pad_set_formats_function (mad->sinkpad, GST_DEBUG_FUNCPTR (gst_mad_get_formats));

  mad->srcpad = gst_pad_new_from_template(
		  gst_static_pad_template_get (&mad_src_template_factory), "src");
  gst_element_add_pad (GST_ELEMENT (mad), mad->srcpad);
  gst_pad_set_event_function (mad->srcpad, GST_DEBUG_FUNCPTR (gst_mad_src_event));
  gst_pad_set_event_mask_function (mad->srcpad, GST_DEBUG_FUNCPTR (gst_mad_get_event_masks));
  gst_pad_set_query_function (mad->srcpad, GST_DEBUG_FUNCPTR (gst_mad_src_query));
  gst_pad_set_query_type_function (mad->srcpad, GST_DEBUG_FUNCPTR (gst_mad_get_query_types));
  gst_pad_set_convert_function (mad->srcpad, GST_DEBUG_FUNCPTR (gst_mad_convert_src));
  gst_pad_set_formats_function (mad->srcpad, GST_DEBUG_FUNCPTR (gst_mad_get_formats));

  mad->tempbuffer = g_malloc (MAD_BUFFER_MDLEN * 3);
  mad->tempsize = 0;
  mad->need_sync = TRUE;
  mad->base_byte_offset = 0;
  mad->bytes_consumed = 0;
  mad->total_samples = 0;
  mad->new_header = TRUE;
  mad->framecount = 0;
  mad->vbr_average = 0;
  mad->vbr_rate = 0;
  mad->restart = FALSE;
  mad->segment_start = 0;
  mad->header.mode = -1;
  mad->header.emphasis = -1;
  mad->tags = NULL;

  mad->half = FALSE;
  mad->ignore_crc = FALSE;

  GST_FLAG_SET (mad, GST_ELEMENT_EVENT_AWARE);
}

static void
gst_mad_dispose (GObject *object)
{
  GstMad *mad = GST_MAD (object);

  gst_mad_set_index (GST_ELEMENT (object), NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);

  g_free (mad->tempbuffer);
}

static void
gst_mad_set_index (GstElement *element, GstIndex *index)
{
  GstMad *mad = GST_MAD (element);
  
  mad->index = index;

  if (index)
    gst_index_get_writer_id (index, GST_OBJECT (element), &mad->index_id);
}

static GstIndex*
gst_mad_get_index (GstElement *element)
{
  GstMad *mad = GST_MAD (element);

  return mad->index;
}

static const GstFormat*
gst_mad_get_formats (GstPad *pad)
{
  static const GstFormat src_formats[] = {
    GST_FORMAT_BYTES,
    GST_FORMAT_DEFAULT,
    GST_FORMAT_TIME,
    0
  };
  static const GstFormat sink_formats[] = {
    GST_FORMAT_BYTES,
    GST_FORMAT_TIME,
    0
  };

  return (GST_PAD_IS_SRC (pad) ? src_formats : sink_formats);
}

static const GstEventMask*
gst_mad_get_event_masks (GstPad *pad)
{
  static const GstEventMask gst_mad_src_event_masks[] = {
    { GST_EVENT_SEEK, GST_SEEK_METHOD_SET |
                      GST_SEEK_FLAG_FLUSH },
    { 0, }
  };
  return gst_mad_src_event_masks;
}

static gboolean
gst_mad_convert_sink (GstPad *pad, GstFormat src_format, gint64 src_value,
		      GstFormat *dest_format, gint64 *dest_value)
{
  gboolean res = TRUE;
  GstMad *mad;

  mad = GST_MAD (gst_pad_get_parent (pad));

  if (mad->vbr_average == 0)
    return FALSE;

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
          /* multiply by 8 because vbr is in bits/second */
          *dest_value = src_value * 8 * GST_SECOND / mad->vbr_average;
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          /* multiply by 8 because vbr is in bits/second */
          *dest_value = src_value * mad->vbr_average / (8 * GST_SECOND);
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }
  return res;
}

static gboolean
gst_mad_convert_src (GstPad *pad, GstFormat src_format, gint64 src_value,
		     GstFormat *dest_format, gint64 *dest_value)
{
  gboolean res = TRUE;
  guint scale = 1;
  gint bytes_per_sample;
  GstMad *mad;

  mad = GST_MAD (gst_pad_get_parent (pad));

  bytes_per_sample = MAD_NCHANNELS (&mad->frame.header) << 1;

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_DEFAULT:
	  if (bytes_per_sample == 0)
            return FALSE;
	  *dest_value = src_value / bytes_per_sample;
          break;
        case GST_FORMAT_TIME:
	{
          gint byterate = bytes_per_sample * mad->frame.header.samplerate;

	  if (byterate == 0)
            return FALSE;
	  *dest_value = src_value * GST_SECOND / byterate;
          break;
	}
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_DEFAULT:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
	  *dest_value = src_value * bytes_per_sample;
	  break;
        case GST_FORMAT_TIME:
	  if (mad->frame.header.samplerate == 0)
            return FALSE;
	  *dest_value = src_value * GST_SECOND / mad->frame.header.samplerate;
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
	  scale = bytes_per_sample;
	  /* fallthrough */
        case GST_FORMAT_DEFAULT:
	  *dest_value = src_value * scale * mad->frame.header.samplerate / GST_SECOND;
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      res = FALSE;
  }
  return res;
}

static const GstQueryType*
gst_mad_get_query_types (GstPad *pad)
{
  static const GstQueryType gst_mad_src_query_types[] = {
    GST_QUERY_TOTAL,
    GST_QUERY_POSITION,
    0
  };
  return gst_mad_src_query_types;
}

static gboolean
gst_mad_src_query (GstPad *pad, GstQueryType type,
		   GstFormat *format, gint64 *value)
{
  gboolean res = TRUE;
  GstMad *mad;

  mad = GST_MAD (gst_pad_get_parent (pad));

  switch (type) {
    case GST_QUERY_TOTAL:
    {
      switch (*format) {
	case GST_FORMAT_BYTES:
	case GST_FORMAT_DEFAULT:
	case GST_FORMAT_TIME:
        {
	  gint64 peer_value;
          const GstFormat *peer_formats;

	  res = FALSE;

	  peer_formats = gst_pad_get_formats (GST_PAD_PEER (mad->sinkpad));

	  while (peer_formats && *peer_formats && !res) {

	    GstFormat peer_format = *peer_formats;

	    /* do the probe */
            if (gst_pad_query (GST_PAD_PEER (mad->sinkpad), GST_QUERY_TOTAL,
			       &peer_format, &peer_value))
	    {
              GstFormat conv_format;
	      /* convert to TIME */
              conv_format = GST_FORMAT_TIME;
	      res = gst_pad_convert (mad->sinkpad,
				peer_format, peer_value,
				&conv_format, value);
	      /* and to final format */
	      res &= gst_pad_convert (pad,
			GST_FORMAT_TIME, *value,
			format, value);
	    }
	    peer_formats++;
	  }
	  break;
	}
	default:
	  res = FALSE;
	  break;
      }
      break;
    }
    case GST_QUERY_POSITION:
      switch (*format) {
	default:
	{
	  /* we only know about our samples, convert to requested format */
	  res &= gst_pad_convert (pad,
			  GST_FORMAT_DEFAULT, mad->total_samples,
			  format, value);
	  break;
	}
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
}

static gboolean
index_seek (GstMad *mad, GstPad *pad, GstEvent *event)
{
  /* since we know the exact byteoffset of the frame,
     make sure to try bytes first */

  const GstFormat try_all_formats[] = { 
    GST_FORMAT_BYTES,
    GST_FORMAT_TIME,
    0
  };
  const GstFormat *try_formats = try_all_formats;
  const GstFormat *peer_formats;

  GstIndexEntry *entry =
    gst_index_get_assoc_entry (mad->index, mad->index_id,
			       GST_INDEX_LOOKUP_BEFORE, 0,
			       GST_EVENT_SEEK_FORMAT (event),
			       GST_EVENT_SEEK_OFFSET (event));
  if (!entry)
    return FALSE;
  
  peer_formats = gst_pad_get_formats (GST_PAD_PEER (mad->sinkpad));

  while (gst_formats_contains (peer_formats, *try_formats)) {
    gint64 value;
    GstEvent *seek_event;
    
    if (gst_index_entry_assoc_map (entry, *try_formats, &value)) {
      /* lookup succeeded, create the seek */

      GST_DEBUG ("index %s %" G_GINT64_FORMAT
		     " -> %s %" G_GINT64_FORMAT,
		     gst_format_get_details (GST_EVENT_SEEK_FORMAT (event))->nick,
		     GST_EVENT_SEEK_OFFSET (event),
		     gst_format_get_details (*try_formats)->nick,
		     value);

      seek_event = gst_event_new_seek (*try_formats |
				       GST_SEEK_METHOD_SET |
				       GST_SEEK_FLAG_FLUSH, value);

      if (gst_pad_send_event (GST_PAD_PEER (mad->sinkpad), seek_event)) {
	/* seek worked, we're done, loop will exit */
	mad->restart = TRUE;
	g_assert (GST_EVENT_SEEK_FORMAT (event) == GST_FORMAT_TIME);
	mad->segment_start = GST_EVENT_SEEK_OFFSET (event);
	return TRUE;
      }
    }
    try_formats++;
  }

  return FALSE;
}

static gboolean
normal_seek (GstMad *mad, GstPad *pad, GstEvent *event)
{
  gint64 src_offset;
  gboolean flush;
  GstFormat format;
  const GstFormat *peer_formats;
  gboolean res;
  
  format = GST_FORMAT_TIME;
  
  /* first bring the src_format to TIME */
  if (!gst_pad_convert (pad,
			GST_EVENT_SEEK_FORMAT (event), GST_EVENT_SEEK_OFFSET (event),
			&format, &src_offset))
  {
    /* didn't work, probably unsupported seek format then */
    return FALSE;
  }
  
  /* shave off the flush flag, we'll need it later */
  flush = GST_EVENT_SEEK_FLAGS (event) & GST_SEEK_FLAG_FLUSH;
  
  /* assume the worst */
  res = FALSE;
  
  peer_formats = gst_pad_get_formats (GST_PAD_PEER (mad->sinkpad));
  
  /* while we did not exhaust our seek formats without result */
  while (peer_formats && *peer_formats && !res) {
    gint64 desired_offset;
    
    format = *peer_formats;
    
    /* try to convert requested format to one we can seek with on the sinkpad */
    if (gst_pad_convert (mad->sinkpad, GST_FORMAT_TIME, src_offset,
			 &format, &desired_offset))
      {
	GstEvent *seek_event;
	
	/* conversion succeeded, create the seek */
	seek_event = gst_event_new_seek (format | GST_SEEK_METHOD_SET | flush,
					 desired_offset);
	/* do the seek */
	if (gst_pad_send_event (GST_PAD_PEER (mad->sinkpad), seek_event)) {
	  /* seek worked, we're done, loop will exit */
	  res = TRUE;
	}
      }
    /* at this point, either the seek worked or res == FALSE */
    if (res)
      /* we need to break out of the processing loop on flush */
      mad->restart = flush;
    
    peer_formats++;
  }

  return res;
}

static gboolean
gst_mad_src_event (GstPad *pad, GstEvent *event)
{
  gboolean res = TRUE;
  GstMad *mad;

  mad = GST_MAD (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK_SEGMENT:
      gst_event_ref (event);
      if (gst_pad_send_event (GST_PAD_PEER (mad->sinkpad), event)) {
        /* seek worked, we're done, loop will exit */
        res = TRUE;
      }
      break;
    /* the all-formats seek logic */
    case GST_EVENT_SEEK:
      if (mad->index)
	res = index_seek (mad, pad, event);
      else
	res = normal_seek (mad, pad, event);
      break;

    default:
      res = FALSE;
      break;
  }

  gst_event_unref (event);
  return res;
}

static inline signed int
scale (mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

/* do we need this function? */
static void
gst_mad_set_property (GObject *object, guint prop_id,
		      const GValue *value, GParamSpec *pspec)
{
  GstMad *mad;

  g_return_if_fail (GST_IS_MAD (object));

  mad = GST_MAD (object);

  switch (prop_id) {
    case ARG_HALF:
      mad->half = g_value_get_boolean (value);
      break;
    case ARG_IGNORE_CRC:
      mad->ignore_crc = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
static void
gst_mad_get_property (GObject *object, guint prop_id,
		      GValue *value, GParamSpec *pspec)
{
  GstMad *mad;

  g_return_if_fail (GST_IS_MAD (object));

  mad = GST_MAD (object);

  switch (prop_id) {
    case ARG_HALF:
      g_value_set_boolean (value, mad->half);
      break;
    case ARG_IGNORE_CRC:
      g_value_set_boolean (value, mad->ignore_crc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mad_update_info (GstMad *mad)
{
  gint abr = mad->vbr_average;
  struct mad_header *header = &mad->frame.header;
  gboolean changed = FALSE;

#define CHECK_HEADER(h1,str)					\
G_STMT_START{							\
  if (mad->header.h1 != header->h1 || mad->new_header) {	\
    mad->header.h1 = header->h1;				\
     changed = TRUE;						\
  };								\
} G_STMT_END

  /* update average bitrate */
  if (mad->new_header) {
    mad->framecount = 1;
    mad->vbr_rate = header->bitrate;
    abr = 0;
  } else {
    mad->framecount++;
    mad->vbr_rate += header->bitrate;
  }
  mad->vbr_average = (gint) (mad->vbr_rate / mad->framecount);

  CHECK_HEADER (layer,	    "layer");
  CHECK_HEADER (mode,	    "mode");
  CHECK_HEADER (emphasis,   "emphasis");

  if (header->bitrate != mad->header.bitrate || mad->new_header) {
    mad->header.bitrate = header->bitrate;
  }
  mad->new_header = FALSE;

#undef CHECK_HEADER

}

static void
gst_mad_handle_event (GstPad *pad, GstBuffer *buffer)
{
  GstEvent *event = GST_EVENT (buffer);
  GstMad *mad = GST_MAD (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event))
  {
    case GST_EVENT_DISCONTINUOUS:
    {
      gint n = GST_EVENT_DISCONT_OFFSET_LEN (event);
      gint i;

      mad->total_samples = 0;

      for (i = 0; i < n; i++)
      {
	const GstFormat *formats;
	formats = gst_pad_get_formats (pad);

        if (gst_formats_contains (formats, GST_EVENT_DISCONT_OFFSET(event, i).format))
        {
          gint64 value = GST_EVENT_DISCONT_OFFSET (event, i).value;
          gint64 time;
          GstFormat format;
          GstEvent *discont;

          /* see how long the input bytes take */
          format = GST_FORMAT_TIME;
          if (!gst_pad_convert (pad,
		                GST_EVENT_DISCONT_OFFSET (event, i).format,
				value, &format, &time))
          {
            time = 0;
          }

          /* for now, this is the best we can do to get the total number
	   * of samples */
          format = GST_FORMAT_DEFAULT;
          if (!gst_pad_convert (mad->srcpad,
		                GST_FORMAT_TIME, time, &format, &mad->total_samples))
          {
            mad->total_samples = 0;
          }

          gst_event_unref (event);

          if (GST_PAD_IS_USABLE (mad->srcpad))
	  {
            discont = gst_event_new_discontinuous (FALSE, GST_FORMAT_TIME,
			                           time, NULL);
            gst_pad_push (mad->srcpad, GST_DATA (discont));
          }
          break;
        }
      }
      mad->tempsize = 0;
      /* we don't need to restart when we get here */
      mad->restart = FALSE;
      break;
    }
    default:
      gst_pad_event_default (pad, event);
      break;
  }
  return;
}

static gboolean
gst_mad_check_restart (GstMad *mad)
{
  gboolean yes = mad->restart;
  if (mad->restart) {
    mad->restart = FALSE;
    mad->tempsize = 0;
  }
  return yes;
}

static void
gst_mad_chain (GstPad *pad, GstData *_data)
{
  GstBuffer *buffer = GST_BUFFER (_data);
  GstMad *mad;
  gchar *data;
  glong size;
  gboolean new_pts = FALSE;
  GstClockTime timestamp;

  mad = GST_MAD (gst_pad_get_parent (pad));
  g_return_if_fail (GST_IS_MAD (mad));

  /* handle events */
  if (GST_IS_EVENT (buffer))
  {
    gst_mad_handle_event (pad, buffer);
    return;
  }

  gst_mad_check_restart (mad);

  timestamp = GST_BUFFER_TIMESTAMP (buffer);

  /* handle timestamps */
  if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
    /* if there is nothing queued (partial buffer), we prepare to set the
     * timestamp on the next buffer */
    if (mad->tempsize == 0) {
      /* we have to save the result here because we can't yet convert the timestamp
       * to a sample offset yet, the samplerate might not be known yet */
      mad->last_ts = timestamp;
      mad->base_byte_offset = GST_BUFFER_OFFSET (buffer);
      mad->bytes_consumed = 0;
    }
    /* else we need to finish the current partial frame with the old timestamp
     * and queue this timestamp for the next frame */
    else {
      new_pts = TRUE;
    }
  }

  /* handle data */
  data = GST_BUFFER_DATA (buffer);
  size = GST_BUFFER_SIZE (buffer);

  /* process the incoming buffer in chunks of maximum MAD_BUFFER_MDLEN bytes;
   * this is the upper limit on processable chunk sizes set by mad */
  while (size > 0)
  {
    gint tocopy;
    guchar *mad_input_buffer;

    tocopy = MIN (MAD_BUFFER_MDLEN, size);

    /* append the chunk to process to our internal temporary buffer */
    memcpy (mad->tempbuffer + mad->tempsize, data, tocopy);
    mad->tempsize += tocopy;

    /* update our incoming buffer's parameters to reflect this */
    size -= tocopy;
    data += tocopy;

    mad_input_buffer = mad->tempbuffer;

    /* while we have data we can consume it */
    while (mad->tempsize >= 0) {
      gint consumed;
      guint nchannels;
      guint nsamples;
      gint rate;
      guint64 time_offset;
      guint64 time_duration;

      mad_stream_buffer (&mad->stream, mad_input_buffer, mad->tempsize);

      if (mad_frame_decode (&mad->frame, &mad->stream) == -1) {
	/* not enough data, need to wait for next buffer? */
	if (mad->stream.error == MAD_ERROR_BUFLEN) {
	  break;
	}
        if (!MAD_RECOVERABLE (mad->stream.error)) {
          gst_element_error (GST_ELEMENT (mad), "fatal error decoding stream");
          return;
        }
	else if (mad->stream.error == MAD_ERROR_LOSTSYNC) {
	  /* lost sync, force a resync */
	  signed long tagsize;

	  tagsize = id3_tag_query (mad->stream.this_frame,
	                          mad->stream.bufend - mad->stream.this_frame);

	  if (tagsize > mad->tempsize) {
            GST_INFO ("mad: got partial id3 tag in buffer, skipping");
	  }
	  else if (tagsize > 0) {
	    struct id3_tag *tag;
	    id3_byte_t const *data;

            GST_INFO ("mad: got ID3 tag size %ld", tagsize);

	    data = mad->stream.this_frame;

	    /* mad has moved the pointer to the next frame over the start of the
	     * id3 tags, so we need to flush one byte less than the tagsize */
	    mad_stream_skip (&mad->stream, tagsize - 1);

	    tag = id3_tag_parse (data, tagsize);
	    if (tag) {
	      GstTagList *list;

	      list = gst_mad_id3_to_tag_list (tag);
	      id3_tag_delete (tag);
	      GST_DEBUG_OBJECT (mad, "found tag");
	      gst_element_found_tags (GST_ELEMENT (mad), list);
	      if (mad->tags) {
		gst_tag_list_insert (mad->tags, list, GST_TAG_MERGE_PREPEND);
	      } else {
		mad->tags = gst_tag_list_copy (list);
	      }
	      if (GST_PAD_IS_USABLE (mad->srcpad)) {
		gst_pad_push (mad->srcpad, GST_DATA (gst_event_new_tag (list)));
	      } else {
		gst_tag_list_free (list);
	      }
	    }
	  }
	}
	mad_frame_mute (&mad->frame);
	mad_synth_mute (&mad->synth);
	mad_stream_sync (&mad->stream);
	/* recoverable errors pass */
	goto next;
      }

      nchannels = MAD_NCHANNELS (&mad->frame.header);
      nsamples  = MAD_NSBSAMPLES (&mad->frame.header) *
         (mad->stream.options & MAD_OPTION_HALFSAMPLERATE ? 16 : 32);

#if MAD_VERSION_MINOR <= 12
      rate = mad->header.sfreq;
#else
      rate = mad->frame.header.samplerate;
#endif

      gst_mad_update_info (mad);

      if (mad->channels != nchannels || mad->rate != rate) {
        if (mad->stream.options & MAD_OPTION_HALFSAMPLERATE)
	  rate >>=1;

	/* we set the caps even when the pad is not connected so they
	 * can be gotten for streaminfo */
        if (gst_pad_try_set_caps (mad->srcpad,
	    gst_caps_new_simple ("audio/x-raw-int",
	      "endianness",  G_TYPE_INT, G_BYTE_ORDER,
	      "signed",      G_TYPE_BOOLEAN, TRUE,
	      "width",       G_TYPE_INT, 16,
	      "depth",       G_TYPE_INT, 16,
	      "rate",        G_TYPE_INT, rate,
	      "channels",    G_TYPE_INT, nchannels,
	      NULL)) <= 0)
	{
	  if (!gst_pad_recover_caps_error (mad->srcpad, NULL)) {
	    gst_buffer_unref (buffer);
	    return;
	  }
        }
	mad->channels = nchannels;
	mad->rate = rate;
      }

      if (mad->frame.header.samplerate == 0) {
	g_warning ("mad->frame.header.samplerate is 0; timestamps cannot be calculated");
	time_offset = GST_CLOCK_TIME_NONE;
	time_duration = GST_CLOCK_TIME_NONE;
      }
      else {
	/* if we have a pending timestamp, we can use it now to calculate the sample offset */
	if (GST_CLOCK_TIME_IS_VALID (mad->last_ts)) {
          GstFormat format = GST_FORMAT_DEFAULT;
          gst_pad_convert (mad->srcpad, GST_FORMAT_TIME, mad->last_ts, &format, &mad->total_samples);
	  mad->last_ts = GST_CLOCK_TIME_NONE;
	}
	time_offset = mad->total_samples * GST_SECOND
					/ mad->frame.header.samplerate;
	time_duration = (nsamples * GST_SECOND
			 / mad->frame.header.samplerate);
      }

      if (mad->index) {
	guint64 x_bytes = mad->base_byte_offset + mad->bytes_consumed;

	gst_index_add_association (mad->index, mad->index_id, 0,
				   GST_FORMAT_BYTES, x_bytes,
				   GST_FORMAT_TIME, time_offset,
				   NULL);
      }

      if (GST_PAD_IS_USABLE (mad->srcpad) &&
	  mad->segment_start <= (time_offset == GST_CLOCK_TIME_NONE ? 0 : time_offset)) {

	/* for sample accurate seeking, calculate how many samples
	   to skip and send the remaining pcm samples */

        GstBuffer *outbuffer;
        gint16 *outdata;
        mad_fixed_t const *left_ch, *right_ch;

        mad_synth_frame (&mad->synth, &mad->frame);
        left_ch   = mad->synth.pcm.samples[0];
        right_ch  = mad->synth.pcm.samples[1];

        outbuffer = gst_buffer_new_and_alloc (nsamples * nchannels * 2);
        outdata = (gint16 *) GST_BUFFER_DATA (outbuffer);

	GST_BUFFER_TIMESTAMP (outbuffer) = time_offset;
	GST_BUFFER_DURATION (outbuffer) = time_duration;
	GST_BUFFER_OFFSET (outbuffer) = mad->total_samples;

        /* output sample(s) in 16-bit signed native-endian PCM */
        if (nchannels == 1) {
          gint count = nsamples;
          while (count--) {
            *outdata++ = scale(*left_ch++) & 0xffff;
          }
	}
	else {
          gint count = nsamples;
          while (count--) {
            *outdata++ = scale(*left_ch++) & 0xffff;
            *outdata++ = scale(*right_ch++) & 0xffff;
          }
        }

        gst_pad_push (mad->srcpad, GST_DATA (outbuffer));
      }

      mad->total_samples += nsamples;

      /* we have a queued timestamp on the incoming buffer that we should
       * use for the next frame */
      if (new_pts) {
	mad->last_ts = timestamp;
	new_pts = FALSE;
	mad->base_byte_offset = GST_BUFFER_OFFSET (buffer);
	mad->bytes_consumed = 0;
      }

      if (gst_mad_check_restart (mad)) {
	goto end;
      }

next:
      /* figure out how many bytes mad consumed */
      consumed = mad->stream.next_frame - mad_input_buffer;

      /* move out pointer to where mad want the next data */
      mad_input_buffer += consumed;
      mad->tempsize -= consumed;
      mad->bytes_consumed += consumed;
    }
    memmove (mad->tempbuffer, mad_input_buffer, mad->tempsize);
  }

end:
  gst_buffer_unref (buffer);
}

static GstElementStateReturn
gst_mad_change_state (GstElement *element)
{
  GstMad *mad;

  mad = GST_MAD (element);

  switch (GST_STATE_TRANSITION (element)) {
    case GST_STATE_NULL_TO_READY:
      break;
    case GST_STATE_READY_TO_PAUSED:
    {
      guint options = 0;

      mad_stream_init (&mad->stream);
      mad_frame_init (&mad->frame);
      mad_synth_init (&mad->synth);
      mad->tempsize = 0;
      mad->total_samples = 0;
      mad->rate = 0;
      mad->channels = 0;
      mad->vbr_average = 0;
      mad->segment_start = 0;
      mad->new_header = TRUE;
      mad->framecount = 0;
      mad->vbr_rate = 0;
      mad->frame.header.samplerate = 0;
      mad->last_ts = GST_CLOCK_TIME_NONE;
      if (mad->ignore_crc) options |= MAD_OPTION_IGNORECRC;
      if (mad->half) options |= MAD_OPTION_HALFSAMPLERATE;
      mad_stream_options (&mad->stream, options);
      break;
    }
    case GST_STATE_PAUSED_TO_PLAYING:
      /* do something to get out of the chain function faster */
      break;
    case GST_STATE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_PAUSED_TO_READY:
      mad_synth_finish (&mad->synth);
      mad_frame_finish (&mad->frame);
      mad_stream_finish (&mad->stream);
      mad->restart = TRUE;
      if (mad->tags) {
	gst_tag_list_free (mad->tags);
	mad->tags = NULL;
      }
      break;
    case GST_STATE_READY_TO_NULL:
      break;
  }

  return parent_class->change_state (element);
}

