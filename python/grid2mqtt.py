#!/usr/bin/env python3
""" Publishes data from grid frequency SSE stream towards MQTT """

import argparse
import json

import paho.mqtt.client
import sseclient


def main():
    """ main entry point """
    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--stream", help="SSE stream URL",
                        default="http://gridfrequency.local/events")
    parser.add_argument("-b", "--broker", help="MQTT broker host name", default="mosquitto")
    parser.add_argument("-t", "--topic", help="MQTT topic", default="revspace/sensors/ac/frequency")
    args = parser.parse_args()

    print(f"Connecting to SSE '{args.stream}'")
    sse_client = sseclient.SSEClient(args.stream)

    print(f"Connecting to MQTT '{args.broker}'")
    mqtt_client = paho.mqtt.client.Client()
    mqtt_client.will_set(args.topic, payload=None, retain=True)
    mqtt_client.connect(args.broker)

    print("Processing SSE")
    for msg in sse_client:
        print(f"Got {msg}")
        data = json.loads(msg.data)
        frequency = data.get("frequency")
        if frequency:
            payload = f"{frequency:.3f} Hz"
            print(f"Publish '{payload}' to {args.topic}")
            mqtt_client.publish(args.topic, payload, retain=True)


if __name__ == "__main__":
    main()
