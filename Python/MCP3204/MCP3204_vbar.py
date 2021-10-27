#!/usr/bin/env python

import paho.mqtt.client as mqtt
import json
import math
import pandas as pd
import numpy as np
import queue
import sys
import logging
import __main__ as main

import datetime

# Check if we're executed interactively (Jupyter etc)
interactive_mode = bool(getattr(sys, 'ps1', sys.flags.interactive))
if interactive_mode:
    from bokeh.plotting import figure, output_file, show, ColumnDataSource
    from bokeh.models import CustomJS, Button
    from bokeh.models.sources import ColumnDataSource
    from bokeh.layouts import row, column
    from bokeh.io import output_notebook, show, push_notebook, curdoc
    from bokeh.palettes import Dark2_5 as palette
    from bokeh.resources import INLINE
    output_notebook(resources=INLINE)


################################# Settings BEGIN
# Only set these variables, if they haven't been set before
if not 'config' in locals():
    ## Don't modify this line! :)
    config = {}

    ## MQTT - our client id
    config['mqtt_id'] = 'MCP3204-Reader'

    ## MQTT - broker ip / hostname
    config['mqtt_broker'] = 'iot.fh-muenster.de'

    ## MQTT - broker username / password
    config['mqtt_broker_user'] = 'MQTT_USER'
    config['mqtt_broker_pass'] = 'MQTT_PASSWORD'

    ## MQTT - empty sset of topics
    config['mqtt_topics'] = set([])

    ## MQTT - subscribed topics
    config['mqtt_topics'].add('sensor/3C:71:BF:AA:7B:18')
    #config['mqtt_topics'].add('sensor/60:01:94:4A:A8:95')

    ## Recording - stop the program if the defined amount of samples has been recorded (0 = unlimited)
    config['record_sample_limit'] = 0

    ## Recording - stop the program if the defined amount of seconds has passed (0 = unlimited)
    config['record_duration_limit'] = 10

    ## Display - upper limit of samples displayed in the bokeh graph
    config['bokeh_stream_rollover'] = 250

    ## Export formats (True/False)
    config['export_json']  = False
    config['export_csv']   = False
    config['export_excel'] = False

################################# Settings END

# Initialize logging facility
logger = logging.getLogger()
logger.setLevel(logging.INFO)
mqtt_queue = queue.Queue()

sensor_meta = {}

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, config, flags, rc):
    logging.info('MQTT connected - RC ' + str(rc))
    #print(config)
    # Subscribing in on_connect() means that if we lose the connection and reconnect then subscriptions will be renewed.
    for topic in config['mqtt_topics']:
        logging.debug('MQTT subscribing to: ' + topic)
        client.subscribe(topic)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    # Put message in MQTT message queue
    mqtt_queue.put(msg)

def record_and_display(config):
    sample_index = 0
    start_time = datetime.datetime.now()
    done = False
    pd_source = pd.DataFrame(columns=['datetime', 'sensor', 'mv_c0', 'mv_c1', 'mv_c2', 'mv_c3'])

    logging.debug('Starting program')

    ####################################
    if interactive_mode:
        bokeh_figure = figure(title='MCP3204 Voltage Plot', sizing_mode='stretch_width', x_axis_type='datetime', plot_height = 400, tools = 'pan, wheel_zoom, box_zoom, reset, crosshair, hover, save')
        bokeh_csource = {}
        
        channels = ['c0', 'c1', 'c2', 'c3'];
        voltages = [1,2,3,4]
        
        #bokeh_csource[next(iter(config['mqtt_topics']))] = ColumnDataSource(data = dict(x = [], mv_c0 = [], mv_c1 = [], mv_c2 = [], mv_c3 = []))
        bokeh_csource = ColumnDataSource(data = dict(x = [], mv_c0 = [], mv_c1 = [], mv_c2 = [], mv_c3 = []))
        
        bokeh_figure.vbar(x=channels, top=voltages, width=0.9, source=bokeh_csource, line_color='white')
        #for channel, color in zip(channels, palette):
         #   bokeh_figure.line(x = 'x', y = 'mv_' + channel, source = bokeh_csource, color = color)
          #  bokeh_figure.circle(x = 'x', y = 'mv_' + channel, source = bokeh_csource, legend_label = channel, color = color, fill_color = 'white')

        bokeh_figure.yaxis.axis_label = 'Voltage (mV)'
        bokeh_figure.xaxis.axis_label = 'Time'
        bokeh_handle = show(bokeh_figure, notebook_handle = True)

    mqtt_client = mqtt.Client(client_id = config['mqtt_id'], userdata=config)
    mqtt_client.username_pw_set(config['mqtt_broker_user'], config['mqtt_broker_pass'])
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(config['mqtt_broker'], 1883, 60)
    mqtt_client.loop_start()

    logging.debug('Starting loop')

    while not done:


        if config['record_sample_limit'] > 0 and sample_index > config['record_sample_limit']:
            logging.info('Reached sample limit, stopping')
            done = True

        if config['record_duration_limit'] > 0 and (datetime.datetime.now() - start_time) > datetime.timedelta(seconds=config['record_duration_limit']):
            logging.info('Reached runtime limit, stopping')
            done = True

        try:
            # Receive message from queue
            msg = mqtt_queue.get(block = True, timeout = 0.1)

            # We expect JSON payload
            sensor_data = json.loads(msg.payload)

            # Check if we have all required fields
            if not 'mv_c0' in sensor_data:
                logging.error('missing key in sensor_data:')
                logging.error(sensor_data)
                continue

            if sensor_data['mv_c0'] is None:
                logging.error('Sensor value error:')
                logging.error(sensor_data)
                continue

            if msg.topic in sensor_meta and True:
                # Rollover
                if sensor_data['uptime'] < sensor_meta[msg.topic]['fist_uptime']:
                    sensor_meta[msg.topic]['first_datetime'] = datetime.datetime.now()
                    sensor_meta[msg.topic]['fist_uptime'] = sensor_data['uptime']

                sensor_value_datetime = sensor_meta[msg.topic]['first_datetime'] - datetime.timedelta(milliseconds=sensor_meta[msg.topic]['fist_uptime']) + datetime.timedelta(milliseconds=sensor_data['uptime'])
            else:
                sensor_meta[msg.topic] = {}
                sensor_meta[msg.topic]['first_datetime'] = datetime.datetime.now()
                sensor_meta[msg.topic]['fist_uptime'] = sensor_data['uptime']
                sensor_value_datetime = datetime.datetime.now()
            
            # Build object for pandas dataframe and append
            if config['export_json'] or config['export_csv'] or config['export_excel']:
                pd_data = {}
                pd_data['datetime'] = sensor_value_datetime
                pd_data['sensor']   = msg.topic
                pd_data['mv_c0']    = float(sensor_data['mv_c0'])
                pd_data['mv_c1']    = float(sensor_data['mv_c1'])
                pd_data['mv_c2']    = float(sensor_data['mv_c2'])
                pd_data['mv_c3']    = float(sensor_data['mv_c3'])

                pd_source.loc[len(pd_source)] = pd_data
            
            sample_index = sample_index + 1

        except queue.Empty:
            #print('NOTE - timeout!')
            continue
        except json.JSONDecodeError:
            logging.error('JSONDecodeError')
            done = True
            continue;
        except ValueError:
            logging.error('ValueError')
            done = True
            continue;
        except KeyboardInterrupt:
            logging.warning('Keyboard interrup!');
            done = True;
            continue;
        except Exception as e:
            logging.error('Unexpected error!')
            print(e)
            done = True
            continue;

        if interactive_mode:
            bokeh_data = {}
            bokeh_data['x'] = [sensor_value_datetime]
            bokeh_data['mv_c0'] = [float(sensor_data['mv_c0'])]
            bokeh_data['mv_c1'] = [float(sensor_data['mv_c1'])]
            bokeh_data['mv_c2'] = [float(sensor_data['mv_c2'])]
            bokeh_data['mv_c3'] = [float(sensor_data['mv_c3'])]

            #print(bokeh_data)
            #bokeh_csource[msg.topic].stream(bokeh_data, config['bokeh_stream_rollover'])
            bokeh_csource.stream(bokeh_data, config['bokeh_stream_rollover'])
            push_notebook(handle = bokeh_handle)
        else:
            print('{sensor} - {temp:.2f} C'.format(sensor=msg.topic, temp=float(sensor_data['mv_c0'])))


    # We are done, disconnect MQTT client
    mqtt_client.disconnect()

    # Export collected data
    if config['export_csv']:
        pd_source.to_csv(datetime.datetime.now().strftime('%Y%m%d%H%M%S') + '.csv')
        logging.info('Data written to ' + datetime.datetime.now().strftime('%Y%m%d%H%M%S') + '.csv')

    if config['export_json']:
        pd_source.to_json(datetime.datetime.now().strftime('%Y%m%d%H%M%S') + '.json')
        logging.info('Data written to ' + datetime.datetime.now().strftime('%Y%m%d%H%M%S') + '.json')

    if config['export_excel']:
        pd_source.to_excel(datetime.datetime.now().strftime('%Y%m%d%H%M%S') + '.xlsx')
        logging.info('Data written to ' + datetime.datetime.now().strftime('%Y%m%d%H%M%S') + '.xlsx')

    mqtt_client.disconnect();

    logging.info('Finished!')

# Run if there's no start button
#if not ('button_run') in locals():
#    record_and_display(config)
if __name__ == "__main__":
    record_and_display(config)