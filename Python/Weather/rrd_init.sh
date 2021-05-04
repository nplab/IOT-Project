#!/bin/bash
rrdtool create climate.rrd \
        --step 300 \
        DS:temp:GAUGE:600:0:50 \
        DS:humi:GAUGE:600:0:100 \
        DS:pres:GAUGE:600:0:2000 \
        DS:gasr:GAUGE:600:0:1000 \
        RRA:AVERAGE:0.5:1:1000 \
        RRA:MIN:0.5:1:1000 \
        RRA:MAX:0.5:1:1000 \
        RRA:AVERAGE:0.5:12:1000 \
        RRA:MIN:0.5:12:1000 \
        RRA:MAX:0.5:12:1000 \
        RRA:AVERAGE:0.5:288:1000 \
        RRA:MIN:0.5:288:1000 \
        RRA:MAX:0.5:288:1000