/*
=========================================================
 REC-WAVE
 EMISSOR DE TESTES - SEQUENCIA DE FREQUENCIAS

 COMANDO DURANTE TRANSMISSAO:
 P = PARAR

 Autor: Tony Bonilha Vieira
=========================================================
*/

const int BUZZER_PIN = 15;
const int MAX_FREQUENCIAS = 20;

int frequencias[MAX_FREQUENCIAS];
int tempos[MAX_FREQUENCIAS];

int quantidadeFrequencias = 0;
bool transmissaoParada = false;


//------------------------------------------------------
// FUNÇÕES
//------------------------------------------------------

void criarSequencia();
void mostrarSequencia();
void executarSequencia();
bool verificarParada();


//======================================================
// SETUP
//======================================================

void setup()
{
    Serial.begin(115200);

    delay(3000);

    pinMode(BUZZER_PIN, OUTPUT);

    Serial.println();
    Serial.println("======================================");
    Serial.println("REC-WAVE");
    Serial.println("EMISSOR DE TESTES");
    Serial.println("======================================");
    Serial.println();

    criarSequencia();
}


//======================================================
// LOOP
//======================================================

void loop()
{
    if (Serial.available())
    {
        while (Serial.available())
        {
            Serial.read();
        }

        criarSequencia();
    }
}


//======================================================
// VERIFICAR COMANDO DE PARADA
//======================================================

bool verificarParada()
{
    if (Serial.available())
    {
        char comando = Serial.read();

        while (Serial.available())
        {
            Serial.read();
        }

        if (comando == 'P' || comando == 'p')
        {
            noTone(BUZZER_PIN);

            transmissaoParada = true;

            Serial.println();
            Serial.println("**************************************");
            Serial.println("       TRANSMISSAO INTERROMPIDA");
            Serial.println("**************************************");
            Serial.println();

            return true;
        }
    }

    return false;
}


//======================================================
// CRIAR SEQUENCIA
//======================================================

void criarSequencia()
{
    noTone(BUZZER_PIN);

    transmissaoParada = false;
    quantidadeFrequencias = 0;

    Serial.println();
    Serial.println("======================================");
    Serial.println("NOVA SEQUENCIA");
    Serial.println("======================================");
    Serial.println();

    while (quantidadeFrequencias < MAX_FREQUENCIAS)
    {
        // FREQUENCIA

        Serial.println("Insira a frequencia a ser emitida:");

        while (!Serial.available())
        {
            delay(10);
        }

        int frequencia = Serial.parseInt();

        while (Serial.available())
        {
            Serial.read();
        }

        if (frequencia <= 0)
        {
            Serial.println();
            Serial.println("Frequencia invalida.");
            Serial.println("Digite um valor maior que 0.");
            Serial.println();

            continue;
        }


        // TEMPO

        Serial.println();
        Serial.println("Insira o tempo de emissao em segundos:");

        while (!Serial.available())
        {
            delay(10);
        }

        int tempo = Serial.parseInt();

        while (Serial.available())
        {
            Serial.read();
        }

        if (tempo <= 0)
        {
            Serial.println();
            Serial.println("Tempo invalido.");
            Serial.println("Digite um valor maior que 0.");
            Serial.println();

            continue;
        }


        // ARMAZENA

        frequencias[quantidadeFrequencias] = frequencia;
        tempos[quantidadeFrequencias] = tempo;

        quantidadeFrequencias++;


        // MOSTRA O QUE FOI ADICIONADO

        Serial.println();
        Serial.println("Frequencia adicionada!");

        Serial.print("Frequencia: ");
        Serial.print(frequencia);
        Serial.println(" Hz");

        Serial.print("Tempo: ");
        Serial.print(tempo);
        Serial.println(" segundos");


        // LIMITE

        if (quantidadeFrequencias >= MAX_FREQUENCIAS)
        {
            Serial.println();
            Serial.println("Limite de 20 frequencias atingido.");
            break;
        }


        // PERGUNTA SE QUER OUTRA

        Serial.println();
        Serial.println("Quer inserir mais uma frequencia? (S/N)");

        while (!Serial.available())
        {
            delay(10);
        }

        char resposta = Serial.read();

        while (Serial.available())
        {
            Serial.read();
        }


        if (resposta == 'N' || resposta == 'n')
        {
            break;
        }

        Serial.println();
    }


    // MOSTRA A SEQUENCIA

    mostrarSequencia();


    // AGUARDA ENTER

    Serial.println();
    Serial.println("Pressione ENTER para iniciar.");
    Serial.println("Durante a transmissao, digite P para PARAR.");

    while (!Serial.available())
    {
        delay(10);
    }

    while (Serial.available())
    {
        Serial.read();
    }


    // EXECUTA

    executarSequencia();


    // FINAL

    if (!transmissaoParada)
    {
        Serial.println();
        Serial.println("======================================");
        Serial.println("SEQUENCIA FINALIZADA");
        Serial.println("======================================");
        Serial.println();
    }

    Serial.println("Digite qualquer coisa para criar");
    Serial.println("uma nova sequencia.");
}


//======================================================
// MOSTRAR SEQUENCIA
//======================================================

void mostrarSequencia()
{
    Serial.println();
    Serial.println("======================================");
    Serial.println("SEQUENCIA CONFIGURADA");
    Serial.println("======================================");
    Serial.println();

    for (int i = 0; i < quantidadeFrequencias; i++)
    {
        Serial.print("Etapa ");
        Serial.print(i + 1);
        Serial.print(": ");

        Serial.print(frequencias[i]);
        Serial.print(" Hz");

        Serial.print(" -> ");

        Serial.print(tempos[i]);
        Serial.println(" segundos");
    }

    Serial.println();
    Serial.println("======================================");
}


//======================================================
// EXECUTAR SEQUENCIA
//======================================================

void executarSequencia()
{
    Serial.println();
    Serial.println("======================================");
    Serial.println("INICIANDO TRANSMISSAO");
    Serial.println("======================================");
    Serial.println();

    transmissaoParada = false;


    for (int i = 0; i < quantidadeFrequencias; i++)
    {
        // Verifica se o usuario mandou P

        if (verificarParada())
        {
            return;
        }


        // MOSTRA ETAPA

        Serial.print("Etapa ");
        Serial.print(i + 1);
        Serial.print("/");
        Serial.println(quantidadeFrequencias);

        Serial.print("Emitindo: ");
        Serial.print(frequencias[i]);
        Serial.println(" Hz");

        Serial.print("Duracao: ");
        Serial.print(tempos[i]);
        Serial.println(" segundos");

        Serial.println();


        // LIGA O BUZZER

        tone(BUZZER_PIN, frequencias[i]);


        // CONTROLA O TEMPO SEM TRAVAR

        unsigned long duracao =
            (unsigned long)tempos[i] * 1000UL;

        unsigned long inicio = millis();


        while (millis() - inicio < duracao)
        {
            // Verifica o comando P a cada 5 ms

            if (verificarParada())
            {
                return;
            }

            delay(5);
        }


        // DESLIGA O BUZZER

        noTone(BUZZER_PIN);

        Serial.println("Frequencia finalizada.");
        Serial.println();


        // INTERVALO DE 100 ms

        for (int t = 0; t < 100; t += 5)
        {
            if (verificarParada())
            {
                return;
            }

            delay(5);
        }
    }


    noTone(BUZZER_PIN);
}