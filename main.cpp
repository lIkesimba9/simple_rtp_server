#include <iostream>
#include <gst/gst.h>

#include <gst/rtp/rtp.h>
#include <time.h>
using namespace std;

/*
 * Sender example setup
 * sends the output of v4l2src as h264 encoded RTP on port 5000, RTCP is sent on
 * port 5001. The destination is 127.0.0.1.
 * the video receiver RTCP reports are received on port 5005
 *
 * .-------.    .-------.    .-------.      .----------.     .-------.
 * |v4l2   |    |x264enc|    |h264pay|      | rtpbin   |     |udpsink|  RTP
 * |      src->sink    src->sink    src->send_rtp send_rtp->sink     | port=5000
 * '-------'    '-------'    '-------'      |          |     '-------'
 *                                          |          |
 *                                          |          |     .-------.
 *                                          |          |     |udpsink|  RTCP
 *                                          |    send_rtcp->sink     | port=5001
 *                           .--------.     |          |     '-------' sync=false
 *                RTCP       |udpsrc  |     |          |               async=false
 *                port=5005  |       src->recv_rtcp    |
 *                           '--------'     '----------'
 */




static gboolean bus_call (GstBus     *bus,
                         GstMessage *msg,
                         gpointer    data)
{
    GMainLoop *loop = (GMainLoop *)data;

    switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
        g_print ("End-of-stream\n");
        g_main_loop_quit (loop);
        break;
    case GST_MESSAGE_ERROR: {
        gchar *debug = NULL;
        GError *err = NULL;

        gst_message_parse_error (msg, &err, &debug);

        g_print ("Error: %s\n", err->message);
        g_error_free (err);

        if (debug) {
            g_print ("Debug details: %s\n", debug);
            g_free (debug);
        }

        g_main_loop_quit (loop);
        break;
    }
    default:
        break;
    }

    return TRUE;
}


static gboolean process_rtcp_packet(GstRTCPPacket *packet){
    guint32 ssrc;
    guint count, i;

    count = gst_rtcp_packet_get_rb_count(packet);
    cerr << "    count " << count;
    for (i=0; i<count; i++) {
        guint32 exthighestseq, jitter, lsr, dlsr;
        guint8 fractionlost;
        gint32 packetslost;

        gst_rtcp_packet_get_rb(packet, i, &ssrc, &fractionlost,
                               &packetslost, &exthighestseq, &jitter, &lsr, &dlsr);

        cerr << "    block " << i;
        cerr << "    ssrc " << ssrc;
        cerr << "    highest seq " << exthighestseq;
        cerr << "    jitter " << jitter;
        cerr << "    fraction lost " << fractionlost;
        cerr << "    packet lost " << packetslost;
        cerr << "    lsr " << lsr;
        cerr << "    dlsr " << dlsr;

        //        rtcp_pkt->fractionlost = fractionlost;
        //        rtcp_pkt->jitter = jitter;
        //        rtcp_pkt->packetslost = packetslost;
    }

    //cerr << "Received rtcp packet");

    return TRUE;
}


static gboolean cb_receive_rtcp(GstElement *rtpsession, GstBuffer *buf, gpointer data){

    GstRTCPBuffer rtcpBuffer = GST_RTCP_BUFFER_INIT;
    //    GstRTCPBuffer *rtcpBuffer = (GstRTCPBuffer*)malloc(sizeof(GstRTCPBuffer));
    //    rtcpBuffer->buffer = nullptr;
    GstRTCPPacket *rtcpPacket = (GstRTCPPacket*)malloc(sizeof(GstRTCPPacket));


    if (!gst_rtcp_buffer_validate(buf))
    {
        cerr << "Received invalid RTCP packet" << endl;
    }

    cerr << "Received rtcp packet" << "\n";


    gst_rtcp_buffer_map (buf,(GstMapFlags)(GST_MAP_READ),&rtcpBuffer);
    gboolean more = gst_rtcp_buffer_get_first_packet(&rtcpBuffer,rtcpPacket);
    while (more) {
        GstRTCPType type;

        type = gst_rtcp_packet_get_type(rtcpPacket);
        switch (type) {
        case GST_RTCP_TYPE_RR:
            process_rtcp_packet(rtcpPacket);
            //   gst_rtcp_buffer_unmap (&rtcpBuffer);
            //g_debug("RR");
            //send_event_to_encoder(venc, &rtcp_pkt);
            break;
        default:
            cerr << "Other types" << endl;
            break;
        }
        more = gst_rtcp_packet_move_to_next(rtcpPacket);
    }

    free(rtcpPacket);
    return TRUE;
}
bool linkStaticAndRequestPads(GstElement *sourse,GstElement *sink,gchar *nameSrcPad,gchar *nameSinkPad)
{

    GstPad *srcPad = gst_element_get_static_pad(sourse,nameSrcPad);
    GstPad *sinkPad = gst_element_get_request_pad(sink,nameSinkPad);
    GstPadLinkReturn ret_link = gst_pad_link(srcPad,sinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, static and request pad\n";
        return false;
    }
    gst_object_unref(GST_OBJECT(srcPad));
    gst_object_unref(GST_OBJECT(sinkPad));
    return true;
}
bool linkRequestAndStaticPads(GstElement *sourse,GstElement *sink,gchar *nameSrcPad,gchar *nameSinkPad)
{


    GstPad *srcPad = gst_element_get_request_pad(sourse,nameSrcPad);
    GstPad *sinkPad = gst_element_get_static_pad(sink,nameSinkPad);
    GstPadLinkReturn ret_link = gst_pad_link(srcPad,sinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, request and statitc pad\n";
        return false;
    }
    gst_object_unref(GST_OBJECT(srcPad));
    gst_object_unref(GST_OBJECT(sinkPad));
    return true;
}

static GstPadProbeReturn cb_add_time_to_rtp_packet(GstPad *pad, GstPadProbeInfo *info, gpointer *data) {

    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GstRTPBuffer rtp_buffer;
    memset(&rtp_buffer, 0, sizeof(GstRTPBuffer));

    if (buffer != NULL) {
        struct timespec ts;
        timespec_get(&ts, TIME_UTC);
        gconstpointer nsec = &ts.tv_nsec;
        gconstpointer sec = &ts.tv_sec;
        if (gst_rtp_buffer_map(buffer, (GstMapFlags)GST_MAP_READWRITE, &rtp_buffer)) {
            //gst_rtp_buffer_add_extension_onebyte_header()

            gst_rtp_buffer_add_extension_twobytes_header(&rtp_buffer, 0, 1, nsec, sizeof(ts.tv_nsec));
            gst_rtp_buffer_add_extension_twobytes_header(&rtp_buffer, 0, 2, sec, sizeof(ts.tv_sec));
            std::cout << "sec: " << ts.tv_sec << " " << "nanosec: " << ts.tv_nsec <<  std::endl;
        }
        else {
            cerr << "RTP buffer not mapped\n";
        }
    }
    else {
        cerr << "Gst buffer is NULL\n";
    }
    gst_rtp_buffer_unmap(&rtp_buffer);
}



GstElement *create_pipeline(){
    GstElement *pipeline,*source,*videconverter,
        *x264encoder,*rtph264pay,
        *rtpbin,*udpSinkRtp;
    GstElement *udpSrcRtcp,*udpSinkRtcp;

    pipeline = gst_pipeline_new("rtpStreamer");

    // Создаю элемент захвата видео с камеры.
    source = gst_element_factory_make("v4l2src","source");
    // Создаю элелмент который преобразует данные с камеры, к данным которые поддерживает кодировщик.
    videconverter = gst_element_factory_make("videoconvert","converter");
    // Создаю кодек
    x264encoder = gst_element_factory_make("x264enc","encoder");
    // Создаю элемент который упакуют донные от кодера, в rtp пакеты
    rtph264pay = gst_element_factory_make("rtph264pay","rtppay");
    // Создаю элемент управющий rtp сесией
    rtpbin = gst_element_factory_make("rtpbin","rtpbin");
    // Создаю udp сток для отпраки rtp пакетов
    udpSinkRtp = gst_element_factory_make("udpsink","udpSinkRtp");
    // Создаю upd сток для отпраки rtcp пакетов
    udpSinkRtcp = gst_element_factory_make("udpsink","udpSinkRtcp");
    // Создаю udp источник для принятия rtcp пакетов.
    udpSrcRtcp = gst_element_factory_make("udpsrc","udpSrcRtcp");

    if (!pipeline || !source || !videconverter || !x264encoder || !rtph264pay || !rtpbin || !udpSinkRtp || !udpSinkRtcp || !udpSrcRtcp)
    {
        cerr << "Not all elements could be created.\n";
        return NULL;
    }
    // Задаю устройство с которог захватывать видео
    g_object_set(G_OBJECT(source),"device","/dev/video0",NULL);
    // Устанавливаю параметры кодека.
    g_object_set(G_OBJECT(x264encoder),"tune",0x00000004,NULL);
    g_object_set(G_OBJECT(x264encoder),"bitrate",500,NULL);
    //   g_object_set(G_OBJECT(x264encoder),"threads",2,NULL);

    // Устанавливаю параметры для upd сойденений.
    g_object_set(G_OBJECT(udpSinkRtp),"host","127.0.0.1",NULL);
    g_object_set(G_OBJECT(udpSinkRtp),"port",5000,NULL);

    g_object_set(G_OBJECT(udpSinkRtcp),"host","127.0.0.1",NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"port",5001,NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"sync",FALSE,NULL);
    g_object_set(G_OBJECT(udpSinkRtcp),"async",FALSE,NULL);

    g_object_set(G_OBJECT(udpSrcRtcp),"address","127.0.0.1",NULL);
    g_object_set(G_OBJECT(udpSrcRtcp),"port",5005,NULL);
    g_object_set(G_OBJECT (udpSrcRtcp), "caps", gst_caps_from_string("application/x-rtcp"), NULL);


    // Добавляю элементы в контейнер
    gst_bin_add_many(GST_BIN(pipeline),source,videconverter,
                     x264encoder,rtph264pay,rtpbin,
                     udpSinkRtp,udpSinkRtcp,udpSrcRtcp,NULL);


    // Создаю caps для того, чтобы согласовать параметры захвата видео с камеры и кодировщика.
    GstCaps *capV4l2VideoConverter;
    capV4l2VideoConverter = gst_caps_new_simple ("video/x-raw",
                                                "format",G_TYPE_STRING,"YUY2",
                                                "framerate", GST_TYPE_FRACTION, 30, 1,
                                                NULL);
    if (!capV4l2VideoConverter){
        cerr << "Error create caps\n";
        return NULL;
    }
    GstCaps *capVideoConverterEncoder;
    capVideoConverterEncoder  = gst_caps_new_simple ("video/x-raw",
                                                   "format",G_TYPE_STRING,"I420",
                                                   "framerate", GST_TYPE_FRACTION, 30, 1,
                                                   NULL);
    if (!capVideoConverterEncoder){
        cerr << "Error create caps\n";
        return NULL;
    }


    // Связваю все элементы.
    if (!gst_element_link_filtered(source,videconverter,capV4l2VideoConverter))
    {
        cerr << "Elements could not be linked source and videoconv.\n";
        return NULL;
    }

    if (!gst_element_link_filtered(videconverter,x264encoder,capVideoConverterEncoder))
    {
        cerr << "Elements could not be linked videoconv and x264.\n";
        return NULL;
    }



    if (!gst_element_link_many(x264encoder,rtph264pay,NULL))
    {
        cerr << "Elements could not be linked other.\n";
        return NULL;

    }

    if (!linkStaticAndRequestPads(rtph264pay,rtpbin,"src","send_rtp_sink_%u"))
    {
        cerr << "Error create link, beetwen rtph264pay and rtpbin\n";
        return NULL;
    }

    if (!gst_element_link(rtpbin,udpSinkRtp))
    {
        cerr << "Elements could not be linked rtpbin and udpSinkRtp.\n";
    }

    if (!linkRequestAndStaticPads(rtpbin,udpSinkRtcp,"send_rtcp_src_%u","sink"))
    {
        cerr << "Error create link, beetwen rtpbin and udpSinkRtcp\n";
        return NULL;
    }


    if (!linkStaticAndRequestPads(udpSrcRtcp,rtpbin,"src","recv_rtcp_sink_%u"))
    {
        cerr << "Error create link, beetwen udpSrcRtcp and rtpbin\n";
        return NULL;
    }
    //Пад для расширения заголовка RTP.
    GstPad *rtpPaySrcPad = gst_element_get_static_pad(rtph264pay, "src");
    gst_pad_add_probe(rtpPaySrcPad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)cb_add_time_to_rtp_packet, NULL, NULL);
    gst_object_unref(GST_OBJECT(rtpPaySrcPad));

    // Подключаю сигнал по обработке принятых rtcp пакетов.
    GObject *session;
    g_signal_emit_by_name (rtpbin, "get-internal-session", 0, &session);
    g_signal_connect_after (session, "on-receiving-rtcp",
                           G_CALLBACK (cb_receive_rtcp), NULL);
    g_object_unref(session);




    return pipeline;

}


int main(int argc, char *argv[])
{

    gst_init(0,0);

    GMainLoop *loop;
    loop = g_main_loop_new(NULL,FALSE);
    GstBus *bus;

    GstElement *pipeline = create_pipeline();
    if (pipeline == NULL)
    {
        cerr << "Error create pipeline!" << endl;
        return -1;
    }

    guint watch_id;
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);


    GstStateChangeReturn ret;
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);

    g_main_loop_run (loop);



    /* clean up */
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    g_source_remove (watch_id);
    g_main_loop_unref (loop);


    return 0;
}
