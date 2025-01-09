#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "parson.h"
#include "parson.c"

// define a estrutura para alocação de memória a ser utilizada na função de callback
struct MemoryStruct {
    JSON_Value *json_value; // Valor JSON recebido
    JSON_Status status;     // Status do JSON
};

void mostrarTempo(FILE *fptr){
    int hora,min,dia,mes,ano;
    time_t now;
    time(&now);
    struct tm *local=localtime(&now);
    hora=local->tm_hour;
    min=local->tm_min;
    dia=local->tm_mday;
    mes=local->tm_mon+1;
    ano=local->tm_year+1900;
    if (hora < 12) {    // antes do meio-dia
        fprintf(fptr, "%02d:%02d am ", hora, min);
    }
    else {    // depois do meio-dia
        fprintf(fptr, "%02d:%02d pm ", hora - 12, min);
    }
    fprintf(fptr, "%02d/%02d/%d - ", dia, mes, ano);
}

// função de callback para lidar com a resposta JSON da API
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    // Parse do JSON aqui
    JSON_Value *received_value = json_parse_string_with_comments((const char *)contents);
    if (received_value == NULL) {
        fprintf(stderr, "Erro ao analisar JSON\n");
        return 0; // Retorna 0 para sinalizar erro
    }

    // Se for o primeiro JSON, inicialize
    if (mem->json_value == NULL) {
        mem->json_value = json_value_init_array(); // Inicializa array no Parson
        if (mem->json_value == NULL) {
            json_value_free(received_value);
            return 0; // Retorna 0 para sinalizar erro
        }
    }

    JSON_Array *json_array = json_value_get_array(mem->json_value);
    json_array_append_value(json_array, received_value);

    mem->status = JSONSuccess;

    return realsize;
}

// função para realizar a pergunta (requisição API)
int realizarQuestao(const char *url, const char *api_key, const char *pergunta, const char *model, char **resposta) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Falha ao inicializar curl\n");
        return -1;
    }
    FILE *fptr2;
    fptr2=fopen("pergunta_reposta.CSV","a+");

    struct curl_slist *headers = NULL;
    struct MemoryStruct chunk;
    chunk.json_value = NULL;
    chunk.status = JSONFailure;

    // cria payload JSON usando Parson
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *json_object = json_value_get_object(root_value);
    json_object_set_string(json_object, "model", model);
    json_object_set_number(json_object, "max_tokens", 100);
    json_object_set_number(json_object, "temperature", 0.7);

    // cria array para a pergunta
    JSON_Value *array_value = json_value_init_array();
    JSON_Array *array = json_value_get_array(array_value);
    JSON_Value *item_value1 = json_value_init_object();
    JSON_Object *item_object1 = json_value_get_object(item_value1);
    json_object_set_string(item_object1, "role", "user");
    json_object_set_string(item_object1, "content", pergunta);
    json_array_append_value(array, item_value1);
    json_object_set_value(json_object, "messages", array_value);

    char *json_payload = json_serialize_to_string_pretty(root_value);
    json_value_free(root_value); // Libera memória

    curl_global_init(CURL_GLOBAL_ALL);

    headers = curl_slist_append(headers, "Content-Type: application/json");
    char auth_header[100];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
    headers = curl_slist_append(headers, auth_header);

    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        fprintf(stderr, "Requisição HTTP não concluída, erro: %ld\n", http_code);
        mostrarTempo(fptr2);
        fprintf(fptr2, "Requisição HTTP não concluída, erro: %ld\n", http_code);
        json_value_free(chunk.json_value);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }

    if (res != CURLE_OK) {
        fprintf(stderr, "Requisição HTTP não foi concluída: %s\n", curl_easy_strerror(res));
        mostrarTempo(fptr2);
        fprintf(fptr2, "Requisição HTTP não foi concluída: %s\n", curl_easy_strerror(res));
        json_value_free(chunk.json_value);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }

    // processa a resposta JSON
    if (chunk.status == JSONSuccess) {
        JSON_Array *response_array = json_value_get_array(chunk.json_value);
        if (response_array == NULL) {
            fprintf(stderr, "Arquivo JSON não foi recebido.\n");
            mostrarTempo(fptr2);
            fprintf(fptr2, "Arquivo JSON não foi recebido.\n");
            json_value_free(chunk.json_value);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return -1;
        }

        JSON_Object *response_obj = json_array_get_object(response_array, 0); // Supondo apenas uma resposta
        JSON_Array *choices_array = json_object_get_array(response_obj, "choices");
        if (choices_array == NULL) {
            fprintf(stderr, "Erro no JSON recebido da API.\n");
            mostrarTempo(fptr2);
            fprintf(fptr2, "Erro no JSON recebido da API.\n");
            json_value_free(chunk.json_value);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return -1;
        }

        JSON_Object *message_obj = json_array_get_object(choices_array, 0);
        const char *content = json_object_get_string(json_object_get_object(message_obj, "message"), "content");

        // aloca memória para 'resposta' e copia 'content' para ela
        *resposta = (char *)malloc(strlen(content) + 1);
        if (*resposta == NULL) {
            fprintf(stderr, "Erro de alocação de memória.\n");
            mostrarTempo(fptr2);
            fprintf(fptr2, "Erro de alocação de memória.\n");
            json_value_free(chunk.json_value);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return -1;
        }
        memcpy(*resposta, content, strlen(content) + 1);
    } else {
        fprintf(stderr, "Não foi possível processar a resposta do servidor.\n");
        mostrarTempo(fptr2);
        fprintf(fptr2, "Não foi possível processar a resposta do servidor.\n");
        json_value_free(chunk.json_value);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return -1;
    }
	
    // libera a estrutura JSON
    json_value_free(chunk.json_value);

    // limpeza
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    fclose(fptr2);

    return 0;
}

void menu() {
    printf("Bem-vindo(a) ao Programa de Perguntas e Respostas!");
    printf("\nEm que posso ajudar hoje?");
    printf("\n1. Fazer uma pergunta;");
    printf("\n2. Sair.");
    printf("\nDigite alguma opcao do menu: ");
}

void clear_terminal() {
    #ifdef _WIN32
        system("cls"); // Comando para limpar tela no Windows
    #else
        system("clear"); // Comando para limpar tela no Linux/Unix
    #endif
}

int main() {
    menu();
    int opcao;

    char pergunta[256];
    char *resposta = NULL; // Declara resposta aqui
    
    do {
        scanf("%d", &opcao);
        getchar(); // Limpa o caractere de nova linha deixado pelo scanf
        printf("\n");
        
        switch (opcao){
            case 1:
                const char *url = "https://api.openai.com/v1/chat/completions";
                const char *api_key = "colocar chave api aqui";
                const char *model = "gpt-3.5-turbo";
                curl_global_init(CURL_GLOBAL_ALL);

                printf("Pergunta:\n");
                fgets(pergunta, sizeof(pergunta), stdin);

                // Remove o caractere de nova linha, se presente
                size_t len = strlen(pergunta);
                if (pergunta[len - 1] == '\n') {
                    pergunta[len - 1] = '\0';
                }

                int result = realizarQuestao(url, api_key, pergunta, model, &resposta);
                if (result == 0) {
                    printf("Resposta do servidor:\n%s\n", resposta);
                }

                FILE *fptr;
                fptr=fopen("pergunta_reposta.CSV","a+");
                mostrarTempo(fptr);
                fprintf(fptr,"Usuário: %s, \n",pergunta);
                mostrarTempo(fptr);
                fprintf(fptr,"LLM: %s, \n", resposta);
                fclose(fptr);

                // Libera memória alocada para 'resposta'
                free(resposta);

                curl_global_cleanup();

                // Aguarda o usuário pressionar Enter para continuar
                printf("\nPressione Enter para continuar...");
                getchar(); // Captura o Enter pressionado

                clear_terminal(); // Limpa o terminal antes de exibir o menu novamente
                menu(); // Exibe o menu novamente
            
            case 2:
                break;
            
            default:
                printf("Opcao invalida, tente novamente.\n\n");
                printf("Digite alguma opcao do menu: ");
                break;
        }
        
    } while (opcao != 2);

    return 0;
}
