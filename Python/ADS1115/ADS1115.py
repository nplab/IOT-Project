import paho.mqtt.client as mqtt
import json
import time
import math
import pandas as pd
import numpy as np
import queue
import sys
import logging
import __main__ as main

from datetime import datetime


# Check if we're executed interactively (Jupyter etc)
interactive_mode = bool(getattr(sys, 'ps1', sys.flags.interactive))
if interactive_mode:
    from bokeh.plotting import figure, output_file, show, ColumnDataSource
    from bokeh.models import CustomJS, Button
    from bokeh.models.sources import ColumnDataSource
    from bokeh.layouts import row, column
    from bokeh.io import output_notebook, show, push_notebook, curdoc
    from bokeh.palettes import Dark2_5 as palette
    output_notebook()
    

################################# Settings BEGIN
# Only set these variables, if they haven't been set before
if not ('config') in locals():
    ## Don't modify this line! :)
    config = {}

    ## MQTT - our client id
    config['mqtt_id'] = 'ADS1115-Reader'

    ## MQTT - broker ip / hostname
    config['mqtt_broker'] = 'iot.fh-muenster.de'

    ## MQTT - broker username / password
    config['mqtt_broker_user'] = 'MQTT_USER'
    config['mqtt_broker_pass'] = 'MQTT_PASSWORD'

    ## MQTT - empty sset of topics
    config['mqtt_topics'] = set([])

    ## MQTT - subscribed topics
    config['mqtt_topics'].add('sensor/80:7D:3A:B7:4F:18')

    ## Recording - stop the program if the defined amount of samples has been recorded (0 = unlimited)
    config['record_sample_limit'] = 0

    ## Recording - stop the program if the defined amount of seconds has passed (0 = unlimited)
    config['record_duration_limit'] = 60

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
    start_time = time.time()
    done = False
    pd_source = pd.DataFrame(columns=['uptime', 'topic', 'diff_01', 'diff_23'])

    logging.debug('Starting program')

    ####################################
    if interactive_mode:
        bokeh_figure = figure(title='ADS1115 Plot', sizing_mode='stretch_width', plot_height = 400, tools = 'pan, wheel_zoom, box_zoom, reset, crosshair, hover, save')
        bokeh_csource = {}        

        # Create DataSources and lines/circles for each topic
        for topic, color in zip(config['mqtt_topics'], palette):
            bokeh_csource[topic] = ColumnDataSource(data = dict(uptime = [], diff_01 = [], diff_23 = []))
            bokeh_figure.line(x = 'uptime', y = 'diff_01', source = bokeh_csource[topic], color = color)
            bokeh_figure.circle(x = 'uptime', y = 'diff_01', source = bokeh_csource[topic], legend = topic, color = color, fill_color = 'white')
            bokeh_figure.line(x = 'uptime', y = 'diff_23', source = bokeh_csource[topic], color = color)
            bokeh_figure.circle(x = 'uptime', y = 'diff_23', source = bokeh_csource[topic], legend = topic, color = color, fill_color = 'white')


        bokeh_figure.yaxis.axis_label = 'Currency (v)'
        bokeh_handle = show(bokeh_figure, notebook_handle = True)

    mqtt_client = mqtt.Client(client_id = config['mqtt_id'], userdata=config)
    mqtt_client.username_pw_set(config['mqtt_broker_user'], config['mqtt_broker_pass'])
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(config['mqtt_broker'], 1883, 60)
    mqtt_client.loop_start()

    logging.debug('Starting loop')

    while not done:
        try:
            # Receive message from queue
            msg = mqtt_queue.get(block = True, timeout = 0.1)

            # We expect JSON payload
            sensor_data = json.loads(msg.payload)

            # Check if we have all required fields
            if not 'uptime' in sensor_data or not 'diff_01' in sensor_data or not 'diff_23' in sensor_data:
                logging.error('missing key in sensor_data:')
                logging.error(sensor_data)
                continue
    
            # Build object for pandas dataframe and append
            pd_data = {}
            pd_data['uptime']   = sensor_data['uptime']
            pd_data['topic']    = msg.topic
            pd_data['diff_01']  = float(sensor_data['diff_01'])
            pd_data['diff_23']  = float(sensor_data['diff_23'])

            pd_source.loc[len(pd_source)] = pd_data
            sample_index = sample_index + 1
            
            

            #print(pd_data);

            if interactive_mode:
                bokeh_data = {}
                bokeh_data['uptime'] = [sensor_data['uptime']]
                bokeh_data['diff_01'] = [float(sensor_data['diff_01'])]
                bokeh_data['diff_23'] = [float(sensor_data['diff_23'])]

                #print(bokeh_data)
                bokeh_csource[msg.topic].stream(bokeh_data, config['bokeh_stream_rollover'])
                push_notebook(handle = bokeh_handle)
            else:
                print(msg.payload)

        except queue.Empty:
            #print('NOTE - timeout!')
            pass
        except json.JSONDecodeError:
            logging.error('JSONDecodeError')
            done = True
        except ValueError:
            logging.error('ValueError')
            done = True
        except:
            logging.error('Unexpected error!')
            done = True

        if config['record_sample_limit'] > 0 and sample_index > config['record_sample_limit']:
            logging.info('Reached sample limit, stopping')
            done = True

        if config['record_duration_limit'] > 0 and (time.time() - start_time) > config['record_duration_limit']:
            logging.info('Reached runtime limit, stopping')
            done = True


    # We are done, disconnect MQTT client
    mqtt_client.disconnect()

    # Export collected data
    if config['export_csv']:
        pd_source.to_csv(datetime.now().strftime('%Y%m%d%H%M%S') + '.csv')
        logging.info('Data written to ' + datetime.now().strftime('%Y%m%d%H%M%S') + '.csv')

    if config['export_json']:
        pd_source.to_json(datetime.now().strftime('%Y%m%d%H%M%S') + '.json')
        logging.info('Data written to ' + datetime.now().strftime('%Y%m%d%H%M%S') + '.json')

    if config['export_excel']:
        pd_source.to_excel(datetime.now().strftime('%Y%m%d%H%M%S') + '.xlsx')
        logging.info('Data written to ' + datetime.now().strftime('%Y%m%d%H%M%S') + '.xlsx')

    logging.info('Finished!')

# Run if there's no start button
if not ('button_run') in locals():
    record_and_display(config)