import 'dart:async';

  @override
  Widget build(BuildContext context) {

    return Scaffold(

      appBar: AppBar(
        title: const Text("Live sEMG Graph"),
      ),

      body: Padding(

        padding: const EdgeInsets.all(16),

        child: Card(

          elevation: 4,

          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(16),
          ),

          child: Padding(

            padding: const EdgeInsets.all(16),

            child: LineChart(

              LineChartData(

                gridData: const FlGridData(show: true),

                titlesData: const FlTitlesData(show: true),

                borderData: FlBorderData(show: true),

                minY: 0,
                maxY: 100,

                lineBarsData: [

                  LineChartBarData(

                    spots: spots,

                    isCurved: true,

                    barWidth: 4,

                    dotData: const FlDotData(show: false),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}