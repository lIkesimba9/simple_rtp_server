PEER_V=5004 PEER_IP=127.0.0.1 \
SELF_V=5000 \
CAPS_V="media=(string)video,clock-rate=(int)90000,encoding-name=(string)H264,payload=(int)103" \
bash -c 'gst-launch-1.0 -e \
  rtpsession name=r sdes="application/x-rtp-source-sdes,cname=(string)\"user\@example.com\"" \
  udpsrc port=$SELF_V ! "application/x-rtp,$CAPS_V" ! r.recv_rtp_sink \
    r.recv_rtp_src ! rtph264depay ! decodebin ! autovideosink \
  udpsrc port=$((SELF_V+1)) ! r.recv_rtcp_sink \
  r.send_rtcp_src ! udpsink host=$PEER_IP port=$((PEER_V+1)) sync=false async=false'

