#include "pulse_virtual.h"

// global vars
int ret;
int *p;
pa_context *context;
uint32_t module_idx;
static pa_stream *stream = NULL;
static pa_stream_flags_t flags = PA_STREAM_ADJUST_LATENCY;
SNDFILE *writeinf;
static pa_sample_spec sample_spec = {
  .format = PA_SAMPLE_S16LE,
  .rate = 44100,
  .channels = 2
};

void* pulse_virtual(void *pipe)
{    
    setup_cleanup();
    
    p = (int*)pipe;
    pa_mainloop *pa_ml = pa_mainloop_new();
    pa_mainloop_api *pa_mlapi = pa_mainloop_get_api(pa_ml);
    context = pa_context_new(pa_mlapi, "pifmrds");

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
        pa_context_load_module(context, "module-null-sink", "sink_name=pifmsink sink_properties=device.description=FM_Speakers", sink_ready_cb, NULL);
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
        fprintf(stderr, "Error: invalid index in sink_ready_cb().\n") ;
        return;
    }
    module_idx = idx;
    printf("Sink created!\n");

    pa_operation *o = pa_context_get_sink_info_by_name(c, "pifmsink", sinkinfo_cb, NULL);
    pa_operation_unref(o);
}

void sinkinfo_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    if (eol == 0)
    {
        printf("Sink info:\n");
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
        buffer_attr.maxlength = (uint32_t) -1;
        buffer_attr.tlength = (uint32_t) -1;
        buffer_attr.prebuf = (uint32_t) -1;
        buffer_attr.minreq = (uint32_t) -1;   
        buffer_attr.fragsize = (uint32_t) 1024;

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
        printf("Creating stream\n");
        break;
    case PA_STREAM_TERMINATED:
        break;
    case PA_STREAM_READY:
        if (1) {
            const pa_buffer_attr *a;
            
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

            // TODO: getting stream info
            // const pa_format_info* s_info = pa_stream_get_format_info(s);

            SF_INFO localsfinfo;
            localsfinfo.samplerate = 44100;
            localsfinfo.channels = 2;
            localsfinfo.format = SF_FORMAT_IRCAM | SF_FORMAT_PCM_16;

            // This is just to write file header (sndfile complains on read otherwise)
            if(! (writeinf = sf_open_fd(p[1], SFM_WRITE, &localsfinfo, 0))) {
                fprintf(stderr, "Error: could not open write pipe; %s.\n", sf_strerror (writeinf)) ;
            } else {
                printf("Using write pipe.\n");
            }
            sf_close(writeinf);
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
    // printf("Can read %d\n", length);

    while (pa_stream_readable_size(s) > 0) {
        const void *data;
        size_t length;
        
        // peek actually creates and fills the data vbl
        if (pa_stream_peek(s, &data, &length) < 0) {
            fprintf(stderr, "Read failed\n");
            exit(1);
            return;
        }

        // printf("Writing %d\n", length);
        // TODO: check length (make math happen boi)
        // sf_write_raw(writeinf, data, length);
        write(p[1], data, length);

        pa_usec_t latency = 0;
        pa_stream_get_latency(s, &latency, NULL);
        // printf("latency: %lld\n", latency);

        // swallow the data peeked at before
        pa_stream_drop(s);
    }
}

void stream_flushed_cb(pa_stream *s, int success, void *userdata)
{
    // printf("stream flushed\n");
}

void sink_unload_cb(pa_context *c, int success, void *userdata)
{
    printf("\nSink module unloaded successfully!\nExiting...\n");
    pthread_exit(0);
}

void setup_cleanup()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = cleanup_handler;
    sigaction(SIGPA, &sa, NULL);
}

void cleanup_handler()
{
    pa_context_unload_module(context, module_idx, sink_unload_cb, NULL);
}