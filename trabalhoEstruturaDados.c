#include <stdio.h>
#include <curl/curl.h>
#include "parson.h"
#include "parson.c"

int realizarQuestao(const char *url, const char *api_key, const char *pergunta, const char *model){
		CURL *curl = curl_easy_init();
		if (!curl) {
			fprintf(stderr, "Failed to initialize curl\n");
			return -1;
		}

		int resultado = 0;
		struct curl_slist *headers = NULL;

		// Construct the JSON payload dynamically
		JSON_Value *root_value = json_value_init_object();
		JSON_Object *json_object = json_value_get_object(root_value);
		json_object_set_string(json_object, "prompt", pergunta);
		json_object_set_string(json_object, "model", model);
		json_object_set_number(json_object, "max_tokens", 100);
		json_object_set_number(json_object, "temperature", 0.7);
			// Create the array of JSON objects
		JSON_Value *array_value = json_value_init_array();
		JSON_Array *array = json_value_get_array(array_value);

		// Create first JSON object
		JSON_Value *item_value1 = json_value_init_object();
		JSON_Object *item_object1 = json_value_get_object(item_value1);
		json_object_set_string(item_object1, "role", "assistant");
		json_object_set_string(item_object1, "content", "Say this is a test!");
		json_array_append_value(array, item_value1);

		// You can add more objects similarly if needed

		// Add the array to the main JSON object
		json_object_set_value(json_object, "messages", array_value);

		
		char *json_payload = json_serialize_to_string_pretty(root_value);
		json_value_free(root_value); // Free the JSON_Value structure
		
		curl_global_init(CURL_GLOBAL_ALL);

		headers = curl_slist_append(headers, "Content-Type: application/json");
		char auth_header[100];
		snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", api_key);
		headers = curl_slist_append(headers, auth_header);

		CURLcode res;
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);

		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			fprintf(stderr, "Erro ao enviar solicitação: %s\n", curl_easy_strerror(res));
			resultado = -1;
		}

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		curl_global_cleanup();


		return resultado;
}
	



int main()
{

	const char *url = "https://api.openai.com/v1/chat/completions";
    const char *api_key = ""; 
    const char *model = "gpt-3.5-turbo";
	const char *pergunta = NULL;
	char buf[256];
    curl_global_init(CURL_GLOBAL_ALL);
	puts("Pergunta:");
    scanf("%s", buf);

	//CHAMAR FUNCAO PARA FAZER PERGUNTA
	realizarQuestao(url, api_key, pergunta, model);
	curl_global_cleanup();

	
	////SALVAR NO ARQUIVO CSV AQUI.
    //FILE *fptr;
    //fptr=fopen("perguntas_respostas.CSV", "a+");
    //fprintf(fptr,"Pergunta do Usuário: %s ",pergunta);
    //fclose(fptr);
    //return 0;
}
