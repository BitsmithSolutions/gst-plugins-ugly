/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstbaseaudosink.h: 
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

/* a base class for audio sinks.
 *
 * It uses a ringbuffer to schedule playback of samples. This makes
 * it very easy to drop or insert samples to align incomming 
 * buffers to the exact playback timestamp.
 *
 * Subclasses must provide a ringbuffer pointing to either DMA
 * memory or regular memory. A subclass should also call a callback
 * function when it has processed N samples in the buffer. The subclass
 * is free to use a thread to signal this callback, use EIO or any
 * other mechanism.
 *
 * The base class is able to operate in push or pull mode. The chain
 * mode will queue the samples in the ringbuffer as much as possible.
 * The available space is calculated in the callback function.
 *
 * The pull mode will pull_range() a new buffer of N samples with a
 * configurable latency. This allows for high-end real time 
 * audio processing pipelines driven by the audiosink. The callback
 * function will be used to perform a pull_range() on the sinkpad.
 * The thread scheduling the callback can be a real-time thread.
 */

#ifndef __GST_BASEAUDIOSINK_H__
#define __GST_BASEAUDIOSINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gstringbuffer.h"

G_BEGIN_DECLS

#define GST_TYPE_BASEAUDIOSINK  	 (gst_baseaudiosink_get_type())
#define GST_BASEAUDIOSINK(obj) 		 (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASEAUDIOSINK,GstBaseAudioSink))
#define GST_BASEAUDIOSINK_CLASS(klass) 	 (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASEAUDIOSINK,GstBaseAudioSinkClass))
#define GST_BASEAUDIOSINK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_BASEAUDIOSINK, GstBaseAudioSinkClass))
#define GST_IS_BASEAUDIOSINK(obj)  	 (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASEAUDIOSINK))
#define GST_IS_BASEAUDIOSINK_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASEAUDIOSINK))

#define GST_BASEAUDIOSINK_CLOCK(obj)	 (GST_BASEAUDIOSINK (obj)->clock)
#define GST_BASEAUDIOSINK_PAD(obj)	 (GST_BASEAUDIOSINK (obj)->sinkpad)

typedef struct _GstBaseAudioSink GstBaseAudioSink;
typedef struct _GstBaseAudioSinkClass GstBaseAudioSinkClass;

struct _GstBaseAudioSink {
  GstBaseSink 	 element;

  GstRingBuffer *ringbuffer;
};

struct _GstBaseAudioSinkClass {
  GstBaseSinkClass parent_class;

  /* subclass ringbuffer allocation */
  GstRingBuffer* (*create_ringbuffer)  (GstBaseAudioSink *sink);
};

GType gst_baseaudiosink_get_type(void);

GstRingBuffer *gst_baseaudiosink_create_ringbuffer (GstBaseAudioSink *sink);

G_END_DECLS

#endif /* __GST_BASEAUDIOSINK_H__ */
