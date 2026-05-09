import 'package:flutter/material.dart';
import 'services/mqtt_service.dart';
import 'semg_graph_screen.dart';
import 'dart:async';
import 'dart:math';

void main() {
  runApp(const GripMateApp());
}

class GripMateApp extends StatelessWidget {
  const GripMateApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'GripMate',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: const HomeScreen(),
    );
  }
}

//////////////////////////////////////////////////////////////
// HOME SCREEN
//////////////////////////////////////////////////////////////

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  final mqttService = MqttService();

  bool mqttConnected = false;

  @override
  void initState() {
    super.initState();
    connectMQTT();
  }

  Future<void> connectMQTT() async {
    await mqttService.connect();

    setState(() {
      mqttConnected = true;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("GripMate Prosthetic Arm"),
        backgroundColor: Colors.blue.shade800,
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(
          children: [
            //////////////////////////////////////////////////////////
            // ARM IMAGE
            //////////////////////////////////////////////////////////

            Container(
              width: 220,
              height: 220,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                border: Border.all(
                  color: Colors.blue.shade400,
                  width: 4,
                ),
              ),
              child: ClipOval(
                child: Image.asset(
                  "assets/prosthetic_arm.jpg",
                  fit: BoxFit.cover,
                ),
              ),
            ),

            const SizedBox(height: 20),

            //////////////////////////////////////////////////////////
            // MQTT STATUS
            //////////////////////////////////////////////////////////

            Card(
              color: mqttConnected ? Colors.green.shade50 : Colors.red.shade50,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(16),
              ),
              child: ListTile(
                leading: Icon(
                  mqttConnected ? Icons.wifi : Icons.wifi_off,
                  color: mqttConnected ? Colors.green : Colors.red,
                ),
                title: Text(
                  mqttConnected ? "MQTT Connected" : "MQTT Disconnected",
                ),
              ),
            ),

            const SizedBox(height: 20),

            //////////////////////////////////////////////////////////
            // NAVIGATION BUTTONS
            //////////////////////////////////////////////////////////

            navButton(
              context,
              "Usage Analytics",
              Icons.analytics,
              const UsageAnalyticsScreen(),
            ),

            navButton(
              context,
              "Health Monitoring",
              Icons.monitor_heart,
              const HealthMonitoringScreen(),
            ),

            navButton(
              context,
              "Safety & Alerts",
              Icons.security,
              const SafetyAlertsScreen(),
            ),

            navButton(
              context,
              "Prosthetic Control",
              Icons.pan_tool,
              ProstheticControlScreen(
                mqtt: mqttService,
              ),
            ),

            navButton(
              context,
              "Live sEMG Graph",
              Icons.show_chart,
              const SemgGraphScreen(),
            ),
          ],
        ),
      ),
    );
  }

  Widget navButton(
    BuildContext context,
    String title,
    IconData icon,
    Widget page,
  ) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 10),
      child: ElevatedButton(
        style: ElevatedButton.styleFrom(
          backgroundColor: Colors.blue.shade600,
          minimumSize: const Size(double.infinity, 60),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(14),
          ),
        ),
        onPressed: () =>
            Navigator.push(context, MaterialPageRoute(builder: (_) => page)),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(icon, size: 26),
            const SizedBox(width: 12),
            Text(
              title,
              style: const TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.w600,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

//////////////////////////////////////////////////////////////
// USAGE ANALYTICS
//////////////////////////////////////////////////////////////

class UsageAnalyticsScreen extends StatefulWidget {
  const UsageAnalyticsScreen({super.key});

  @override
  State<UsageAnalyticsScreen> createState() => _UsageAnalyticsScreenState();
}

class _UsageAnalyticsScreenState extends State<UsageAnalyticsScreen> {
  double gripStrength = 0;
  double peakGrip = 0;

  late Timer timer;

  final Random random = Random();

  @override
  void initState() {
    super.initState();

    timer = Timer.periodic(
      const Duration(seconds: 1),
      (timer) {
        double simulatedGrip = random.nextDouble() * 100;

        setState(() {
          gripStrength = simulatedGrip;

          if (gripStrength > peakGrip) {
            peakGrip = gripStrength;
          }
        });
      },
    );
  }

  String gripLevel(double value) {
    if (value < 30) return "Soft Grip";
    if (value < 70) return "Medium Grip";
    return "Strong Grip";
  }

  @override
  void dispose() {
    timer.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Usage Analytics")),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            const InfoCard(
              "Daily Usage",
              "4.2 Hours",
              Icons.access_time,
            ),

            //////////////////////////////////////////////////////////
            // LIVE GRIP STRENGTH
            //////////////////////////////////////////////////////////

            Card(
              elevation: 4,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(16),
              ),
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  children: [
                    const Text(
                      "Live Grip Strength",
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 10),
                    LinearProgressIndicator(
                      value: gripStrength / 100,
                      minHeight: 10,
                    ),
                    const SizedBox(height: 10),
                    Text(
                      "${gripStrength.toStringAsFixed(1)}% - ${gripLevel(gripStrength)}",
                      style: const TextStyle(fontSize: 16),
                    ),
                  ],
                ),
              ),
            ),

            InfoCard(
              "Peak Grip Today",
              "${peakGrip.toStringAsFixed(1)}%",
              Icons.fitness_center,
            ),

            const InfoCard(
              "Battery Usage",
              "62%",
              Icons.battery_charging_full,
            ),

            const InfoCard(
              "WiFi Status",
              "Connected",
              Icons.wifi,
            ),
          ],
        ),
      ),
    );
  }
}

//////////////////////////////////////////////////////////////
// HEALTH MONITORING
//////////////////////////////////////////////////////////////

class HealthMonitoringScreen extends StatefulWidget {
  const HealthMonitoringScreen({super.key});

  @override
  State<HealthMonitoringScreen> createState() => _HealthMonitoringScreenState();
}

class _HealthMonitoringScreenState extends State<HealthMonitoringScreen> {
  double vibrationLevel = 0;

  late Timer timer;

  final Random random = Random();

  @override
  void initState() {
    super.initState();

    timer = Timer.periodic(
      const Duration(seconds: 1),
      (timer) {
        setState(() {
          vibrationLevel = random.nextDouble() * 100;
        });
      },
    );
  }

  String vibrationStatus(double value) {
    if (value < 10) return "Idle";
    if (value < 50) return "Normal Feedback";
    return "High Intensity Alert";
  }

  @override
  void dispose() {
    timer.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Health Monitoring")),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            const InfoCard(
              "Battery Health",
              "92%",
              Icons.battery_full,
            ),

            const InfoCard(
              "Motor Efficiency",
              "87%",
              Icons.settings,
            ),

            const InfoCard(
              "Signal Strength",
              "Strong",
              Icons.network_check,
            ),

            //////////////////////////////////////////////////////////
            // HAPTIC FEEDBACK
            //////////////////////////////////////////////////////////

            Card(
              elevation: 4,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(16),
              ),
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  children: [
                    const Text(
                      "Haptic Feedback Status",
                      style: TextStyle(
                        fontSize: 18,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                    const SizedBox(height: 10),
                    LinearProgressIndicator(
                      value: vibrationLevel / 100,
                      minHeight: 10,
                    ),
                    const SizedBox(height: 10),
                    Text(
                      "${vibrationLevel.toStringAsFixed(0)}% - ${vibrationStatus(vibrationLevel)}",
                      style: const TextStyle(fontSize: 16),
                    ),
                  ],
                ),
              ),
            ),

            const StatusCard("All systems operational"),
          ],
        ),
      ),
    );
  }
}

//////////////////////////////////////////////////////////////
// SAFETY & ALERTS
//////////////////////////////////////////////////////////////

class SafetyAlertsScreen extends StatelessWidget {
  const SafetyAlertsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Safety & Alerts")),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: const [
            AlertCard(
              "Battery Low",
              Icons.battery_alert,
              Colors.orange,
            ),
            AlertCard(
              "WiFi Connection Lost",
              Icons.wifi_off,
              Colors.red,
            ),
          ],
        ),
      ),
    );
  }
}

//////////////////////////////////////////////////////////////
// PROSTHETIC CONTROL
//////////////////////////////////////////////////////////////

class ProstheticControlScreen extends StatefulWidget {
  final MqttService mqtt;

  const ProstheticControlScreen({
    super.key,
    required this.mqtt,
  });

  @override
  State<ProstheticControlScreen> createState() =>
      _ProstheticControlScreenState();
}

class _ProstheticControlScreenState extends State<ProstheticControlScreen> {
  Widget controlBlock({
    required String title,
    required String command,
    required IconData icon,
    required Color color,
  }) {
    return GestureDetector(
      onTap: () {
        widget.mqtt.sendCommand(command);

        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('$title Command Sent')),
        );
      },
      child: Container(
        height: 160,
        margin: const EdgeInsets.symmetric(vertical: 16),
        decoration: BoxDecoration(
          gradient: LinearGradient(
            colors: [color.withOpacity(0.7), color],
          ),
          borderRadius: BorderRadius.circular(20),
          boxShadow: [
            BoxShadow(
              color: color.withOpacity(0.4),
              blurRadius: 10,
              offset: const Offset(0, 6),
            )
          ],
        ),
        child: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(icon, size: 60, color: Colors.white),
              const SizedBox(height: 12),
              Text(
                title,
                style: const TextStyle(
                  fontSize: 22,
                  fontWeight: FontWeight.bold,
                  color: Colors.white,
                ),
              ),
              const SizedBox(height: 6),
              Text(
                "Command: $command",
                style: const TextStyle(
                  color: Colors.white70,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Prosthetic Control'),
      ),
      body: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          children: [
            controlBlock(
              title: "Close Arm",
              command: "CLOSE",
              icon: Icons.front_hand,
              color: Colors.red,
            ),
            controlBlock(
              title: "Open Arm",
              command: "OPEN",
              icon: Icons.pan_tool_alt,
              color: Colors.green,
            ),
            controlBlock(
              title: "Point Gesture",
              command: "POINT",
              icon: Icons.ads_click,
              color: Colors.blue,
            ),
          ],
        ),
      ),
    );
  }
}

//////////////////////////////////////////////////////////////
// REUSABLE PROFESSIONAL WIDGETS
//////////////////////////////////////////////////////////////

class InfoCard extends StatelessWidget {
  final String title;
  final String value;
  final IconData icon;

  const InfoCard(
    this.title,
    this.value,
    this.icon, {
    super.key,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 4,
      margin: const EdgeInsets.symmetric(vertical: 10),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
      ),
      child: ListTile(
        leading: CircleAvatar(
          backgroundColor: Colors.blue.shade100,
          child: Icon(
            icon,
            color: Colors.blue.shade800,
          ),
        ),
        title: Text(title),
        trailing: Text(
          value,
          style: const TextStyle(
            fontWeight: FontWeight.bold,
            fontSize: 16,
          ),
        ),
      ),
    );
  }
}

class StatusCard extends StatelessWidget {
  final String text;

  const StatusCard(this.text, {super.key});

  @override
  Widget build(BuildContext context) {
    return Card(
      color: Colors.green.shade50,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
      ),
      child: ListTile(
        leading: const Icon(
          Icons.check_circle,
          color: Colors.green,
        ),
        title: Text(
          text,
          style: const TextStyle(
            fontWeight: FontWeight.bold,
          ),
        ),
      ),
    );
  }
}

class AlertCard extends StatelessWidget {
  final String title;
  final IconData icon;
  final Color color;

  const AlertCard(
    this.title,
    this.icon,
    this.color, {
    super.key,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      color: color.withOpacity(0.1),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(16),
      ),
      child: ListTile(
        leading: Icon(icon, color: color),
        title: Text(
          title,
          style: TextStyle(
            color: color,
            fontWeight: FontWeight.bold,
          ),
        ),
      ),
    );
  }
}
