import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import 'dart:async';
import 'dart:math';

class SemgGraphScreen extends StatefulWidget {
  const SemgGraphScreen({super.key});

  @override
  State<SemgGraphScreen> createState() => _SemgGraphScreenState();
}

class _SemgGraphScreenState extends State<SemgGraphScreen> {
  List<FlSpot> points = [];
  double time = 0;
  Timer? timer;

  @override
  void initState() {
    super.initState();
    startDemoSignal();
  }

  void startDemoSignal() {
    timer = Timer.periodic(const Duration(milliseconds: 100), (timer) {
      setState(() {
        double value;

        int phase = (time.toInt() ~/ 25) % 4;

        if (phase == 0) {
          value = 15 + Random().nextDouble() * 10; // Relax
        } else if (phase == 1) {
          value = 40 + Random().nextDouble() * 40; // Rising
        } else if (phase == 2) {
          value = 90 + Random().nextDouble() * 20; // Strong hold
        } else {
          value = 30 + Random().nextDouble() * 15; // Release
        }

        points.add(FlSpot(time, value));

        if (points.length > 60) {
          points.removeAt(0);
        }

        time += 1;
      });
    });
  }

  void simulateManualSpike() {
    setState(() {
      points.add(FlSpot(time, 120)); // Big spike
      time += 1;
    });
  }

  @override
  void dispose() {
    timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Live sEMG Signal")),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          children: [
            ElevatedButton(
              onPressed: simulateManualSpike,
              child: const Text("Simulate Contraction"),
            ),
            const SizedBox(height: 20),
            Expanded(
              child: LineChart(
                LineChartData(
                  minY: 0,
                  maxY: 120,
                  gridData: FlGridData(show: true),
                  titlesData: FlTitlesData(show: false),
                  borderData: FlBorderData(show: true),
                  lineBarsData: [
                    LineChartBarData(
                      spots: points,
                      isCurved: true,
                      color: Colors.blue,
                      dotData: FlDotData(show: false),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
