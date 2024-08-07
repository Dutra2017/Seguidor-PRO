#include <Arduino.h>
#include <QTRsensors.h>

#define GENERO "Binario"

#define VERSAO 1.0

// Definição Pinos da ponte H
const int IN1 = 2;    // Horario Motor Esquerdo //Trocar pelo 4
const int IN2 = 4;    // Antihorario Motor Esquerdo //Trocar pelo 5
const int IN3 = 5;    // Horario Motor Direito //Trocar pelo 18
const int IN4 = 18;   // Antihorario Motor Direito //Trocar pelo 19
const int SLEEP = 21; // Ativa ou desativa o output da ponte H //trocar pelo 22
const int FALL = 19;  // Detecta se tem algum erro //trocar pelo 21

#define OFFSET 0 // Diferenca de potencia entre os dois motores

void calibrar();
double calculoPid();
void controlePid();
void controleMotor(int vel_M1, int vel_M2);
void contagem_de_voltas();
void deteccao_faixa();

const int BITS = 10;    // Define a resolucao do pwm para 10 Bits (2^10 ou 1024)
const int FREQ = 10000; // Define a frequencia do pwm
const int IN1_chanel = 0;
const int IN2_chanel = 1;
const int IN3_chanel = 2;
const int IN4_chanel = 3;

int velMax_M1 = 1000 + OFFSET;
int velMax_M2 = 1000;
int velMin_M1 = 664 + OFFSET;
int velMin_M2 = 664;

// Setpoint 8 sensores: 3500
// Setpoint 6 sensores: 2365

const int setpoint = 2365;

const uint8_t NUM_SENSORES = 6;
uint16_t valorSensores[NUM_SENSORES];

const int BOTAO_CONTROLE = 23;
const int SENSOR_CURVA = 15;
const int SENSOR_PARADA = 34;
unsigned long int tempo;

int ultimo_val_sensor = 0;

double kp_c = 0.28, ki_c = 0, kd_c = 0;
double kp = 0.08, ki = 0, kd = 0.06;
// Varriaveis 8 sensores: kp = 0.13, ki = 0, kd = 0.8

bool curva = 0;

int n_voltas = 1; // Variável para definir o número de voltas na pista
int cont_voltas = 0;

bool Linha_preta = true;

bool state_esquerdo = 0;
bool state_direito = 0;

QTRSensors qtr; // Objeto qtr

unsigned long int tempo_sensor_parada = 0;
unsigned long int tempo_sensor_curva = 0;

int var = 0;

void setup()
{
  Serial.begin(115200);

  pinMode(SLEEP, OUTPUT);
  pinMode(FALL, INPUT_PULLUP);
  pinMode(SENSOR_CURVA, INPUT);
  pinMode(SENSOR_PARADA, INPUT);
  pinMode(BOTAO_CONTROLE, INPUT_PULLUP);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  ledcAttachPin(IN1, IN1_chanel);
  ledcAttachPin(IN2, IN2_chanel);
  ledcAttachPin(IN3, IN3_chanel);
  ledcAttachPin(IN4, IN4_chanel);

  ledcSetup(IN1_chanel, FREQ, BITS);
  ledcSetup(IN2_chanel, FREQ, BITS);
  ledcSetup(IN3_chanel, FREQ, BITS);
  ledcSetup(IN4_chanel, FREQ, BITS);

  controleMotor(0, 0);
}

void loop()
{

  unsigned int tempo_pressionado = 0;

  if (!digitalRead(BOTAO_CONTROLE))
  {
    tempo = millis();

    while (!digitalRead(BOTAO_CONTROLE))
    {
      tempo_pressionado = millis() - tempo;
    }
  }

  while (tempo_pressionado <= 500 && tempo_pressionado != 0)
  {

    if (digitalRead(FALL) && cont_voltas < n_voltas + 1)
    {
      digitalWrite(SLEEP, 1);
      deteccao_faixa();
      controlePid();
    }

    else
    {
      digitalWrite(SLEEP, 0);
      curva = 0;
      cont_voltas = 0;
      break;
    }
  }

  if (tempo_pressionado >= 1200 && tempo_pressionado != 0)
  {
    calibrar();
  }
}

void calibrar()
{
  qtr.setTypeAnalog();
  qtr.setSensorPins((const uint8_t[]){27, 26, 25, 33, 32, 35}, NUM_SENSORES);
  qtr.setEmitterPin(12);

  delay(500);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);

  for (uint16_t i = 0; i < 400; i++)
  {
    qtr.calibrate();
  }
  digitalWrite(2, LOW);

  delay(1000);
}

double calculoPid(double input, double kp, double ki, double kd)
{
  double erro, valor_kp, valor_kd, Pid;
  static double valor_ki;

  erro = setpoint - input;

  valor_kp = erro * kp;

  valor_ki += erro * ki;

  valor_kd = (ultimo_val_sensor - input) * kd;

  ultimo_val_sensor = input;

  Pid = valor_kp + valor_ki + valor_kd;

  return Pid;
}

void controlePid()
{

  double pid;

  static int linha_esquerda = 0;

  int vel_M1;
  int vel_M2;

  uint16_t val_sensor = qtr.readLineBlack(valorSensores);

  if (curva == 1 /*&& (valorSensores[2] < 900 && valorSensores[3] < 900)*/)
  {
    pid = calculoPid(val_sensor, kp_c, ki_c, kd_c);

    if (vel_M1 < velMin_M1 - 20)
    {
      vel_M1 = -velMin_M1 - pid;
    }

    if (vel_M2 < velMin_M2 - 20)
    {
      vel_M2 = -velMin_M2 + pid;
    }
  }
  else if (curva == 0 && (valorSensores[0] < 900 || valorSensores[5] < 900))
  {
    pid = calculoPid(val_sensor, kp, ki, kd);

    if (vel_M1 < 0)
    {
      vel_M1 = velMin_M1 - pid;
    }

    if (vel_M2 < 0)
    {
      vel_M2 = velMin_M2 + pid;
    }
  }

  else
  {
    pid = calculoPid(val_sensor, kp_c, ki_c, kd_c);

    if (vel_M1 < velMin_M1 - 70)
    {
      vel_M1 = -velMin_M1 - pid;
    }

    if (vel_M2 < velMin_M2 - 70)
    {
      vel_M2 = -velMin_M2 + pid;
    }
  }

  vel_M1 = velMin_M1 - pid;
  vel_M2 = velMin_M2 + pid;

  if (vel_M1 > velMax_M1)
  {
    vel_M1 = velMax_M1;
  }
  if (vel_M2 > velMax_M2)
  {
    vel_M2 = velMax_M2;
  }

  controleMotor(vel_M1, vel_M2);
}

void controleMotor(int vel_M1, int vel_M2)
{
  if (vel_M1 > 0)
  {
    ledcWrite(IN1_chanel, 0);
    ledcWrite(IN2_chanel, vel_M1);
    // Serial.print("M1: ");
    // Serial.print(vel_M1);
    // Serial.print('\t');
  }
  else
  {
    ledcWrite(IN1_chanel, -vel_M1);
    ledcWrite(IN2_chanel, 0);
    // Serial.print("M1: ");
    // Serial.print(-vel_M1);
    // Serial.print('\t');
  }

  if (vel_M2 > 0)
  {
    ledcWrite(IN3_chanel, vel_M2);
    ledcWrite(IN4_chanel, 0);
    // Serial.print("M2: ");
    // Serial.print(vel_M2);
    // Serial.println('\t');
  }
  else
  {
    ledcWrite(IN3_chanel, 0);
    ledcWrite(IN4_chanel, -vel_M2);
    // Serial.print("M2: ");
    // Serial.print(-vel_M2);
    // Serial.print('\t');
    // Serial.print(cont_voltas);
    // Serial.println('\t');
  }
}

void deteccao_faixa()
{
  static int sensor_prioritario = 0;

  if (digitalRead(SENSOR_CURVA) == Linha_preta && state_esquerdo == 0)
  {
    if (sensor_prioritario == 0 && state_direito == 0)
    {
      tempo_sensor_curva = millis();
      sensor_prioritario = 1;
    }

    state_esquerdo = 1;

  }
  else if (digitalRead(SENSOR_CURVA) != Linha_preta && state_esquerdo == 1)
  {
    state_esquerdo = 0;
  }

  while(millis() - tempo_sensor_curva < 100)
  {
    if(digitalRead(SENSOR_PARADA) == Linha_preta && state_direito == 0)
    {
      sensor_prioritario = 0;
      state_direito = 1;
      break;
    }
    
  }

  if(millis() - tempo_sensor_curva > 100 && sensor_prioritario == 1 )
  {
    curva = !curva;
    sensor_prioritario = 0;
  }
  
// //------------------------------------------------------------------------------

  if (digitalRead(SENSOR_PARADA) == Linha_preta && state_direito == 0)
  {
    if (sensor_prioritario == 0 && state_esquerdo == 0)
    {
      tempo_sensor_parada = millis();
      sensor_prioritario = 2;
    }
    state_direito = 1;
  }

  else if (digitalRead(SENSOR_PARADA) == !Linha_preta && state_direito == 1)
  {
    state_direito = 0;
  }

  while(millis() - tempo_sensor_parada < 100)
  {
    if(digitalRead(SENSOR_CURVA) == Linha_preta && state_esquerdo == 0)
    {
      sensor_prioritario = 0;
      state_esquerdo = 1;
      break;
    }
  
  }
  if(millis() - tempo_sensor_parada > 100 && sensor_prioritario == 2)
  {
    cont_voltas++;
    sensor_prioritario = 0;
  }
  
}
