#include <iostream>
#include <gst/gst.h>

#include <gst/rtp/rtp.h>

using namespace std;
/*gst-launch-1.0
 * v4l2src !
 * videoconvert !
 * video/x-raw,framerate=30/1,format=I420
 * ! x264enc tune=zerolatency  byte-stream=true bitrate=1500 threads=2
 * ! rtph264pay pt=96 - default value
 * ! .send_rtp_sink rtpsession .send_rtp_src !
 * udpsink host=127.0.0.1 port=5000 -v
*/
/*PEER_V=9004 PEER_IP=127.0.0.1 \
 *SELF_PATH="video.mp4" \
 *SELF_V=5004 SELF_VSSRC=112233 \
 *bash -c 'gst-launch-1.0 -e \
 *rtpsession name=r sdes="application/x-rtp-source-sdes,cname=(string)\"user\@example.com\"" \
 *v4l2src ! videoconvert ! video/x-raw,framerate=30/1,format=I420 \
 *! x264enc tune=zerolatency \
 *! rtph264pay ! "application/x-rtp,payload=(int)103,clock-rate=(int)90000,ssrc=(uint)$SELF_VSSRC" \
 *! r.send_rtp_sink \
 *r.send_rtp_src ! udpsink host=$PEER_IP port=$PEER_V \
 *r.send_rtcp_src ! udpsink host=$PEER_IP port=$((PEER_V+1)) sync=false async=false \
 *udpsrc port=$((SELF_V+1)) ! tee name=t \
 *t. ! queue ! r.recv_rtcp_sink \
 *t. ! queue ! fakesink dump=true async=false'
*/

void OnPadAdded(GstElement *element, GstPad *pad, gpointer data) {
    GstPad *sinkpad;
    //узнаю навания элементов котоыре передаются.
    gchar *name = gst_element_get_name(element);
    g_print("Element name is %s\n",name);
    gchar *pad_name = GST_PAD_NAME(pad);
    g_print("Element pad is %s\n",pad_name);
    //узнаю навания элементов котоыре передаются.
    GstElement *udpsink = (GstElement *) data;
    gchar *name_decoder = gst_element_get_name(udpsink);
    g_print("Element name is %s\n",name_decoder);


    sinkpad = gst_element_get_static_pad(udpsink, "sink");

    GstPadLinkReturn ret = gst_pad_link(pad, sinkpad);

    gst_object_unref(sinkpad);
}

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


static gboolean
process_rtcp_packet(GstRTCPPacket *packet){
    guint32 ssrc, rtptime, packet_count, octet_count;
    guint64 ntptime;
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
    g_object_set(G_OBJECT(x264encoder),"bitrate",1500,NULL);
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


// Связваю все элементы до кодировщика.
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
    GstPad *sendRtpSinkPad = gst_element_get_request_pad(rtpbin,"send_rtp_sink_%u");
    GstPad *rtpPaySrcPad = gst_element_get_static_pad(rtph264pay,"src");
    GstPadLinkReturn ret_link = gst_pad_link(rtpPaySrcPad,sendRtpSinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, beetwen sendRtpSinkPad and rtpPaySrcPad\n";
        return NULL;
    }
    gst_object_unref(GST_OBJECT(sendRtpSinkPad));
    gst_object_unref(GST_OBJECT(rtpPaySrcPad));

    if (!gst_element_link(rtpbin,udpSinkRtp))
    {
        cerr << "Elements could not be linked rtpbin and udpSinkRtp.\n";
    }


    GstPad *sendRtcpSrcPad = gst_element_get_request_pad(rtpbin,"send_rtcp_src_%u");
    GstPad *udpSinkPad = gst_element_get_static_pad(udpSinkRtcp,"sink");
    ret_link = gst_pad_link(sendRtcpSrcPad, udpSinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, beetwen sendRtcpSrcPad and udpSinkPad\n";
        return NULL;
    }
    gst_object_unref(GST_OBJECT(sendRtcpSrcPad));
    gst_object_unref(GST_OBJECT(udpSinkPad));

    GstPad *recvRtcpSinkPad = gst_element_get_request_pad(rtpbin,"recv_rtcp_sink_%u");
    GstPad *udpSrcPad = gst_element_get_static_pad(udpSrcRtcp,"src");
    ret_link = gst_pad_link(udpSrcPad, recvRtcpSinkPad);
    if (ret_link != GST_PAD_LINK_OK)
    {
        cerr << "Error create link, beetwen udpSrcPad and recvRtcpSinkPad\n";
        return NULL;
    }

    gst_object_unref(GST_OBJECT(recvRtcpSinkPad));
    gst_object_unref(GST_OBJECT(udpSrcPad));

      GObject *session;


      g_signal_emit_by_name (rtpbin, "get-internal-session", 0, &session);
      g_signal_connect_after (session, "on-receiving-rtcp",
                               G_CALLBACK (cb_receive_rtcp), NULL);
     // g_object_unref(session);

    return pipeline;

}


int main()
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
