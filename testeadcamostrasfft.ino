/*
=========================================================
 REC-WAVE
 TESTE: ADC -> AMOSTRAS -> FFT

 MAX9814
 OUT -> GPIO 32

 Fluxo:

 Som
  ↓
 MAX9814
  ↓
 Sinal elétrico
  ↓
 ADC do ESP32
  ↓
 512 amostras
  ↓
 FFT
  ↓
 Frequência dominante
=========================================================
*/

#include <arduinoFFT.h>

// -----------------------------
// CONFIGURAÇÕES
// -----------------------------

#define MIC_PIN 32

const uint16_t SAMPLES = 512;
const double SAMPLING_FREQUENCY = 10000.0;

// Vetores que vão guardar as amostras
double vReal[SAMPLES];
double vImag[SAMPLES];

// Objeto da FFT
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);


// -----------------------------
// SETUP
// -----------------------------

void setup()
{
    Serial.begin(115200);

    analogReadResolution(12);

    delay(2000);

    Serial.println();
    Serial.println("====================================");
    Serial.println("REC-WAVE");
    Serial.println("TESTE ADC + FFT");
    Serial.println("====================================");
    Serial.println();

    Serial.println("Aguardando sinal...");
}


// -----------------------------
// LOOP
// -----------------------------

void loop()
{
    // Período entre cada amostra
    unsigned long periodo =
        1000000.0 / SAMPLING_FREQUENCY;


    // ==========================================
    // 1. COLETAR 512 AMOSTRAS
    // ==========================================

    for (uint16_t i = 0; i < SAMPLES; i++)
    {
        unsigned long inicio = micros();

        // Lê o ADC do MAX9814
        vReal[i] = analogRead(MIC_PIN);

        // Parte imaginária começa em zero
        vImag[i] = 0;

        // Espera até chegar o momento da próxima amostra
        while (micros() - inicio < periodo)
        {
        }
    }


    // ==========================================
    // 2. REMOVER COMPONENTE DC
    // ==========================================

    FFT.dcRemoval();


    // ==========================================
    // 3. APLICAR JANELA
    // ==========================================

    FFT.windowing(
        FFTWindow::Hamming,
        FFTDirection::Forward
    );


    // ==========================================
    // 4. CALCULAR FFT
    // ==========================================

    FFT.compute(FFTDirection::Forward);


    // ==========================================
    // 5. TRANSFORMAR EM MAGNITUDE
    // ==========================================

    FFT.complexToMagnitude();


    // ==========================================
    // 6. ENCONTRAR FREQUÊNCIA DOMINANTE
    // ==========================================

    double frequencia = FFT.majorPeak();


    // ==========================================
    // 7. MOSTRAR RESULTADO
    // ==========================================

    Serial.print("Frequencia detectada: ");

    Serial.print(frequencia, 2);

    Serial.println(" Hz");

    Serial.println();

    delay(300);
}