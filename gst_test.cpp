#include <gst/gst.h>
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>
#include <fcntl.h>


void mem_usage(double &vm_usage, double &resident_set)
{
    vm_usage = 0.0;
    resident_set = 0.0;
    std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);
    std::string pid, comm, state, ppid, pgrp, session, tty_nr;
    std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    std::string utime, stime, cutime, cstime, priority, nice;
    std::string O, itrealvalue, starttime;
    unsigned long vsize;
    long rss;
    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime >> cutime >> cstime >> priority >> nice >> O >> itrealvalue >> starttime >> vsize >> rss;
    stat_stream.close();
    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
    vm_usage = vsize / 1024.0;
    resident_set = rss * page_size_kb;
}

typedef struct _CustomData
{
    GstElement *pipeline;
    GstElement *source;
    GstElement *convert;
    GstElement *resample;
    GstElement *sink;

    GstElement *src_2;
    GstElement *conv_2;
    GstElement *sink_2;
} CustomData;

static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data)
{
    GstPad *sink_pad = gst_element_get_static_pad(data->convert, "sink");
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    if (gst_pad_is_linked(sink_pad))
    {
        goto exit;
    }

    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (g_str_has_prefix(new_pad_type, "audio/x-raw"))
    {
        goto exit;
    }

    std::cout << "ok\n";
    gst_pad_link(new_pad, sink_pad);

exit:
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);

    gst_object_unref(sink_pad);
}

int main(int argc, char *argv[])
{
    char buff[512];

    int fdmem = open("mem.csv", O_RDWR | O_APPEND);
    int fdvmem = open("vmem.csv", O_RDWR | O_APPEND);

    double vm, rss;
    CustomData data;
    gboolean terminate = FALSE;

    gst_init(&argc, &argv);

    data.source = gst_element_factory_make("uridecodebin", "source");
    data.convert = gst_element_factory_make("videoconvert", "convert");
    data.sink = gst_element_factory_make("fakesink", "sink");

    data.src_2 = GST_ELEMENT(gst_object_ref(data.source));
    data.conv_2 = GST_ELEMENT(gst_object_ref(data.convert));
    data.sink_2 = GST_ELEMENT(gst_object_ref(data.sink));

    data.pipeline = gst_pipeline_new("test-pipeline");

    if (!data.pipeline || !data.source || !data.sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    gst_bin_add_many(GST_BIN(data.pipeline), data.src_2, data.conv_2, data.sink_2, NULL);
    if (!gst_element_link_many(data.conv_2, data.sink_2, NULL))
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    g_object_set(data.source, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);

    sleep(1);
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

    do
    {
        mem_usage(vm, rss);

        sprintf(buff, "%f,", rss);
        write(fdmem, buff, strlen(buff));
        sprintf(buff, "%f,", vm);
        write(fdvmem, buff, strlen(buff));

        sleep(5);
        gst_element_set_state(data.pipeline, GST_STATE_NULL);

        sleep(5);
        gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

        while (data.pipeline->current_state != GST_STATE_PLAYING)
            ;


    } while (!terminate);

    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0;
}