#!/bin/bash
rrdtool graph temp_graph.svg \
-w 785 -h 240 -a SVG \
--slope-mode \
--vertical-label "temperature (°C) / Humidity (%)" \
--right-axis 30:0 \
DEF:temp=climate.rrd:temp:AVERAGE \
DEF:humi=climate.rrd:humi:AVERAGE \
DEF:pres=climate.rrd:pres:AVERAGE \
CDEF:scaled_pres=pres,30,/ \
LINE1:temp#ff0000:"Temperature\t" \
GPRINT:temp:LAST:"Cur\: %5.2lf\t" \
GPRINT:temp:AVERAGE:"Avg\: %5.2lf\t" \
GPRINT:temp:MAX:"Max\: %5.2lf\t" \
GPRINT:temp:MIN:"Min\: %5.2lf\n" \
LINE1:humi#0000ff:"Humidity\t" \
GPRINT:humi:LAST:"Cur\: %5.2lf\t" \
GPRINT:humi:AVERAGE:"Avg\: %5.2lf\t" \
GPRINT:humi:MAX:"Max\: %5.2lf\t" \
GPRINT:humi:MIN:"Min\: %5.2lf\n" \
LINE1:scaled_pres#00ff00:"Pressure\t" \
GPRINT:pres:LAST:"Cur\: %5.2lf\t" \
GPRINT:pres:AVERAGE:"Avg\: %5.2lf\t" \
GPRINT:pres:MAX:"Max\: %5.2lf\t" \
GPRINT:pres:MIN:"Min\: %5.2lf\n" \

