import 'package:mqtt_client/mqtt_client.dart';
    }

    //////////////////////////////////////////////////////////
    // SUBSCRIBE TOPICS
    //////////////////////////////////////////////////////////

    client.subscribe(
      'gripmate/emg',
      MqttQos.atLeastOnce,
    );

    client.subscribe(
      'gripmate/fsr',
      MqttQos.atLeastOnce,
    );

    client.subscribe(
      'gripmate/gesture',
      MqttQos.atLeastOnce,
    );

    //////////////////////////////////////////////////////////
    // LISTENER
    //////////////////////////////////////////////////////////

    client.updates!.listen((messages) {

      final recMess =
          messages[0].payload as MqttPublishMessage;

      final payload =
          MqttPublishPayload.bytesToStringAsString(
              recMess.payload.message);

      final topic = messages[0].topic;

      print('TOPIC: $topic');
      print('PAYLOAD: $payload');
    });
  }

  //////////////////////////////////////////////////////////
  // SEND COMMANDS
  //////////////////////////////////////////////////////////

  void sendCommand(String command) {

    final builder = MqttClientPayloadBuilder();

    builder.addString(command);

    client.publishMessage(
      'gripmate/command',
      MqttQos.atLeastOnce,
      builder.payload!,
    );

    print("Sent: $command");
  }
}