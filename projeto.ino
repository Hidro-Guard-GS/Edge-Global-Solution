// Inclusão das bibliotecas necessárias
#include <LiquidCrystal_I2C.h> // Biblioteca para o display LCD via I2C
#include <Wire.h>              // Biblioteca para comunicação I2C
#include <RTClib.h>            // Biblioteca para o RTC (relógio de tempo real)
#include <EEPROM.h>            // Biblioteca para uso da memória EEPROM

// === LCD ===
LiquidCrystal_I2C lcd(0x27, 16, 2); // Inicializa o LCD no endereço I2C 0x27 com 16 colunas e 2 linhas

// === RTC ===
RTC_DS1307 rtc; // Inicializa o objeto do módulo RTC DS1307

// === Definição dos pinos usados ===
#define TRIG_PIN 3         // Pino de disparo do sensor ultrassônico
#define ECHO_PIN 2         // Pino de eco do sensor ultrassônico
#define BUTTON_PIN A2      // Pino do botão de iniciar (entrada analógica usada como digital)
const short int ledVermelho = 7;
const short int ledAmarelo  = 6;
const short int ledVerde    = 5;
const short int pinoBuzzer  = 8; // Pino do buzzer para alarme sonoro

// === Variáveis globais ===
bool iniciado = false;         // Indica se o sistema já foi iniciado
int enderecoEEPROM = 0;        // Endereço atual na EEPROM para salvar dados
float ultimaDistancia = -1;    // Armazena a última distância medida para comparar mudanças

// === Estrutura de dados para salvar na EEPROM ===
struct Registro {
  DateTime horario; // Data e hora do registro
  float distancia;  // Distância medida
};

// === Definição de caracteres personalizados para o LCD ===
byte gota[8] = { B00100, B01110, B01110, B11111, B11111, B11111, B01110, B00000 };
byte onda1[8] = { B00000, B00000, B00001, B00011, B00110, B01100, B11000, B10000 };
byte onda2[8] = { B00000, B00001, B00011, B00110, B01100, B11000, B10000, B00000 };
byte onda3[8] = { B00001, B00011, B00110, B01100, B11000, B10000, B00000, B00000 };

void setup() {
  Serial.begin(9600);    // Inicializa a comunicação serial
  rtc.begin();           // Inicializa o RTC
  lcd.begin(16, 2);      // Inicializa o LCD
  lcd.backlight();       // Liga a luz de fundo do LCD

  // Criação dos caracteres personalizados no LCD
  lcd.createChar(0, gota);
  lcd.createChar(1, onda1);
  lcd.createChar(2, onda2);
  lcd.createChar(3, onda3);

  // Configuração dos pinos
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ledVermelho, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Botão com resistor pull-up
  pinMode(pinoBuzzer, OUTPUT);

  // Animações de inicialização
  animarEntradaOnda();
  animarSubidaLogo();
  piscarLogo();
  animarSaidaVarredura();

  // Mensagem de boas-vindas no LCD
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Iniciando...");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aperte p/ iniciar");

}

void loop() {
  // Aguarda o botão ser pressionado para iniciar
  if (!iniciado) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      iniciado = true;
      lcd.clear();
    } else {
      return; // Não faz nada até o botão ser pressionado
    }
  }

  // === Medição de distância com sensor ultrassônico ===
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracao = pulseIn(ECHO_PIN, HIGH);
  float distancia = duracao * 0.034 / 2; // Conversão do tempo em distância (cm)

  // Verificação de medida válida
  if (distancia <= 0 || distancia > 400) return;

  // === Exibe a distância no LCD ===
  lcd.setCursor(0, 0);
  lcd.print("Distancia:");
  lcd.setCursor(0, 1);
  lcd.print(distancia, 1);
  lcd.print(" cm       ");

  // === Salva nova medição se houve mudança ===
  if (distancia != ultimaDistancia) {
    ultimaDistancia = distancia;

    Registro r;
    r.horario = rtc.now();
    r.distancia = distancia;

    // Salva na EEPROM se ainda houver espaço
    if (enderecoEEPROM + sizeof(Registro) < EEPROM.length()) {
      EEPROM.put(enderecoEEPROM, r);

      // Verifica se o dado foi salvo corretamente
      Registro verificado;
      EEPROM.get(enderecoEEPROM, verificado);

      Serial.print("[");
      Serial.print(verificado.horario.timestamp(DateTime::TIMESTAMP_TIME));
      Serial.print("] Distancia SALVA: ");
      Serial.print(verificado.distancia, 1);
      Serial.print(" cm (Endereco: ");
      Serial.print(enderecoEEPROM);
      Serial.println(")");

      enderecoEEPROM += sizeof(Registro); // Avança para próximo endereço
    } else {
      Serial.println(">> EEPROM cheia, nao salvou mais.");
    }
  }

  // === Lógica de alerta por LEDs e buzzer ===
  if (distancia > 180) {
    // Situação segura
    digitalWrite(ledVerde, HIGH);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, LOW);
    noTone(pinoBuzzer);
  } else if (distancia <= 180 && distancia >= 150) {
    // Alerta intermediário
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, HIGH);
    digitalWrite(ledVermelho, LOW);
    tocarAlarme(1); // Alarme nível 1
  } else {
    // Situação de perigo
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, HIGH);
    tocarAlarme(2); // Alarme nível 2
  }

  delay(1000); // Aguarda 1 segundo antes da próxima medição
}

// === Função de alarme sonoro com diferentes padrões ===
void tocarAlarme(int tipoAlerta) {
  if (tipoAlerta == 1) {
    tone(pinoBuzzer, 1000); // Frequência 1kHz
    delay(3000);
    noTone(pinoBuzzer);
    delay(3000);
  } else if (tipoAlerta == 2) {
    tone(pinoBuzzer, 2000); // Frequência 2kHz
    delay(5000);
    noTone(pinoBuzzer);
    delay(2000);
  }
}

// === Lê e imprime todos os dados salvos na EEPROM via Serial Monitor ===
void get_log() {
  Serial.println("== REGISTROS SALVOS NA EEPROM ==");
  int endereco = 0;
  while (endereco + sizeof(Registro) <= EEPROM.length()) {
    Registro r;
    EEPROM.get(endereco, r);

    // Se o ano estiver fora do esperado, assume fim dos dados válidos
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

// === Animação de entrada com ondas ===
void animarEntradaOnda() {
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 0);
    lcd.write(byte((i % 3) + 1)); // Alterna os 3 tipos de onda
    delay(80);
  }
  delay(300);
  lcd.clear();
}

// === Animação que simula o nome subindo ===
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
  lcd.write(byte(0)); // Adiciona a gota ao final
  delay(1000);
}

// === Pisca o nome no LCD como efeito visual ===
void piscarLogo() {
  for (int i = 0; i < 2; i++) {
    lcd.noDisplay();
    delay(300);
    lcd.display();
    delay(300);
  }
  delay(700);
}

// === Animação final com varredura de ondas ===
void animarSaidaVarredura() {
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i, 0);
    lcd.write(byte((i % 3) + 1));
    delay(80);
  }
  delay(300);
  lcd.clear();
}