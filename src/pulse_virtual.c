#include "pulse_virtual.h"

// global vars
int ret;
int *p_in;
pa_context *context;
static pa_stream *stream = NULL;
static pa_stream_flags_t flags = PA_STREAM_ADJUST_LATENCY;
int fdout;
char *fname = "/tmp/tmp.s16";
SNDFILE *localinf;
static pa_sample_spec sample_spec = {
  .format = PA_SAMPLE_S16LE,
  .rate = 44100,
  .channels = 2
};

void* pulse_virtual(int *pipe)
{
    p_in = pipe;
    pa_mainloop *pa_ml = pa_mainloop_new();
    pa_mainloop_api *pa_mlapi = pa_mainloop_get_api(pa_ml);
    context = pa_context_new(pa_mlapi, "test");

    pa_context_connect(context, NULL, 0, NULL);
    pa_context_set_state_callback(context, context_state_cb, NULL);

    if (pa_mainloop_run(pa_ml, &ret) < 0) {
        printf("pa_mainloop_run() failed.");
        exit(1);
    }

    return NULL;
}

void context_state_cb(pa_context *c, void *userdata)
{  
    switch (pa_context_get_state(c))
    {
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
	    break;
    case PA_CONTEXT_READY:
        printf("Server context is ready!\n");
        // load_module
        pa_context_load_module(context, "module-null-sink", "sink_name=testsink sink_properties=device.description=FM_Speakers", sink_ready_cb, NULL);
	    break;
    case PA_CONTEXT_FAILED:
	    return;
    case PA_CONTEXT_TERMINATED:
    default:
	    return;
    }
}

void sink_ready_cb(pa_context *c, uint32_t idx, void *userdata)
{
    if (idx == PA_INVALID_INDEX) {
        // TODO: PUSH SOME ERROR MESSAGE
        return;
    }
    printf("Sink created!\n");

    pa_operation *o = pa_context_get_sink_info_by_name(c, "testsink", sinkinfo_cb, NULL);
    pa_operation_unref(o);
}

void sinkinfo_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    if (eol == 0)
    {
        printf("Hello, get hype, alright!\n");
        printf("Index: %d\n", i->index);
        printf("Name: %s\n", i->name);
        printf("Description: %s\n", i->description);

        // START: Recording a stream
        pa_buffer_attr buffer_attr;

        if (!(stream = pa_stream_new(c, "CaptureStream", &sample_spec, NULL))) {
        printf("pa_stream_new() failed: %s", pa_strerror(pa_context_errno(c)));
        exit(1);
        }

        // Watch for changes in the stream state to create the output file
        pa_stream_set_state_callback(stream, stream_state_cb, NULL);
        
        // Watch for changes in the stream's read state to write to the output file
        pa_stream_set_read_callback(stream, stream_read_cb, NULL);

        // Set properties of the record buffer
        pa_zero(buffer_attr);
        buffer_attr.maxlength = (uint32_t) 8000;
        buffer_attr.tlength = (uint32_t) -1;
        buffer_attr.prebuf = (uint32_t) -1;
        buffer_attr.minreq = (uint32_t) -1;   
        buffer_attr.fragsize = (uint32_t) 8000;

        // and start recording
        if (pa_stream_connect_record(stream, i->monitor_source_name, &buffer_attr, flags) < 0) {
        printf("pa_stream_connect_record() failed: %s", pa_strerror(pa_context_errno(c)));
        exit(1);
        }
        // END
    }
}

void stream_state_cb(pa_stream *s, void *userdata)
{
    assert(s);

    switch (pa_stream_get_state(s)) {
    case PA_STREAM_CREATING:
        // The stream has been created, so 
        // let's open a file to record to
        printf("Creating stream\n");
        fdout = creat(fname,  0711);
        break;
    case PA_STREAM_TERMINATED:
        close(fdout);
        break;
    case PA_STREAM_READY:
        // Just for info: no functionality in this branch
        if (1) {
            const pa_buffer_attr *a;
            char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

            // printf("Stream successfully created.");

            if (!(a = pa_stream_get_buffer_attr(s)))
            {
                printf("pa_stream_get_buffer_attr() failed: %s", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            }
            else
            {
                printf("Buffer metrics: maxlength=%u, fragsize=%u", a->maxlength, a->fragsize);          
            }

            printf("Connected to device %s (%u, %ssuspended).\n",
            pa_stream_get_device_name(s),
            pa_stream_get_device_index(s),
            pa_stream_is_suspended(s) ? "" : "not ");

            SF_INFO localsfinfo;
            localsfinfo.samplerate  = 44100;
            // localsfinfo.frames     = 123456789;
            localsfinfo.channels    = 2;
            localsfinfo.format 	   = SF_FORMAT_IRCAM | SF_FORMAT_PCM_16;

            if(! (localinf = sf_open_fd(p_in[1], SFM_WRITE, &localsfinfo, 0))) {
                fprintf(stderr, "Error: could not open write pipe; %s.\n", sf_strerror (localinf)) ;
            } else {
                printf("Using write pipe.\n");
            }
            sf_close(localinf);
        }
        break;
    case PA_STREAM_FAILED:
    default:
        printf("Stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
        exit(1);
  }
}

void stream_read_cb(pa_stream *s, size_t length, void *userdata)
{
    assert(s);
    assert(length > 0);

    // Copy the data from the server out to a file
    // fprintf(stderr, "Can read %d\n", length);

    while (pa_stream_readable_size(s) > 0) {
        const void *data;
        size_t length;
        
        // peek actually creates and fills the data vbl
        if (pa_stream_peek(s, &data, &length) < 0) {
        fprintf(stderr, "Read failed\n");
        exit(1);
        return;
        }
        // fprintf(stderr, "Writing %d\n", length);
        // write(fdout, data, length); // writing to file? nah

        // TODO: check length (make math happen boi)
        // sf_write_raw(localinf, data, length);
        write(p_in[1], data, length);

        // swallow the data peeked at before
        pa_stream_drop(s);

        // pa_usec_t *latency;
        // int *negative;
        // pa_stream_get_latency(s, latency, negative);
        // printf("latency: %lld\n", *latency);
    }
}