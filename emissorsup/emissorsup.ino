/*
=========================================================
 REC-WAVE
 Transmissão de Texto por Frequências

 Autor: Tony Bonilha Vieira

 Fluxo:

 Texto
 ↓
 ASCII
 ↓
 Binário
 ↓
 Grupos de 3 bits
 ↓
 Frequências
 ↓
 Buzzer


 Protocolo:

 000-111  = Dados (grupos de 3 bits)

 3400 Hz  = Bit final 0
 3700 Hz  = Bit final 1

 4000 Hz  = Controle INICIO/FIM

=========================================================
*/


const int BUZZER_PIN = 15;


//------------------------------------------------------
// Frequências especiais
//------------------------------------------------------

const int FREQUENCIA_CONTROLE = 4000;

const int FREQUENCIA_BIT_0 = 3400;

const int FREQUENCIA_BIT_1 = 3700;



//------------------------------------------------------
// Tabela de frequências dos grupos de 3 bits
//------------------------------------------------------

int obterFrequencia(String bits)
{
    if(bits=="000") return 1000;
    if(bits=="001") return 1300;
    if(bits=="010") return 1600;
    if(bits=="011") return 1900;
    if(bits=="100") return 2200;
    if(bits=="101") return 2500;
    if(bits=="110") return 2800;
    if(bits=="111") return 3100;

    return -1;
}



//------------------------------------------------------
// Emite um bip
//------------------------------------------------------

void emitirBip(int frequencia, int tempo)
{
    tone(BUZZER_PIN, frequencia);

    delay(tempo);

    noTone(BUZZER_PIN);

    delay(100);
}





void setup()
{
    Serial.begin(115200);

    delay(3000);


    Serial.println();
    Serial.println("======================================");
    Serial.println("REC-WAVE");
    Serial.println("Transmissao de Texto");
    Serial.println("======================================");
    Serial.println();

    Serial.println("Digite um texto:");
}






void loop()
{

    if(Serial.available())
    {


        //--------------------------------------------------
        // Lê texto
        //--------------------------------------------------

        String texto = Serial.readStringUntil('\n');

        texto.trim();



        Serial.println();

        Serial.print("Texto recebido: ");

        Serial.println(texto);





        //--------------------------------------------------
        // Conversão para binário
        //--------------------------------------------------

        String binario="";



        for(int i=0;i<texto.length();i++)
        {

            char c = texto[i];


            for(int b=7;b>=0;b--)
            {

                if(c&(1<<b))
                    binario += "1";
                else
                    binario += "0";

            }

        }



        Serial.println();

        Serial.print("Binario: ");

        Serial.println(binario);





        //--------------------------------------------------
        // INÍCIO
        //--------------------------------------------------

        Serial.println();

        Serial.println("Enviando CONTROLE INICIO");


        emitirBip(FREQUENCIA_CONTROLE,500);





        //--------------------------------------------------
        // Transmissão dos grupos de 3 bits
        //--------------------------------------------------

        Serial.println();

        Serial.println("Transmitindo dados...");



        int tamanho = binario.length();


        int i = 0;



        while(i + 3 <= tamanho)
        {

            String grupo = binario.substring(i,i+3);


            int freq = obterFrequencia(grupo);



            Serial.print(grupo);

            Serial.print(" -> ");

            Serial.print(freq);

            Serial.println(" Hz");



            emitirBip(freq,300);



            i += 3;

        }





        //--------------------------------------------------
        // Bits restantes
        //--------------------------------------------------

        int resto = tamanho - i;



        if(resto > 0)
        {

            Serial.println();

            Serial.println("Bits finais encontrados:");



            while(i < tamanho)
            {

                char bit = binario[i];


                if(bit == '0')
                {

                    Serial.println("BIT FINAL 0 -> 3400 Hz");


                    emitirBip(FREQUENCIA_BIT_0,300);

                }


                else
                {

                    Serial.println("BIT FINAL 1 -> 3700 Hz");


                    emitirBip(FREQUENCIA_BIT_1,300);

                }



                i++;

            }

        }





        //--------------------------------------------------
        // FIM
        //--------------------------------------------------

        Serial.println();

        Serial.println("Enviando CONTROLE FIM");


        emitirBip(FREQUENCIA_CONTROLE,500);





        Serial.println();

        Serial.println("Fim da transmissao.");

        Serial.println();

        Serial.println("Digite outro texto:");

    }

}