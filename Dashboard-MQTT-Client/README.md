### Paso 1: Crear un nuevo proyecto Flutter

1. Abre tu terminal o línea de comandos.
2. Navega a la carpeta donde deseas crear tu proyecto.
3. Ejecuta el siguiente comando para crear un nuevo proyecto Flutter:
   ```bash
   flutter create mqtt_line_chart
   ```
4. Navega a la carpeta del proyecto:
   ```bash
   cd mqtt_line_chart
   ```

### Paso 2: Agregar dependencias

Edita el archivo `pubspec.yaml` para incluir las dependencias necesarias:

```yaml
dependencies:
  flutter:
    sdk: flutter
  mqtt_client: ^9.6.1
  syncfusion_flutter_charts: ^20.2.46 # Añade esta línea para usar los gráficos de Syncfusion
```

Luego, ejecuta el comando para instalar las dependencias:

```bash
flutter pub get
```

### Paso 3: Crear el archivo `mqtt_service.dart`

Crea un archivo llamado `mqtt_service.dart` en la carpeta `lib` y añade el siguiente código:

```dart
import 'package:mqtt_client/mqtt_client.dart'; // Importa la biblioteca MQTT Client
import 'package:mqtt_client/mqtt_server_client.dart'; // Importa la biblioteca MQTT Server Client

class MqttService {
  final MqttServerClient client; // Declaración del cliente MQTT

  // Constructor de MqttService que inicializa el cliente MQTT
  MqttService(String server, String clientId)
      : client = MqttServerClient(server, '') {
    // Asegúrate de que el clientId sea válido
    const sanitizedClientId = '';

    client.logging(on: true); // Habilita el logging para el cliente MQTT
    client.setProtocolV311(); // Configura el protocolo MQTT 3.1.1
    client.keepAlivePeriod = 20; // Configura el periodo de keep alive en 20 segundos

    // Configuración del mensaje de conexión
    final connMessage = MqttConnectMessage()
        .withClientIdentifier(sanitizedClientId) // Identificador del cliente
        .startClean() // Indica que el cliente debe comenzar con una sesión limpia
        .withWillQos(MqttQos.atLeastOnce); // Configura el QoS para el mensaje de "última voluntad"

    client.connectionMessage = connMessage; // Asigna el mensaje de conexión al cliente
  }

  // Método que retorna un stream de datos de temperatura
  Stream<double> getTemperatureStream() async* {
    try {
      // Intenta conectar al servidor MQTT
      await client.connect();
    } catch (e) {
      // Si la conexión falla, desconecta el cliente y retorna
      client.disconnect();
      return;
    }

    // Verifica si la conexión fue exitosa
    if (client.connectionStatus?.state == MqttConnectionState.connected) {
      // Se suscribe al tópico de temperatura con QoS 1
      client.subscribe("temperature/topic", MqttQos.atLeastOnce);

      // Escucha los mensajes entrantes y emite los valores de temperatura
      await for (final c in client.updates!) {
        final MqttPublishMessage recMess = c[0].payload as MqttPublishMessage; // Obtiene el mensaje publicado
        final String pt = MqttPublishPayload.bytesToStringAsString(recMess.payload.message); // Convierte el payload a String
        yield double.tryParse(pt) ?? 0.0; // Convierte el payload a double y lo emite en el stream
      }
    } else {
      // Si la conexión no fue exitosa, desconecta el cliente
      client.disconnect();
    }
  }
}
```

### Paso 4: Crear el archivo `main.dart`

Edita el archivo `main.dart` en la carpeta `lib` con el siguiente código:

```dart
import 'package:flutter/material.dart';
import 'package:syncfusion_flutter_charts/charts.dart'; // Importa la biblioteca para gráficos
import 'mqtt_service.dart'; // Importa el servicio MQTT

void main() {
  runApp(const MyApp()); // Llama a runApp para iniciar la aplicación
}

// MyApp es un widget Stateless que define la estructura general de la aplicación
class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'MQTT Line Chart App', // Título de la aplicación
      theme: ThemeData(
        primarySwatch: Colors.blue, // Tema principal de la aplicación
      ),
      home: const LineChartScreen(), // Define LineChartScreen como la pantalla principal
    );
  }
}

// LineChartScreen es un StatefulWidget que mostrará el gráfico de líneas de temperatura
class LineChartScreen extends StatefulWidget {
  const LineChartScreen({super.key});

  @override
  _LineChartScreenState createState() => _LineChartScreenState(); // Crea el estado asociado a este widget
}

// _LineChartScreenState contiene el estado del widget LineChartScreen
class _LineChartScreenState extends State<LineChartScreen> {
  late MqttService _mqttService; // Declaración del servicio MQTT
  List<TemperatureData> _data = []; // Lista para almacenar los datos de temperatura
  late ChartSeriesController _chartSeriesController;

  @override
  void initState() {
    super.initState();
    // Inicializa el servicio MQTT con el broker y el clientId
    _mqttService = MqttService('broker.emqx.io', '');
    // Escucha el stream de temperatura y actualiza el estado cuando llegue un nuevo valor
    _mqttService.getTemperatureStream().listen((temperature) {
      setState(() {
        _data.add(TemperatureData(DateTime.now(), temperature)); // Añade los nuevos datos a la lista
        // Limita el número de puntos en el gráfico a 20
        if (_data.length > 20) {
          _data.removeAt(0);
        }
        _chartSeriesController.updateDataSource(addedDataIndex: _data.length - 1);
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Temperature Line Chart'), // Título de la barra de la aplicación
      ),
      body: Center(
        // Contenedor principal de la pantalla
        child: SfCartesianChart(
          // Widget para mostrar el gráfico de líneas
          primaryXAxis: DateTimeAxis(), // Configura el eje X como eje de tiempo
          series: <LineSeries<TemperatureData, DateTime>>[
            LineSeries<TemperatureData, DateTime>(
              onRendererCreated: (ChartSeriesController controller) {
                _chartSeriesController = controller;
              },
              dataSource: _data, // Define la fuente de datos para el gráfico
              xValueMapper: (TemperatureData data, _) => data.time, // Mapea los valores de X
              yValueMapper: (TemperatureData data, _) => data.temperature, // Mapea los valores de Y
            )
          ],
        ),
      ),
    );
  }
}

// Clase para representar los datos de temperatura
class TemperatureData {
  TemperatureData(this.time, this.temperature);
  final DateTime time; // Tiempo en el que se recibió la temperatura
  final double temperature; // Valor de la temperatura
}
```

### Explicación Detallada del Código con Comentarios

#### mqtt_service.dart

- **Importaciones**:
  - `mqtt_client.dart`: Biblioteca principal para manejar las conexiones MQTT.
  - `mqtt_server_client.dart`: Biblioteca para configurar el cliente MQTT que se conecta a un servidor.

- **Clase `MqttService`**:
  - `MqttServerClient client`: Variable que representa el cliente MQTT.

- **Constructor `MqttService`**:
  - `MqttService(String server, String clientId)`: Inicializa el cliente MQTT con el servidor especificado y un `clientId`.
  - `client = MqttServerClient(server, '')`: Crea una instancia del cliente MQTT.
  - `client.logging(on: true)`: Habilita el logging para el cliente MQTT.
  - `client.setProtocolV311()`: Configura el cliente para usar el protocolo MQTT 3.1.1.
  - `client.keepAlivePeriod = 20`: Configura el periodo de keep alive en 20 segundos.
  - `MqttConnectMessage()`: Configura el mensaje de conexión con el `clientId`, indicando que debe empezar con una sesión limpia y configurando el QoS del mensaje de "última voluntad".
  - `client.connectionMessage = connMessage`: Asigna el mensaje de conexión al cliente.

- **Método `getTemperatureStream`**:
  - `Stream<double> getTemperatureStream() async*`: Método asincrónico que retorna un stream de datos de temperatura.
  - `try { await client.connect(); } catch (e) { ... }`: Intenta conectar al servidor MQTT. Si la conexión falla, desconecta el cliente y retorna.
  - `if (client.connectionStatus?.state == MqttConnectionState.connected) { ... }`: Verifica si la conexión fue exitosa.
  - `client.subscribe("temperature/topic", MqttQos.atLeastOnce)`: Se suscribe al tópico de temperatura con QoS 1.
  - `await for (final c in client.updates!) { ... }`: Escucha los mensajes entrantes y emite los valores de temperatura.
  - `final MqttPublishMessage recMess = c[0

].payload as MqttPublishMessage`: Obtiene el mensaje publicado.
  - `final String pt = MqttPublishPayload.bytesToStringAsString(recMess.payload.message)`: Convierte el payload del mensaje a una cadena de texto.
  - `yield double.tryParse(pt) ?? 0.0`: Convierte la cadena de texto a un número de punto flotante (double) y lo emite en el stream.

#### main.dart

- **MyApp**:
  - `runApp(const MyApp())`: Llama a `runApp` para iniciar la aplicación.
  - `MyApp` es un widget `StatelessWidget` que define la estructura general de la aplicación.
  - `MaterialApp` configura el título, el tema y la pantalla principal de la aplicación.

- **LineChartScreen**:
  - `LineChartScreen` es un `StatefulWidget` que mostrará el gráfico de líneas de temperatura.
  - `_LineChartScreenState` es el estado asociado a `LineChartScreen`.

- **initState**:
  - Inicializa el servicio MQTT con el broker y el `clientId`.
  - Escucha el stream de temperatura y actualiza el estado cuando llegue un nuevo valor.

- **build**:
  - `Scaffold` configura la estructura visual de la pantalla.
  - `SfCartesianChart` muestra el gráfico de líneas.
  - `DateTimeAxis` configura el eje X como eje de tiempo.
  - `LineSeries` define la serie de datos para el gráfico, mapeando los valores de X y Y a los datos de temperatura y tiempo.

- **TemperatureData**:
  - Clase para representar los datos de temperatura con un tiempo y un valor de temperatura.

### Paso 5: Ejecutar la aplicación

1. Conecta un dispositivo o inicia un emulador.
2. Ejecuta el siguiente comando en la terminal:
   ```bash
   flutter run
   ```

