#include <Wire.h>
#include <MPU6050.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "MAX30100_PulseOximeter.h"
#include <WiFi.h>
#include <PubSubClient.h>

// Definiciones para la pantalla OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define RADIUS 30  // Radio del círculo

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Definiciones para el sensor DHT11
#define DHTPIN 25
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Definiciones para los pines del buzzer, botones y LEDs
#define BUZZER_PIN 13
#define BUTTON_PIN 14      // Botón para cambiar pantallas
#define LED_BUTTON_PIN 15  // Botón para encender/apagar LED y buzzer
#define LED_RED_PIN 12
#define LED_GREEN_PIN 27
#define LED_BLUE_PIN 26

// Definiciones para I2C
#define I2C_SDA 21
#define I2C_SCL 22

// Definición del período de reporte
#define REPORTING_PERIOD_MS 1000

// para segundos
unsigned long lastHeartRatePublish = 0;
const unsigned long heartRateInterval = 5000;

PulseOximeter pox;
MPU6050 mpu;

uint32_t tsLastReport = 0;
bool isOn = false;  // Variable de estado para buzzer y LEDs

int stepCount = 0;    // Contador de pasos
int displayMode = 0;  // Modo de pantalla actual (0: Frecuencia cardíaca, 1: Oxígeno en sangre, 2: Temperatura, 3: Humedad, 4: Pasos, 5: Nivel de orientación)

const int graphWidth = SCREEN_WIDTH;
int xPos = 0;  // Posición actual de la gráfica de electrocardiograma

// Arreglo para almacenar valores de la gráfica de ECG
int ecgData[SCREEN_WIDTH];

// Wi-Fi credentials
const char* ssid = "Redmi Note 10S";
const char* password = "";

// MQTT Broker settings
const char* mqtt_server = "192.168.149.216";  // Cambia por tu servidor MQTT

WiFiClient espClient;
PubSubClient client(espClient);

// Tópicos MQTT
const char* heartRateTopic = "sensor/heartRate";
const char* spO2Topic = "sensor/spO2";
const char* temperatureTopic = "sensor/temp";
const char* humidityTopic = "sensor/humidity";
const char* stepCountTopic = "sensor/stepCount";
const char* giroscopioTopic = "giroscopio/gx";
const char* buzzerTopic = "sensor/buzzer";
const char* rgbTopic = "sensor/rgb";


// Función de callback para mensajes MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == buzzerTopic) {
    if (message == "ON") {
      digitalWrite(BUZZER_PIN, HIGH);
    } else if (message == "OFF") {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  // Control de LEDs RGB
  if (String(topic) == rgbTopic) {
    if (message == "RED") {
      digitalWrite(LED_RED_PIN, HIGH);
      digitalWrite(LED_GREEN_PIN, LOW);
      digitalWrite(LED_BLUE_PIN, LOW);
    } else if (message == "GREEN") {
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, HIGH);
      digitalWrite(LED_BLUE_PIN, LOW);
    } else if (message == "BLUE") {
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, LOW);
      digitalWrite(LED_BLUE_PIN, HIGH);
    } else if (message == "OFF") {
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, LOW);
      digitalWrite(LED_BLUE_PIN, LOW);
    }
  }
}




// Función de callback para detectar latidos
void onBeatDetected() {
  Serial.println("Beat!");
}
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando a MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("conectado");
      client.subscribe(rgbTopic);   // Suscripción a RGB
      client.subscribe(buzzerTopic); // Suscripción al buzzer
    } else {
      Serial.print("Error, rc=");
      Serial.print(client.state());
      Serial.println(" intentando de nuevo en 5 segundos");
      delay(5000);
    }
  }
}


void setup() {
  Serial.begin(115200);
  
  // Inicializa el bus I2C con los pines SDA y SCL personalizados
  Wire.begin(I2C_SDA, I2C_SCL);

  // Inicializar la pantalla OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("No se pudo encontrar una pantalla OLED"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Inicializando..."));
  display.display();

  // Inicializar el sensor DHT11
  dht.begin();

  // Inicializar el sensor MAX30100
  if (!pox.begin()) {
    Serial.println("No se pudo encontrar el sensor MAX30100. Verifica las conexiones.");
    for (;;)
      ;
  } else {
    Serial.println("Sensor MAX30100 encontrado.");
  }

  // Configurar el sensor MAX30100
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);

  // Configurar los pines del buzzer, botones y LEDs
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);      // Usa resistencia interna de pull-up para cambiar pantallas
  pinMode(LED_BUTTON_PIN, INPUT_PULLUP);  // Botón para LEDs y buzzer
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  // Acelerómetro y Giroscopio
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("No se pudo conectar con el MPU6050.");
    for (;;)
      ;
  } else {
    Serial.println("MPU6050 encontrado.");
  }

  // Asegúrate de que todos los dispositivos estén apagados al inicio
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, LOW);

  // Inicializar el arreglo de ECG
  for (int i = 0; i < SCREEN_WIDTH; i++) {
    ecgData[i] = SCREEN_HEIGHT / 2;
  }

  // Conectar a Wi-Fi
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Actualizar datos del sensor MAX30100
  pox.update();
  int heartRate = pox.getHeartRate();

  // Leer datos del sensor DHT11
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Leer datos del acelerómetro y giroscopio
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Contar pasos (esto es un ejemplo simple, puede que necesites una lógica más compleja)
  static int16_t prevZ = 0;
  if (az > 10000 && prevZ <= 10000) {  // Ajusta el umbral según tus necesidades
    stepCount++;
  }
  prevZ = az;

  // Verificar si el botón de cambio de pantalla ha sido presionado
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);

  unsigned long currentMillis = millis();
  if (lastButtonState == HIGH && currentButtonState == LOW && (currentMillis - tsLastReport) > 200) {  // Anti-rebote
    displayMode = (displayMode + 1) % 6;                                                               // Cambiar entre 0, 1, 2, 3, 4 y 5
    delay(50);                                                                                         // Espera breve para evitar múltiples registros de cambio
  }
  lastButtonState = currentButtonState;

  // Verificar si el botón de LED y buzzer ha sido presionado
  static bool lastLedButtonState = HIGH;
  bool currentLedButtonState = digitalRead(LED_BUTTON_PIN);

  if (lastLedButtonState == HIGH && currentLedButtonState == LOW && (currentMillis - tsLastReport) > 200) {  // Anti-rebote
    // Alternar el estado de los LEDs y el buzzer
    isOn = !isOn;
    digitalWrite(BUZZER_PIN, isOn ? HIGH : LOW);

    digitalWrite(LED_RED_PIN, isOn ? HIGH : LOW);
    delay(50);  // Espera breve para evitar múltiples registros de cambio
  }

  lastLedButtonState = currentLedButtonState;

  // Mostrar datos cada REPORTING_PERIOD_MS milisegundos
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    tsLastReport = millis();
    display.clearDisplay();

    switch (displayMode) {
      case 0:  // Frecuencia cardíaca
        display.setCursor(0, 0);
        display.print("Pulso: ");
        display.print(pox.getHeartRate());
        display.println(" BPM");
        if (millis() - lastHeartRatePublish > heartRateInterval) {
          lastHeartRatePublish = millis();
          client.publish(heartRateTopic, String(pox.getHeartRate()).c_str(), true);
        }
        drawHeartRateECG(heartRate);
        break;
      case 1:  // Oxígeno en sangre
        display.setCursor(0, 0);
        display.print("SPO2: ");
        display.print(pox.getSpO2());
        display.println(" %");
        if (millis() - lastHeartRatePublish > heartRateInterval) {
          lastHeartRatePublish = millis();
          client.publish(spO2Topic, String(pox.getSpO2()).c_str(), true);
        }
        break;
      case 2:  // Temperatura
        display.setCursor(0, 0);
        display.print("Temp: ");
        display.print(temp);
        display.println(" C");
        client.publish(temperatureTopic, String(temp).c_str(), true);

        // Llamar a la función para dibujar la barra de temperatura
        drawTemperatureBar(temp);
        break;
      case 3:  // Humedad
        display.setCursor(0, 0);
        display.print("Humedad: ");
        display.print(humidity);
        display.println(" %");
        client.publish(humidityTopic, String(humidity).c_str(), true);
        drawHumidityBar(humidity);
        break;
      case 4:  // Contador de pasos
        display.setCursor(0, 0);
        display.print("Pasos: ");
        display.println(stepCount);
        client.publish(stepCountTopic, String(stepCount).c_str(), true);

        drawStepCountBar(stepCount);
        break;
      case 5:  // Nivel de orientación
      drawGirosCircularGauge(gx);  // Solo se necesita `gx` para la función circular
      client.publish("giroscopio/gx", String(gx).c_str(), true);
      client.publish("giroscopio/gy", String(gy).c_str(), true);
      client.publish("giroscopio/gz", String(gz).c_str(), true);
    break;

    }

    display.display();
  }

  // Actualizar el arreglo de ECG
  ecgData[xPos] = map(heartRate, 0, 255, SCREEN_HEIGHT - 1, 0);  // Solo un ejemplo, deberías ajustar esto según tu entrada real de ECG

  // Avanzar la posición de la gráfica
  xPos++;
  if (xPos >= SCREEN_WIDTH) {
    xPos = 0;
  }

  delay(10);  // Ajusta el delay según lo necesario

  // Control de LEDs basado en la temperatura
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);

  if (temp <= 28.1) {
    digitalWrite(LED_GREEN_PIN, HIGH);  // LED verde si temperatura <= 20
  } else if (temp > 28.2 && temp <= 28.3) {
    digitalWrite(LED_BLUE_PIN, HIGH);  // LED azul si temperatura entre 25 y 30
  } else if (temp > 28.4) {
    digitalWrite(LED_RED_PIN, HIGH);  // LED rojo si temperatura > 45
  }

  // Verificar si se han alcanzado 50 pasos para activar el buzzer
  if (stepCount >= 3) {
    digitalWrite(BUZZER_PIN, HIGH);  // Encender buzzer
    delay(10000);                    // Mantener encendido por 10 segundos
    digitalWrite(BUZZER_PIN, LOW);   // Apagar buzzer
    stepCount = 0;                   // Reiniciar contador de pasos
  }
}

void drawGirosCircularGauge(int16_t gx) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Giroscopio GX:");

  // Dibujar el círculo
  int16_t centerX = SCREEN_WIDTH / 2;
  int16_t centerY = SCREEN_HEIGHT / 2;
  display.drawCircle(centerX, centerY, RADIUS, SSD1306_WHITE);

  // Calcular el ángulo basado en GX
  float angle = map(gx, -32767, 32767, -90, 90);  // Ajuste de rango

  // Convertir el ángulo a coordenadas de la aguja
  int16_t x = centerX + RADIUS * cos(radians(angle));
  int16_t y = centerY + RADIUS * sin(radians(angle));

  // Dibujar la aguja
  display.drawLine(centerX, centerY, x, y, SSD1306_WHITE);

  display.display();
}
void drawHeartRateECG(int heartRate) {
  display.clearDisplay();  // Limpia la pantalla antes de dibujar
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Frecuencia cardíaca:");
  display.setCursor(0, 10);
  display.print(heartRate);
  display.println(" bpm");

  if (heartRate == 0) {
    for (int i = 0; i < SCREEN_WIDTH; i++) {
      ecgData[i] = SCREEN_HEIGHT / 2;
    }
  } else {
    for (int i = 0; i < SCREEN_WIDTH - 1; i++) {
      ecgData[i] = ecgData[i + 1];
    }
    int ecgValue = random(SCREEN_HEIGHT / 2 - 10, SCREEN_HEIGHT / 2 + 10);
    ecgData[SCREEN_WIDTH - 1] = ecgValue;
  }

  for (int x = 0; x < SCREEN_WIDTH - 1; x++) {
    display.drawLine(x, ecgData[x], x + 1, ecgData[x + 1], SSD1306_WHITE);
  }

  display.display();  // Actualiza la pantalla para mostrar los cambios
}
void drawSpO2Bar(int spO2) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Nivel de oxígeno:");
  display.setCursor(0, 10);
  display.print(spO2);
  display.println(" %");

  // Dibuja una gráfica horizontal para el nivel de oxígeno
  int graphWidth = SCREEN_WIDTH - 20;
  int barHeight = 10;
  int maxSpO2 = 100;  // Valor máximo para la gráfica

  int barWidth = map(spO2, 0, maxSpO2, 0, graphWidth);
  display.fillRect(10, 20, barWidth, barHeight, SSD1306_WHITE);
  display.display();  // Actualiza la pantalla para mostrar los cambios

}


void drawTemperatureBar(float temp) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Temperatura:");
  display.setCursor(0, 10);
  display.print(temp);
  display.println(" C");

  // Dibuja una gráfica horizontal para la temperatura
  int graphWidth = SCREEN_WIDTH - 20;
  int barHeight = 10;
  float maxTemp = 50.0;  // Valor máximo para la gráfica

  int barWidth = map(temp, 0, maxTemp, 0, graphWidth);
  display.fillRect(10, 20, barWidth, barHeight, SSD1306_WHITE);
  display.display();  // Actualiza la pantalla para mostrar los cambios

}


void drawHumidityBar(float humidity) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Humedad:");
  display.setCursor(0, 10);
  display.print(humidity);
  display.println(" %");

  // Dibuja una gráfica horizontal para la humedad
  int graphWidth = SCREEN_WIDTH - 20;
  int barHeight = 10;
  int maxHumidity = 100;  // Valor máximo para la gráfica

  int barWidth = map(humidity, 0, maxHumidity, 0, graphWidth);
  display.fillRect(10, 20, barWidth, barHeight, SSD1306_WHITE);
  display.display();  // Actualiza la pantalla para mostrar los cambios

}


void drawStepCountBar(int stepCount) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Contador de pasos:");
  display.setCursor(0, 10);
  display.print(stepCount);

  // Dibuja una gráfica horizontal para el contador de pasos
  int graphWidth = SCREEN_WIDTH - 20;
  int barHeight = 10;
  int maxSteps = 1000;  // Valor máximo para la gráfica

  int barWidth = map(stepCount, 0, maxSteps, 0, graphWidth);
  display.fillRect(10, 20, barWidth, barHeight, SSD1306_WHITE);
  display.display();  // Actualiza la pantalla para mostrar los cambios
}
