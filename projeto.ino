#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

// === LCD ===
LiquidCrystal_I2C lcd(0x27, 16, 2);

// === RTC ===
RTC_DS1307 rtc;

// === Pinos ===
#define TRIG_PIN 3
#define ECHO_PIN 2
#define BUTTON_PIN A2
const short int ledVermelho = 7;
const short int ledAmarelo  = 6;
const short int ledVerde    = 5;
const short int pinoBuzzer  = 8;

// === Variáveis ===
bool iniciado = false;
int enderecoEEPROM = 0;
float ultimaDistancia = -1; // Valor inicial fora do possível

// === Estrutura para salvar dados ===
struct Registro {
  DateTime horario;
  float distancia;
};

// === Caracteres personalizados ===
byte gota[8] = { B00100, B01110, B01110, B11111, B11111, B11111, B01110, B00000 };
byte onda1[8] = { B00000, B00000, B00001, B00011, B00110, B01100, B11000, B10000 };
byte onda2[8] = { B00000, B00001, B00011, B00110, B01100, B11000, B10000, B00000 };
byte onda3[8] = { B00001, B00011, B00110, B01100, B11000, B10000, B00000, B00000 };

void setup() {
  Serial.begin(9600);
  rtc.begin();
  lcd.begin(16, 2);
  lcd.backlight();

  // Criar caracteres
  lcd.createChar(0, gota);
  lcd.createChar(1, onda1);
  lcd.createChar(2, onda2);
  lcd.createChar(3, onda3);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ledVermelho, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(pinoBuzzer, OUTPUT);

  animarEntradaOnda();
  animarSubidaLogo();
  piscarLogo();
  animarSaidaVarredura();

  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Iniciando...");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aperte p/ iniciar");

  // (Opcional) Descomente abaixo para ver os dados salvos na EEPROM ao iniciar
  // get_log();
}

void loop() {
  if (!iniciado) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      iniciado = true;
      lcd.clear();
    } else {
      return;
    }
  }

  // === Medição de distância ===
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracao = pulseIn(ECHO_PIN, HIGH);
  float distancia = duracao * 0.034 / 2;

  if (distancia <= 0 || distancia > 400) return;

  // === Mostrar no LCD ===
  lcd.setCursor(0, 0);
  lcd.print("Distancia:");
  lcd.setCursor(0, 1);
  lcd.print(distancia, 1);
  lcd.print(" cm       ");

  // === Se mudou, salva ===
  if (distancia != ultimaDistancia) {
    ultimaDistancia = distancia;

    Registro r;
    r.horario = rtc.now();
    r.distancia = distancia;

    if (enderecoEEPROM + sizeof(Registro) < EEPROM.length()) {
      EEPROM.put(enderecoEEPROM, r);

      // Ler de volta para verificar
      Registro verificado;
      EEPROM.get(enderecoEEPROM, verificado);

      Serial.print("[");
      Serial.print(verificado.horario.timestamp(DateTime::TIMESTAMP_TIME));
      Serial.print("] Distancia SALVA: ");
      Serial.print(verificado.distancia, 1);
      Serial.print(" cm (Endereco: ");
      Serial.print(enderecoEEPROM);
      Serial.println(")");

      enderecoEEPROM += sizeof(Registro);
    } else {
      Serial.println(">> EEPROM cheia, nao salvou mais.");
    }
  }

  // === Sistema de LEDs e Alarmes ===
  if (distancia > 180) {
    digitalWrite(ledVerde, HIGH);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, LOW);
    noTone(pinoBuzzer);
  } else if (distancia <= 180 && distancia >= 150) {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, HIGH);
    digitalWrite(ledVermelho, LOW);
    tocarAlarme(1);
  } else {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, HIGH);
    tocarAlarme(2);
  }

  delay(1000);
}

void tocarAlarme(int tipoAlerta) {
  if (tipoAlerta == 1) {
    tone(pinoBuzzer, 1000);
    delay(3000);
    noTone(pinoBuzzer);
    delay(3000);
  } else if (tipoAlerta == 2) {
    tone(pinoBuzzer, 2000);
    delay(5000);
    noTone(pinoBuzzer);
    delay(2000);
  }
}

// === Mostrar todos os dados salvos na EEPROM ===
void get_log() {
  Serial.println("== REGISTROS SALVOS NA EEPROM ==");
  int endereco = 0;
  while (endereco + sizeof(Registro) <= EEPROM.length()) {
    Registro r;
    EEPROM.get(endereco, r);

    // Se os dados forem inválidos, saia do loop
    if (r.horario.year() < 2020 || r.horario.year() > 2100) break;

    Serial.print("[");
    Serial.print(r.horario.timestamp(DateTime::TIMESTAMP_DATE));
    Serial.print(" ");
    Serial.print(r.horario.timestamp(DateTime::TIMESTAMP_TIME));
    Serial.print("] Distancia: ");
    Serial.print(r.distancia, 1);
    Serial.print(" cm (Endereco: ");
    Serial.print(endereco);
    Serial.println(")");

    endereco += sizeof(Registro);
  }
  Serial.println("== FIM DOS REGISTROS ==");
}

// === ANIMAÇÕES ===
void animarEntradaOnda() {
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 0);
    lcd.write(byte((i % 3) + 1));
    delay(80);
  }
  delay(300);
  lcd.clear();
}

void animarSubidaLogo() {
  String nome = "Hidro Guard";
  for (int i = 0; i < nome.length(); i++) {
    lcd.setCursor(i + 1, 1);
    lcd.print(nome[i]);
    delay(100);
    lcd.setCursor(i + 1, 1);
    lcd.print(" ");
    lcd.setCursor(i + 1, 0);
    lcd.print(nome[i]);
    delay(100);
  }
  lcd.setCursor(nome.length() + 2, 0);
  lcd.write(byte(0)); // gota
  delay(1000);
}

void piscarLogo() {
  for (int i = 0; i < 2; i++) {
    lcd.noDisplay();
    delay(300);
    lcd.display();
    delay(300);
  }
  delay(700);
}

void animarSaidaVarredura() {
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 0);
    lcd.write(byte((i % 3) + 1));
    delay(80);
  }
  delay(300);
  lcd.clear();
}
