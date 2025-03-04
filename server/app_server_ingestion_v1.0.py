#!/usr/bin/python3

"""
Application Server DB Ingestion

This script is an MQTT and InfluxDB client.

1. Connects to the MQTT broker using web socket and subscribes to all topics
2. Process/decodes Rx Publish messages
3. Connects to InfluxDB and ingest measurements

!!! This is application specific !!!

For different application:
- Change the MQTT topics (global variables)
- Change the "process_publish" function

"""
import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient
import datetime
import json
import logging
import time
from enum import Enum

# Debugging
DEBUG_PRINT_RX = True
DEBUG_PRINT_RX_DECODED = False

# Host IP
HOST = "135.181.146.21"

# RabbitMQ
MQTT_HOST = HOST
MQTT_PORT = 15675
MQTT_CLIENT_ID = "backend_influx"
MQTT_VHOST = "test-v"
MQTT_USER = "demouser1"
MQTT_PASS = "demopass1"
MQTT_QOS = 1
MQTT_CONNACK_TIMEOUT_SEC = 5

# MQTT Topics
MQTT_TOPIC_GW = "u"
MQTT_TOPIC_SOIL = "s"
MQTT_TOPIC_ENV = "c"
MQTT_TOPIC_CONTROLLER_HP = "d"
MQTT_TOPIC_CONTROLLER_LP = "h"
MQTT_TOPIC_HYDRO = "w"

# InfluxDB
INFLUXDB_HOST = HOST
INFLUXDB_PORT = 8086
INFLUXDB_DB_NAME = 'test_db'

# Status
STATUS_DB = 1


class MqttConnack(Enum):
    """
    MQTT Connack error codes enumerator
    """
    RC_UNDEFINED = -1
    RC_CONNECTION_ACCEPTED = 0
    RC_UNACCEPTABLE_PROTOCOL_VERSION = 1
    RC_IDENTIFIER_REJECTED = 2
    RC_SERVE_UNAVAILABLE = 3
    RC_BAD_USER_PASS = 4
    RC_NOT_AUTHORISED = 5


class InfulxDB:
    """
    Influx DB class
    """
    def __init__(self):
        """
        Initializations
        """
        self.influx_client = InfluxDBClient(host=INFLUXDB_HOST, port=INFLUXDB_PORT, database=INFLUXDB_DB_NAME)

    def write(self, data_json):
        """
        Write josn to Influx DB
        :param data_json: The json data to be written
        :return:
        """
        self.influx_client.write_points(data_json)


class ApplicationSerrver:
    """
    Application Server class
    """
    def __init__(self):
        """
        Initializations
        """

        # Initialize databases
        self.influx = InfulxDB()
        # self.mysql = MySQL()

        # Initialize MQTT client
        self.client = mqtt.Client(client_id=MQTT_CLIENT_ID, transport='websockets')
        self.client.ws_set_options(path='/ws')
        self.mqtt_broker = MqttConnack.RC_UNDEFINED

        # Callbacks
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_publish = self.on_publish
        self.client.on_log = self.on_log

        # Set username and password
        self.client.username_pw_set(MQTT_VHOST + ":" + MQTT_USER, MQTT_PASS)

    def connect(self):
        """
        Connect to MQTT broker
        """
        # Connect
        self.client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)

        # Starts a new thread to process MQTT network traffic.
        # NOTE: Without starting the thread, the CONNACK cannot be received!
        self.client.loop_start()

    def on_connect(self, client, userdata, flags, rc):
        """
        Callback after connecting to broker

        :param client:
        :param userdata:
        :param flags:
        :param rc: Return code
        """
        if rc == 0:
            self.mqtt_broker = MqttConnack.RC_CONNECTION_ACCEPTED
        else:
            self.mqtt_broker = rc

    def subscribe(self):
        """
        Subscribe to all topics
        """
        self.client.subscribe(topic="#", qos=MQTT_QOS)

    def on_message(self, client, userdata, msg):
        """
        Callback for Rx PUBLISH messages

        :param client:
        :param userdata:
        :param msg: Payload
        """
        self.process_publish(msg)

    def loop_forever(self):
        """
        Blocking MQTT network loop function - NOT USED
        """
        self.client.loop_forever(timeout=1.0, max_packets=1, retry_first_connection=True)

    def disconnect(self):
        """
        Disconnect from MQTT broker
        """
        self.client.disconnect()

    def on_publish(self, client, userdata, mid):
        """
        NOT USED
        """
        pass

    def on_log(self, client, userdata, level, buf):
        """
        NOT USED
        """
        pass

    def process_publish(self, msg):
        """
        Process Rx Publish messages based on Senseed application

        !!! Application specific function !!!

        :param msg: The message to be decoded
        """
        try:
            # Get current time
            current_time_influx = datetime.datetime.now().strftime('%Y-%m-%dT%H:%M:%SZ')

            # Decode json
            json_data = json.loads(msg.payload.decode("ascii"))

            # Decode measurement and topic tag
            measurement_name = msg.topic.split('/')[0]
            topic = msg.topic.split('/')[1]

            # Decode ID, ST and measurement timestamp
            id = json_data['ID']
            st = json_data['ST']
            timestamp = json_data['TM']

            # Ignore messages based on status, i.e. "ST"
            # NOTE: "ST" is the same for both gateways and things
            if st != STATUS_DB:
                return

            if DEBUG_PRINT_RX:
                logging.info("RX: {} {} {} {} {}".format(timestamp, measurement_name, topic, id, st))

            # ************************ Gateway ************************
            if topic == MQTT_TOPIC_GW:

                # Decode Logs
                logs = json_data['B']
                log_timestamps = logs['A']
                log_numbers = logs['B']

                if log_timestamps and log_numbers:
                    for log in range(0, len(log_timestamps)):
                        # Create json for influxdb
                        json_body = [
                            {
                                "measurement": measurement_name,
                                "tags": {
                                    "topic": topic,
                                    "id": id,
                                    "timestamp": log_timestamps[log]
                                },
                                "time": current_time_influx,
                                "fields": {
                                    "log": log_numbers[log]
                                }
                            }
                        ]
                        # Write data to influxdb
                        self.influx.write(json_body)

                        if DEBUG_PRINT_RX_DECODED:
                            logging.info("RX: {} {} {} {} {} {}".format(timestamp, measurement_name, topic, id,
                                                                        logs['A'],
                                                                        logs['B']))

            # ************************ Soil sensor ************************
            elif topic == MQTT_TOPIC_SOIL:

                # Decode soil measurements
                soil_1 = json_data['C']
                soil_2 = json_data['D']
                soil_3 = json_data['E']
                soil_4 = json_data['F']

                # Create json for influxdb
                json_body = [
                    {
                        "measurement": measurement_name,
                        "tags": {
                            "topic": topic,
                            "id": id,
                            "timestamp": timestamp
                        },
                        "time": current_time_influx,
                        "fields": {
                            "soil_1": soil_1,
                            "soil_2": soil_2,
                            "soil_3": soil_3,
                            "soil_4": soil_4
                        }
                    }
                ]

                # Write status to InfluxDB
                self.influx.write(json_body)

                if DEBUG_PRINT_RX_DECODED:
                    logging.info("RX: {} {} {} {} {} {} {} {}".format(timestamp, measurement_name, topic, id,
                                                                      soil_1,
                                                                      soil_2,
                                                                      soil_3,
                                                                      soil_4))

            # ************************ Environment Sensor ************************
            elif topic == MQTT_TOPIC_ENV:

                # Decode temperature, humidity and CO2
                temp = json_data['C']
                hum = json_data['D']
                co2 = json_data['E']

                # Create json for influxdb
                json_body = [
                    {
                        "measurement": measurement_name,
                        "tags": {
                            "topic": topic,
                            "id": id,
                            "timestamp": timestamp
                        },
                        "time": current_time_influx,
                        "fields": {
                            "temp": temp,
                            "hum": hum,
                            "co2": co2
                        }
                    }
                ]

                # Write status to InfluxDB
                self.influx.write(json_body)

                if DEBUG_PRINT_RX_DECODED:
                    logging.info("RX: {} {} {} {} {} {} {}".format(timestamp, measurement_name, topic, id,
                                                                   temp,
                                                                   hum,
                                                                   co2))

            # ************************ Controller HP ************************
            elif topic == MQTT_TOPIC_CONTROLLER_HP:

                # Decode 'now' and consumption for both sub-devices
                now_1 = json_data['E']
                consumption_1 = json_data['F']
                now_2 = json_data['K']
                consumption_2 = json_data['L']

                # Create json for influxdb
                json_body = [
                    {
                        "measurement": measurement_name,
                        "tags": {
                            "topic": topic,
                            "id": id,
                            "timestamp": timestamp
                        },
                        "time": current_time_influx,
                        "fields": {
                            "now_1": now_1,
                            "consumption_1": consumption_1,
                            "now_2": now_2,
                            "consumption_2": consumption_2
                        }
                    }
                ]

                # Write status to InfluxDB
                self.influx.write(json_body)

                if DEBUG_PRINT_RX_DECODED:
                    logging.info("RX: {} {} {} {} {} {} {} {}".format(timestamp, measurement_name, topic, id,
                                                                      now_1,
                                                                      consumption_1,
                                                                      now_2,
                                                                      consumption_2))

            # ************************ Controller LP ************************
            elif topic == MQTT_TOPIC_CONTROLLER_LP:

                # Decode 'now' for both sub-devices
                now_1 = json_data['E']
                now_2 = json_data['K']

                # Create json for influxdb
                json_body = [
                    {
                        "measurement": measurement_name,
                        "tags": {
                            "topic": topic,
                            "id": id,
                            "timestamp": timestamp
                        },
                        "time": current_time_influx,
                        "fields": {
                            "now_1": now_1,
                            "now_2": now_2,
                        }
                    }
                ]

                # Write status to InfluxDB
                self.influx.write(json_body)

                if DEBUG_PRINT_RX_DECODED:
                    logging.info("RX: {} {} {} {} {} {}".format(timestamp, measurement_name, topic, id,
                                                                now_1,
                                                                now_2))

            # ************************ Hydro ************************
            elif topic == MQTT_TOPIC_HYDRO:

                # Create json for influxdb
                json_body = [
                    {
                        "measurement": measurement_name,
                        "tags": {
                            "topic": topic,
                            "id": id,
                            "timestamp": timestamp
                        },
                        "time": current_time_influx,
                        "fields": {
                            "a": 0
                        }
                    }
                ]

                # Write status to InfluxDB
                self.influx.write(json_body)

                if DEBUG_PRINT_RX_DECODED:
                    logging.info("RX: {} {} {} {}".format(timestamp, measurement_name, topic, id))

            else:
                logging.warning(f"MQTT Rx Publish unknown topic: {topic}")

        except Exception as err:
            logging.error(f"MQTT Rx Publish wrong format: {err}")


if __name__ == "__main__":

    while True:
        try:
            # Initialize logging
            logging_format = "%(asctime)s: %(message)s"
            logging.basicConfig(format=logging_format, level=logging.INFO, datefmt="%d-%m-%Y %H:%M:%S")
            logging.info("Server Initialisations...")

            # Initialize
            server = ApplicationSerrver()
            # Connect to MQTT broker
            server.connect()

            # Wait for connack
            t0 = time.time()
            while server.mqtt_broker == MqttConnack.RC_UNDEFINED:
                time.sleep(0.000001)
                if time.time() - t0 >= MQTT_CONNACK_TIMEOUT_SEC:
                    break

            # Check that connection is established
            if server.mqtt_broker == MqttConnack.RC_CONNECTION_ACCEPTED:
                logging.info("MQTT Broker Connack success")
            else:
                raise Exception(f"MQTT Broker Connack error: {server.mqtt_broker}")

            # Subscribe to topics
            logging.info("MQTT Broker subscription")
            server.subscribe()

            # Yield - Forever loop
            logging.info("MQTT Broker loop")
            while server.mqtt_broker == MqttConnack.RC_CONNECTION_ACCEPTED:
                time.sleep(0.000001)

            # Close connections
            logging.warning("MQTT Broker disconnected")
            server.disconnect()

        except Exception as error:
            logging.error(error)
