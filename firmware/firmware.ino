// Пины для рулевого управления
const int pwmSteeringPin = 2;      // Пин для ШИМ руля от приемника
const int outputPinRight = 9;      // Пин для правого поворота
const int outputPinLeft = 10;      // Пин для левого поворота

// Пины для управления движением
const int pwmThrottlePin = 3;      // Пин для ШИМ газа/тормоза от приемника
const int outputPinForward = 5;    // Пин для движения вперед
const int outputPinReverse = 6;    // Пин для движения назад

// Настройки калибровки
const int CALIBRATION_SAMPLES = 100;
const float STEERING_DEADBAND = 0.03;
const int THROTTLE_DEADBAND = 50;  // Мертвая зона газа (±50 мкс)

// Переменные калибровки
float neutralSteeringVoltage = 0.35;
int calibratedNeutralSteeringPWM = 1500;
int calibratedNeutralThrottlePWM = 1500;
bool isSteeringCalibrated = false;
bool isThrottleCalibrated = false;

// Переменные
int steeringPulseWidth = 1500;
int throttlePulseWidth = 1500;
float steeringVoltage = 0.35;
int steeringRightValue = 0;
int steeringLeftValue = 0;
int throttleForwardValue = 0;
int throttleReverseValue = 0;

void setup() {
  // Настройка пинов
  pinMode(pwmSteeringPin, INPUT);
  pinMode(pwmThrottlePin, INPUT);
  pinMode(outputPinRight, OUTPUT);
  pinMode(outputPinLeft, OUTPUT);
  pinMode(outputPinForward, OUTPUT);
  pinMode(outputPinReverse, OUTPUT);
  
  // Устанавливаем частоту ШИМ для всех пинов
  TCCR1B = TCCR1B & 0b11111000 | 0x02;  // Пины 9, 10
  TCCR0B = TCCR0B & 0b11111000 | 0x02;  // Пины 5, 6
  
  // Инициализируем все выходы в 0
  analogWrite(outputPinRight, 0);
  analogWrite(outputPinLeft, 0);
  analogWrite(outputPinForward, 0);
  analogWrite(outputPinReverse, 0);
  
  Serial.begin(9600);
  
  // Калибровка обоих каналов
  calibrateChannels();
}

void calibrateChannels() {
  Serial.println("КАЛИБРОВКА... Держите руль и газ в нейтрали 3 секунды");
  
  // Калибровка руля
  long sumSteering = 0;
  long sumThrottle = 0;
  
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sumSteering += pulseIn(pwmSteeringPin, HIGH);
    sumThrottle += pulseIn(pwmThrottlePin, HIGH);
    delay(30);
  }
  
  // Калибровка руля
  calibratedNeutralSteeringPWM = sumSteering / CALIBRATION_SAMPLES;
  neutralSteeringVoltage = 0.25 + (calibratedNeutralSteeringPWM - 1000) * (0.20 / 1000.0);
  
  // Калибровка газа
  calibratedNeutralThrottlePWM = sumThrottle / CALIBRATION_SAMPLES;
  
  Serial.print("Руль - нейтраль: ");
  Serial.print(calibratedNeutralSteeringPWM);
  Serial.print(" мкс (");
  Serial.print(neutralSteeringVoltage, 3);
  Serial.println("В)");
  
  Serial.print("Газ - нейтраль: ");
  Serial.print(calibratedNeutralThrottlePWM);
  Serial.println(" мкс");
  
  isSteeringCalibrated = true;
  isThrottleCalibrated = true;
}

void loop() {
  if (!isSteeringCalibrated || !isThrottleCalibrated) return;
  
  // 1. Считываем оба канала от приемника
  steeringPulseWidth = pulseIn(pwmSteeringPin, HIGH);
  throttlePulseWidth = pulseIn(pwmThrottlePin, HIGH);
  
  // 2. ОБРАБОТКА РУЛЕВОГО УПРАВЛЕНИЯ
  steeringVoltage = 0.25 + (steeringPulseWidth - 1000) * (0.20 / 1000.0);
  float steeringDeviation = steeringVoltage - neutralSteeringVoltage;
  
  steeringRightValue = 0;
  steeringLeftValue = 0;
  
  if (steeringDeviation > STEERING_DEADBAND) {
    float ratio = (steeringDeviation - STEERING_DEADBAND) / (0.10 - STEERING_DEADBAND);
    steeringRightValue = ratio * 255;
    steeringRightValue = constrain(steeringRightValue, 0, 223);
  }
  else if (steeringDeviation < -STEERING_DEADBAND) {
    float ratio = (-steeringDeviation - STEERING_DEADBAND) / (0.10 - STEERING_DEADBAND);
    steeringLeftValue = ratio * 255;
    steeringLeftValue = constrain(steeringLeftValue, 0, 223);
  }
  
  // 3. ОБРАБОТКА УПРАВЛЕНИЯ ДВИЖЕНИЕМ С КАЛИБРОВКОЙ
  throttleForwardValue = 0;
  throttleReverseValue = 0;
  
  // Вычисляем отклонение газа от калиброванной нейтрали
  int throttleDeviation = throttlePulseWidth - calibratedNeutralThrottlePWM;
  
  // Логика управления движением ОТНОСИТЕЛЬНО калиброванной нейтрали
  if (throttleDeviation > THROTTLE_DEADBAND) {
    // ДВИЖЕНИЕ ВПЕРЕД
    throttleForwardValue = map(throttlePulseWidth, 
                              calibratedNeutralThrottlePWM + THROTTLE_DEADBAND, 
                              2000, 
                              0, 255);
    throttleForwardValue = constrain(throttleForwardValue, 0, 255);
  }
  else if (throttleDeviation < -THROTTLE_DEADBAND) {
    // ДВИЖЕНИЕ НАЗАД
    throttleReverseValue = map(throttlePulseWidth, 
                              calibratedNeutralThrottlePWM - THROTTLE_DEADBAND, 
                              1000, 
                              0, 255);
    throttleReverseValue = constrain(throttleReverseValue, 0, 255);
  }
  
  // 4. Подаем значения на все выходы
  analogWrite(outputPinRight, steeringRightValue);
  analogWrite(outputPinLeft, steeringLeftValue);
  analogWrite(outputPinForward, throttleForwardValue);
  analogWrite(outputPinReverse, throttleReverseValue);
  
  // 5. Отладочная информация
  Serial.print("Руль: ");
  Serial.print(steeringPulseWidth);
  Serial.print("(");
  Serial.print(steeringRightValue);
  Serial.print("/");
  Serial.print(steeringLeftValue);
  Serial.print(") | Газ: ");
  Serial.print(throttlePulseWidth);
  Serial.print("(N:");
  Serial.print(calibratedNeutralThrottlePWM);
  Serial.print(") F:");
  Serial.print(throttleForwardValue);
  Serial.print(" R:");
  Serial.print(throttleReverseValue);
  Serial.println();
  
  delay(50);
}
