import 'package:flutter/material.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:syncfusion_flutter_charts/charts.dart';
import 'package:syncfusion_flutter_gauges/gauges.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'MQTT Flutter',
      theme: ThemeData(
        primarySwatch: Colors.blue,
      ),
      home: MyHomePage(),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({super.key});

  @override
  _MyHomePageState createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  MqttServerClient? client;
  double _temperature = 0.0;
  double _oximeter = 0.0;
  double _steps = 0.0;
  double _acceleration = 0.0; // Datos del acelerómetro
  double _humidity = 0.0;
  double _carbonMonoxide = 0.0; // Asume que esta es la lectura correcta
  final List<ECGData> _ecgData = [];
  final List<AccelerationData> _accelerationData = [];
  late ChartSeriesController _chartSeriesController;
  bool _isBuzzerOn = false;
  Color _currentColor = Colors.white;

  @override
  void initState() {
    super.initState();
    _connectToBroker();
  }

  Future<void> _connectToBroker() async {
    client = MqttServerClient('192.168.149.216', 'client_id_${DateTime.now().millisecondsSinceEpoch}');
    client!.port = 1883;
    client!.logging(on: true);

    client!.onConnected = () {
      print('Connected to MQTT broker');
      client!.subscribe('sensor/temper', MqttQos.atMostOnce);
      client!.subscribe('sensor/oxigeno', MqttQos.atMostOnce);
      client!.subscribe('sensor/giro', MqttQos.atMostOnce);
      client!.subscribe('sensor/pasos', MqttQos.atMostOnce);
      client!.subscribe('sensor/latidos', MqttQos.atMostOnce);
      client!.subscribe('sensor/rgb', MqttQos.atMostOnce);
      client!.subscribe('sensor/buzzer', MqttQos.atMostOnce);
      client!.subscribe('sensor/humedad', MqttQos.atMostOnce);
      client!.subscribe('sensor/car', MqttQos.atMostOnce);
    };

    client!.onDisconnected = () {
      print('Disconnected from MQTT broker');
      _reconnect();
    };

    try {
      await client!.connect();
      client!.updates?.listen((List<MqttReceivedMessage<MqttMessage?>>? c) {
        final MqttPublishMessage message = c![0].payload as MqttPublishMessage;
        final payload = MqttPublishPayload.bytesToStringAsString(message.payload.message);
        setState(() {
          if (c[0].topic == 'sensor/temper') {
            _temperature = double.tryParse(payload) ?? 0.0;
          } else if (c[0].topic == 'sensor/oxigeno') {
            _oximeter = double.tryParse(payload) ?? 0.0;
          } else if (c[0].topic == 'sensor/latidos') {
            final double ecgValue = double.tryParse(payload) ?? 0.0;
            _addEcgDataPoint(ecgValue);
          } else if (c[0].topic == 'sensor/giro') {
            _acceleration = double.tryParse(payload) ?? 0.0;
          } else if (c[0].topic == 'sensor/pasos') {
            _steps = double.tryParse(payload) ?? 0.0;
          } else if (c[0].topic == 'sensor/humedad') {
            _humidity = double.tryParse(payload) ?? 0.0;
          } else if (c[0].topic == 'sensor/car') {
            _carbonMonoxide = double.tryParse(payload) ?? 0.0;
          } else if (c[0].topic == 'sensor/buzzer') {
            _isBuzzerOn = payload == 'ON';
          }
        });
      });
    } catch (e) {
      print('Exception: $e');
      client!.disconnect();
    }
  }

  void _reconnect() async {
    await Future.delayed(const Duration(seconds: 5));
    await _connectToBroker();
  }

  void _addEcgDataPoint(double value) {
    if (_ecgData.length >= 100) {
      _ecgData.removeAt(0);
    }
    _ecgData.add(ECGData(_ecgData.length.toDouble(), value));
    _chartSeriesController.updateDataSource(
      addedDataIndexes: <int>[_ecgData.length - 1],
      removedDataIndexes: <int>[0],
    );
  }

void _toggleBuzzer(bool value) {
  if (client!.connectionStatus!.state == MqttConnectionState.connected) {
    if (_isBuzzerOn != value) {
      final payload = value ? 'ON' : 'OFF';
      final builder = MqttClientPayloadBuilder();
      builder.addString(payload);
      client!.publishMessage('sensor/buzzer', MqttQos.atLeastOnce, builder.payload!);
      setState(() {
        _isBuzzerOn = value;
      });
    } else {
      print('El estado del buzzer ya está ${value ? 'encendido' : 'apagado'}. No se envía un nuevo mensaje.');
    }
  } else {
    print('No conectado al broker MQTT. No se puede enviar mensaje.');
  }
}


  void _changeColor(Color color) {
    final String colorStr = '${color.red},${color.green},${color.blue}';
    final builder = MqttClientPayloadBuilder();
    builder.addString(colorStr);
    client!.publishMessage('sensor/rgb', MqttQos.atLeastOnce, builder.payload!);
    setState(() {
      _currentColor = color;
    });
  }

  @override
  void dispose() {
    client?.disconnect();
    super.dispose();
  }


  @override
  Widget build(BuildContext context) {
    final double screenHeight = MediaQuery.of(context).size.height;
    final double screenWidth = MediaQuery.of(context).size.width;

    return Scaffold(
      appBar: AppBar(
        title: const Text('MQTT Flutter Dashboard'),
      ),
      body: SingleChildScrollView(
        child: Wrap(
          alignment: WrapAlignment.center,
          spacing: screenWidth * 0.05, // Espacio entre los gráficos
          runSpacing: screenHeight * 0.05, // Espacio entre filas de gráficos
          children: <Widget>[
            _buildTemperatureGauge(screenHeight, screenWidth),
            _buildOximeterGauge(screenHeight, screenWidth),
            _buildHumidityGauge(screenHeight, screenWidth),
            _buildCarbonMonoxideGauge(screenHeight, screenWidth),
            _buildECGChart(screenHeight, screenWidth),
            _buildStepsLabel(screenHeight, screenWidth),
            _buildBuzzerSwitch()
            
          ],
        ),
      ),
    );
  }

  Widget _buildTemperatureGauge(double screenHeight, double screenWidth) {
    return SizedBox(
      height: screenHeight * 0.4,
      width: screenWidth * 0.4,
      child: Column(
        children: [
          Text(
            'Temperatura',
            style: TextStyle(
              fontSize: screenWidth * 0.04,
              fontWeight: FontWeight.bold,
            ),
          ),
          Expanded(
            child: SfRadialGauge(
              axes: <RadialAxis>[
                RadialAxis(
                  minimum: -10,
                  maximum: 50,
                  ranges: <GaugeRange>[
                    GaugeRange(startValue: -50, endValue: _temperature, color: Colors.blue),
                    GaugeRange(startValue: _temperature, endValue: 150, color: Colors.grey.withOpacity(0.3)),
                  ],
                  pointers: <GaugePointer>[
                    NeedlePointer(value: _temperature),
                  ],
                  annotations: <GaugeAnnotation>[
                    GaugeAnnotation(
                      widget: Text(
                        '$_temperature°C',
                        style: TextStyle(fontSize: screenWidth * 0.03, fontWeight: FontWeight.bold),
                      ),
                      angle: 90,
                      positionFactor: 0.5,
                    ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildOximeterGauge(double screenHeight, double screenWidth) {
    return SizedBox(
      height: screenHeight * 0.4,
      width: screenWidth * 0.4,
      child: Column(
        children: [
          Text(
            'Oximetro',
            style: TextStyle(
              fontSize: screenWidth * 0.04,
              fontWeight: FontWeight.bold,
            ),
          ),
          Expanded(
            child: SfRadialGauge(
              axes: <RadialAxis>[
                RadialAxis(
                  minimum: 0,
                  maximum: 100,
                  ranges: <GaugeRange>[
                    GaugeRange(startValue: 0, endValue: _oximeter, color: Colors.green),
                    GaugeRange(startValue: _oximeter, endValue: 100, color: Colors.grey.withOpacity(0.3)),
                  ],
                  pointers: <GaugePointer>[
                    NeedlePointer(value: _oximeter),
                  ],
                  annotations: <GaugeAnnotation>[
                    GaugeAnnotation(
                      widget: Text(
                        '$_oximeter%',
                        style: TextStyle(fontSize: screenWidth * 0.03, fontWeight: FontWeight.bold),
                      ),
                      angle: 90,
                      positionFactor: 0.5,
                    ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }


Widget _buildHumidityGauge(double screenHeight, double screenWidth) {
  return SizedBox(
    height: screenHeight * 0.4,
    width: screenWidth * 0.4,
    child: Column(
      children: [
        Text(
          'Humedad',
          style: TextStyle(
            fontSize: screenWidth * 0.04,
            fontWeight: FontWeight.bold,
          ),
        ),
        Expanded(
          child: SfRadialGauge(
            axes: <RadialAxis>[
              RadialAxis(
                minimum: 0,
                maximum: 100,
                ranges: <GaugeRange>[
                  GaugeRange(startValue: 0, endValue: _humidity, color: Colors.blueAccent),
                  GaugeRange(startValue: _humidity, endValue: 100, color: Colors.grey.withOpacity(0.3)),
                ],
                pointers: <GaugePointer>[
                  NeedlePointer(value: _humidity),
                ],
                annotations: <GaugeAnnotation>[
                  GaugeAnnotation(
                    widget: Text(
                      '$_humidity%',
                      style: TextStyle(fontSize: screenWidth * 0.03, fontWeight: FontWeight.bold),
                    ),
                    angle: 90,
                    positionFactor: 0.5,
                  ),
                ],
              ),
            ],
          ),
        ),
      ],
    ),
  );
}

  Widget _buildCarbonMonoxideGauge(double screenHeight, double screenWidth) {
    return SizedBox(
      height: screenHeight * 0.4,
      width: screenWidth * 0.4,
      child: Column(
        children: [
          Text(
            'Giroscopio',
            style: TextStyle(
              fontSize: screenWidth * 0.04,
              fontWeight: FontWeight.bold,
            ),
          ),
          Expanded(
            child: SfRadialGauge(
              axes: <RadialAxis>[
                RadialAxis(
                  minimum: -2,
                  maximum: 8000,
                  ranges: <GaugeRange>[
                    GaugeRange(startValue: 0, endValue: _acceleration, color: Colors.redAccent),
                    GaugeRange(startValue: _acceleration, endValue: 1000, color: Colors.grey.withOpacity(0.3)),
                  ],
                  pointers: <GaugePointer>[
                    NeedlePointer(value: _acceleration),
                  ],
                  annotations: <GaugeAnnotation>[
                    GaugeAnnotation(
                      widget: Text(
                        '$_acceleration gx',
                        style: TextStyle(fontSize: screenWidth * 0.03, fontWeight: FontWeight.bold),
                      ),
                      angle: 90,
                      positionFactor: 0.5,
                    ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

Widget _buildECGChart(double screenHeight, double screenWidth) {
  return SizedBox(
    height: screenHeight * 0.4,
    width: screenWidth * 0.9, // Ajustar el ancho según sea necesario
    child: Column(
      children: [
        Text(
          'Pulso',
          style: TextStyle(
            fontSize: screenWidth * 0.04,
            fontWeight: FontWeight.bold,
          ),
        ),
        Expanded(
          child: SfCartesianChart(
            primaryXAxis: NumericAxis(),
            primaryYAxis: NumericAxis(
              minimum: 0, // Valor mínimo del eje Y
              maximum: 100, // Valor máximo del eje Y
              title: AxisTitle(text: 'Valor'), // Título del eje Y
              interval: 20, // Intervalo de las marcas en el eje Y
              labelFormat: '{value}', // Formato de las etiquetas del eje Y
            ),
            series: <LineSeries<ECGData, double>>[
              LineSeries<ECGData, double>(
                onRendererCreated: (ChartSeriesController controller) {
                  _chartSeriesController = controller;
                },
                dataSource: _ecgData,
                xValueMapper: (ECGData data, _) => data.time,
                yValueMapper: (ECGData data, _) => data.value,
              ),
            ],
          ),
        ),
      ],
    ),
  );
}

// Añade esto en el método build, dentro del Wrap o donde quieras colocar la etiqueta de pasos
Widget _buildStepsLabel(double screenHeight, double screenWidth) {
  return SizedBox(
    height: screenHeight * 0.2, // Ajusta la altura según sea necesario
    width: screenWidth * 0.4, // Ajusta el ancho según sea necesario
    child: Column(
      children: [
        Text(
          'Pasos',
          style: TextStyle(
            fontSize: screenWidth * 0.04,
            fontWeight: FontWeight.bold,
          ),
        ),
        const SizedBox(height: 10), // Espacio entre el título y el valor
        Text(
          '$_steps pasos',
          style: TextStyle(
            fontSize: screenWidth * 0.03,
            fontWeight: FontWeight.bold,
            color: Colors.black,
          ),
        ),
      ],
    ),
  );
}


  Widget _buildBuzzerSwitch() {
    return SwitchListTile(
      title: const Text('Buzzer'),
      value: _isBuzzerOn,
      onChanged: (bool value) {
        _toggleBuzzer(value);
      },
    );
  }
}

class ECGData {
  ECGData(this.time, this.value);
  final double time;
  final double value;
}

class AccelerationData {
  AccelerationData(this.time, this.value);
  final double time;
  final double value;
}
