import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

class MqttService {
  final client = MqttServerClient('broker.hivemq.com', 'flutter_gripmate');

  Future<void> connect() async {
    client.port = 1883;
    client.keepAlivePeriod = 20;
    client.logging(on: false);

    await client.connect();
  }

  void sendCommand(int command) {
    final builder = MqttClientPayloadBuilder();
    builder.addString(command.toString());

    client.publishMessage(
      'gripmate/control',
      MqttQos.atMostOnce,
      builder.payload!,
    );
  }
}
