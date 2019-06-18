import paho.mqtt.client as mqtt
import json
import time
import math
import pandas as pd
import queue
import sys

from datetime import datetime

################################# Settings BEGIN

## MQTT client id
config_mqtt_id = "example-reader-poll-plot-01"
## MQTT broker ip / hostname
config_mqtt_broker = "10.42.10.86"
## MQTT broker target topic
config_mqtt_topic = "sensor/60:01:94:4A:A7:54"
## Sample limit (0 = unlimited)
config_record_sample_limit = 100
## Sample record duration limit (seconds, 0 = unlimited)
config_record_duration_limit = 0

################################# Settings END

mqtt_queue = queue.Queue()
sample_index = 0
start_time = time.time()
done = False
pd_source = pd.DataFrame(columns=['time', 'temp0', 'temp1'])

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("STATUS - MQTT connected with result code " + str(rc) + " - subscribing " + config_mqtt_topic)
    # Subscribing in on_connect() means that if we lose the connection and reconnect then subscriptions will be renewed.
    client.subscribe(config_mqtt_topic)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    # Put message in queue
    mqtt_queue.put(msg)
    
print("STATUS - Initializing")


####################################

mqtt_client = mqtt.Client(client_id=config_mqtt_id)
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(config_mqtt_broker, 1883, 60)
mqtt_client.loop_start()

print("STATUS - Starting loop")
while not done:
    try:
        msg = mqtt_queue.get(block=True, timeout=0.1)

        # We expect JSON payload
        sensor_data = json.loads(msg.payload)

        # Check if we have all required fields
        if not 'uptime' in sensor_data or not 'temp0' in sensor_data or not 'temp1' in sensor_data:
            print("ERROR - missing key in sensor_data")
        
        new_data = {}
        new_data['time']  = [float(sensor_data['uptime'] / 1000)]
        new_data['temp0'] = [float(sensor_data['temp0'])]
        new_data['temp1'] = [float(sensor_data['temp1'])]

        pd_source.loc[len(pd_source)] = [new_data['time'][0], new_data['temp0'][0], new_data['temp1'][0]]
        sample_index = sample_index + 1

        print(new_data)
        
    except queue.Empty:
        #print("NOTE - timeout!")
        pass
    except json.JSONDecodeError:
        print("ERROR - json.loads(msg.payload) failed!")
        done = True
    except ValueError:
        print("ERROR - bokeh_source.stream(new_data, 100) failed!")
        done = True
    except KeyboardInterrupt:
        print("STATUS - Script stopped, finishing program")
        done = True
    except:
        print("ERROR - unexpected error!")
        done = True
        #sys.exit()
        
    if config_record_sample_limit > 0 and sample_index > config_record_sample_limit:
        done = True
        print("STATUS - Reached sample limit, stopping")
    if config_record_duration_limit > 0 and (time.time() - start_time) > config_record_duration_limit:
        done = True
        print("STATUS - Reached runtime limit, stopping")

# We are done, disconnect MQTT client
mqtt_client.disconnect()

# Export Data
#pd_source.to_csv(datetime.now().strftime("%Y%m%d%H%M%S") + ".csv")
pd_source.to_html(datetime.now().strftime("%Y%m%d%H%M%S") + ".html")
#pd_source.to_json(datetime.now().strftime("%Y%m%d%H%M%S") + ".json")
pd_source.to_excel(datetime.now().strftime("%Y%m%d%H%M%S") + ".xlsx")
print("STATUS - Finished!")