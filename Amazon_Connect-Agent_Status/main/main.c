#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "cJSON.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "core2forAWS.h"

#include "wifi.h"
#include "ui.h"

/* The time prefix used by the logger. */
static const char *TAG = "MAIN";

#define AVAILABLE "AVAILABLE"
#define OFFLINE "OFFLINE00"

#define STARTING_STATUS OFFLINE
#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 1024
#define CLIENT_ID_LEN (ATCA_SERIAL_NUM_SIZE * 2)


/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
char HostAddress[255] = AWS_IOT_MQTT_HOST;
char ledActuatorNameValue[9] = STARTING_STATUS; 

/* Default MQTT port is pulled from the aws_iot_config.h */
uint32_t port = AWS_IOT_MQTT_PORT;

// Handlers and shared global variables
AWS_IoT_Client iotCoreClient;
jsonStruct_t ledActuator;
static char * client_id;
static bool shadowUpdateInProgress;

static void ShadowUpdateStatusCallback(const char *pThingName, 
						ShadowActions_t action, Shadow_Ack_Status_t status, 
						const char *pReceivedJsonDocument, void *pContextData) 
{
	
    shadowUpdateInProgress = false;

	if(SHADOW_ACK_TIMEOUT == status) {
		ESP_LOGI(TAG,"Update Timeout");
	} else if(SHADOW_ACK_REJECTED == status) {
		ESP_LOGI(TAG,"Update Rejected");
	} else if(SHADOW_ACK_ACCEPTED == status) {
		ESP_LOGI(TAG,"Update Accepted");
	}
}

static void ledActuatorClear()
{	
	
    ESP_LOGI(TAG, "Clearing LEDs");
    Core2ForAWS_Sk6812_Clear();
    Core2ForAWS_Sk6812_Show();

}


static void ledActuatorChangeColor(char *  status)
{	
	if(strcmp(status, OFFLINE) == 0) {
        ESP_LOGI(TAG, "Setting LEDs to red");
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0xFF0000);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0xFF0000);
        Core2ForAWS_Sk6812_Show();
    } else if(strcmp(status, AVAILABLE) == 0) {
        ESP_LOGI(TAG, "Setting LEDs to green");
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0x00FF00);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0x00FF00);
        Core2ForAWS_Sk6812_Show();
    } else {
        ESP_LOGI(TAG, "Clearing LEDs");
        Core2ForAWS_Sk6812_Clear();
        Core2ForAWS_Sk6812_Show();
    }

}

static void ShadowGetStatusCallback(const char *pThingName, 
						ShadowActions_t action, Shadow_Ack_Status_t status, 
						const char *pReceivedJsonDocument, void *pContextData) 
{

	if(SHADOW_ACK_TIMEOUT == status) {
		ESP_LOGI(TAG,"Update Timeout");
	} else if(SHADOW_ACK_REJECTED == status) {
		ESP_LOGI(TAG,"Update Rejected");
	} else if(SHADOW_ACK_ACCEPTED == status) {
		
		ESP_LOGI(TAG,"Update Accepted");

		cJSON * root   = cJSON_Parse(pReceivedJsonDocument);
		cJSON * state = cJSON_GetObjectItem(root,"state");
		cJSON * desired = cJSON_GetObjectItem(state,"desired");
		char * status = cJSON_GetObjectItem(desired,"Name")->valuestring;
		
		ledActuatorChangeColor(status);
		memcpy(ledActuatorNameValue, status, 9);
	}
}

static void ledActuator_Callback(const char *pJsonString, 
						uint32_t JsonStringDataLen, jsonStruct_t *pContext) 
{
    shadowUpdateInProgress = true;
	
	char * status = (char *) (pContext->pData);
	if(pContext != NULL) {
        ESP_LOGI(TAG, "Delta - led state changed to %s", status);
	}
	
	ledActuatorChangeColor(status);
	
	char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
	size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

	IoT_Error_t rc = FAILURE;

	rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
	if(SUCCESS != rc) {
		ESP_LOGI(TAG,"aws_iot_shadow_init_json_document Error");
		abort();
	}
	rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 1, &ledActuator);
	if(SUCCESS != rc) {
		ESP_LOGI(TAG,"aws_iot_shadow_add_reported Error");
		abort();
	}
	rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
	if(SUCCESS != rc) {
		ESP_LOGI(TAG,"aws_iot_finalize_json_document Error");
		abort();
	}
	
	ESP_LOGI(TAG,"Update Shadow: %s", JsonDocumentBuffer);

	rc = aws_iot_shadow_update(&iotCoreClient, client_id, JsonDocumentBuffer, ShadowUpdateStatusCallback, NULL, 4, true);
	if(SUCCESS != rc) {
		ESP_LOGI(TAG,"aws_iot_shadow_update Error");
		abort();
	}
		
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) {
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);
}


void disconnect_callback_handler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");
    ui_textarea_add("Disconnected from AWS IoT Core...", NULL, 0);

    IoT_Error_t rc = FAILURE;

    if(NULL == pClient) {
        return;
    }

    if(aws_iot_is_autoreconnect_enabled(pClient)) {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    } else {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if(NETWORK_RECONNECTED == rc) {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        } else {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}




void aws_iot_task(void *param) {

    ESP_LOGI(TAG,"\nAWS IoT SDK Version %d.%d.%d-%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);
	
	ledActuatorClear();

	IoT_Error_t rc = FAILURE;

	ledActuator.cb = ledActuator_Callback;
	ledActuator.pData = &ledActuatorNameValue;
    ledActuator.dataLength = strlen(ledActuatorNameValue)+1;
	ledActuator.pKey = "Name";
	ledActuator.type = SHADOW_JSON_STRING;

	ShadowInitParameters_t sp = ShadowInitParametersDefault;
	sp.pHost = HostAddress;
	sp.port = port;
	sp.enableAutoReconnect = false;
	sp.disconnectHandler = disconnect_callback_handler;

	sp.pRootCA = (const char *)aws_root_ca_pem_start;
	sp.pClientCRT = "#";
	sp.pClientKey = "#0";

	client_id = malloc(CLIENT_ID_LEN + 1);
    ATCA_STATUS ret = Atecc608_GetSerialString(client_id);
    if (ret != ATCA_SUCCESS){
        ESP_LOGE(TAG, "Failed to get device serial from secure element. Error: %i", ret);
        abort();
    }

	ui_textarea_add("\n\nDevice client Id:\n>> %s <<\n", client_id, CLIENT_ID_LEN);
	
	/* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,false, true, portMAX_DELAY); 

	ESP_LOGI(TAG,"Shadow Init");

	rc = aws_iot_shadow_init(&iotCoreClient, &sp);
	if(SUCCESS != rc) {
		ESP_LOGE(TAG,"Shadow Connection Error");
		abort();
	}

	ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
	scp.pMyThingName = client_id;
	scp.pMqttClientId = client_id;
	scp.mqttClientIdLen = (uint16_t) strlen(client_id);

	ESP_LOGI(TAG,"Shadow Connect");
	rc = aws_iot_shadow_connect(&iotCoreClient, &scp);
	if(SUCCESS != rc) {
		ESP_LOGI(TAG,"Shadow Connection Error");
		abort();
	}

	ui_textarea_add("Connected to AWS IoT Device Shadow service", NULL, 0);

    rc = aws_iot_shadow_set_autoreconnect_status(&iotCoreClient, true);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d, aborting...", rc);
        abort();
    }

	rc = aws_iot_shadow_register_delta(&iotCoreClient, &ledActuator);
	if(SUCCESS != rc) {
		ESP_LOGE(TAG,"Shadow Register Delta Error");
		abort();
	}
	
	rc = aws_iot_shadow_get(&iotCoreClient,client_id,ShadowGetStatusCallback, NULL, 4, true);
	if(SUCCESS != rc) {
		ESP_LOGE(TAG,"Shadow Get Error");
		abort();
	}
	
	while(NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc) {
		
		ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "ledActuatorNameValue: %s", ledActuatorNameValue);
		ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
		
		rc = aws_iot_shadow_yield(&iotCoreClient, 200);
		if(NETWORK_ATTEMPTING_RECONNECT == rc || shadowUpdateInProgress) {
            rc = aws_iot_shadow_yield(&iotCoreClient, 100);
			continue;
        }
		vTaskDelay(pdMS_TO_TICKS(1000));
				
	}

	if(SUCCESS != rc) {
		ESP_LOGE(TAG, "An error occurred in the loop %d", rc);
	}
	
	rc = aws_iot_shadow_disconnect(&iotCoreClient);
	if(SUCCESS != rc) {
		ESP_LOGE(TAG, "Disconnect error %d", rc);
	}

	vTaskDelete(NULL);
}
    


void app_main()
{
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);

	Core2ForAWS_LED_Enable(1);
	Core2ForAWS_Sk6812_Clear();
	Core2ForAWS_Sk6812_Show();
    
    ui_init();
    initialise_wifi();

    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 4096 * 3, NULL, 5, NULL, 1);

}