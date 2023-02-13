/*
 * AWS IoT Kit - Core2 for AWS IoT Kit
 * Cloud Connected M5Stack Earth Moisture Sensor v1.0.1
 * main.c
 * 
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/**
 * @file main.c
 * @brief simple MQTT publish and subscribe for use with AWS IoT Kit reference hardware.
 *
 * This example takes the parameters from the build configuration and establishes a connection to AWS IoT Core over MQTT.
 *
 * Some configuration is required. Visit https://edukit.workshop.aws
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "cJSON.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "core2forAWS.h"

#include "wifi.h"
#include "ui.h"

/* The time between each MQTT message publish in milliseconds */
#define PUBLISH_INTERVAL_MS 3000

/* The time prefix used by the logger. */
static const char *TAG = "MAIN";

/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/* Default MQTT port is pulled from the aws_iot_config.h */
uint32_t port = AWS_IOT_MQTT_PORT;

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) {
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);
}

void disconnect_callback_handler(AWS_IoT_Client *pClient, void *data) {
    ESP_LOGW(TAG, "MQTT Disconnect");
    ui_textarea_add("Disconnected from AWS IoT Core...", NULL, 0);
    IoT_Error_t rc = FAILURE;

    if(pClient == NULL) {
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

/**
 * @brief Function that reads from sensor and publishes to MQTT topic.
 *
 * This function is called by the aws_iot_task at the interval defined by 
 * @ref PUBLISH_INTERVAL_MS. 
 * 
 * The function reads from the connected M5Stack
 * Earth moisture sensor on Port B, and gets the calibrated millivolt 
 * value. It then uses the cJSON library to create a JSON object, which
 * then gets stringified to be sent as an MQTT message.
 * 
 * The sensor value is published to a topic that ends with "sensor." So
 * the complete MQTT topic should look like `0123456A78B9012C34/sensor`
 * with the serial number that matches your device's actual serial
 * number, and as registered with AWS IoT.
 * 
 * It also displays the sensor readings on to the screen, frees the
 * memory used by cJSON to construct the JSON payload, and lastly
 * it displays different colors on the RGB LED bars to indicate if
 * the soil is moist or dry.
 *
*/
static void publisher(AWS_IoT_Client *client, char *base_topic, uint16_t base_topic_len){
    // AWS IoT publishing struct configured for QOS0
    IoT_Publish_Message_Params paramsQOS0;
    paramsQOS0.qos = QOS0;
    paramsQOS0.isRetained = 0;
    
    // Read the sensor value from the ADC
    int moisture_millis = Core2ForAWS_Port_B_ADC_ReadMilliVolts();

    // Create a JSON object using the cJSON library
    // JSON object has keys `moisture_sensor`, a string for the 
    // sensor type, and `moisture_level`, a number, for the sensor 
    // readings.
    cJSON *payload = cJSON_CreateObject();
    cJSON *moisture_sensor = cJSON_CreateString("M5Stack_Earth");
    cJSON *moisture_level = cJSON_CreateNumber(moisture_millis);
    cJSON_AddItemToObject(payload, "moisture_sensor", moisture_sensor);
    cJSON_AddItemToObject(payload, "moisture_level", moisture_level);

    // Stringify the JSON object to be sent over MQTT
    // Add the string to the QOS0 payload
    const char *JSONPayload = cJSON_Print(payload);
    paramsQOS0.payload = (void *) JSONPayload;
    paramsQOS0.payloadLen = strlen(JSONPayload);

    // As a best practice, narrow the topic to be more easily digested
    // Here we append "sensor" to the base topic name.
    char *mqtt_pub_topic = "sensor";
    uint8_t mqtt_pub_topic_len = strlen( mqtt_pub_topic );
    uint8_t publish_topic_len = base_topic_len + mqtt_pub_topic_len;
    char publish_topic[ publish_topic_len ];
    snprintf( publish_topic, publish_topic_len, "%s%s", base_topic, mqtt_pub_topic );

    // Publish the message to AWS IoT with the topic specified above
    IoT_Error_t rc = aws_iot_mqtt_publish(client, publish_topic, publish_topic_len, &paramsQOS0);
    if (rc != SUCCESS){
        ESP_LOGE(TAG, "Publish QOS0 error %i", rc);
        rc = SUCCESS;
    }

    // Print the payload string to the screen
    ui_textarea_add("%s", (char *)JSONPayload, paramsQOS0.payloadLen);

    // Delete the JSON object and free dynamically allocated memory
    // This is critical to avoid a memory leak
    cJSON_Delete(payload);

    // Change the color of the LEDS based on sensor mv value
    if(moisture_millis < 2700){ 
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0x00ff00);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0x00ff00);
        Core2ForAWS_Sk6812_Show();
    }
    else{
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0xff0000);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0xff0000);
        Core2ForAWS_Sk6812_Show();
    }
}

void aws_iot_task(void *param) {
    IoT_Error_t rc = FAILURE;

    AWS_IoT_Client client;
    IoT_Client_Init_Params mqttInitParams = iotClientInitParamsDefault;
    IoT_Client_Connect_Params connectParams = iotClientConnectParamsDefault;

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    mqttInitParams.enableAutoReconnect = false; // We enable this later below
    mqttInitParams.pHostURL = HostAddress;
    mqttInitParams.port = port;    
    mqttInitParams.pRootCALocation = (const char *)aws_root_ca_pem_start;
    mqttInitParams.pDeviceCertLocation = "#";
    mqttInitParams.pDevicePrivateKeyLocation = "#0";
    
#define CLIENT_ID_LEN (ATCA_SERIAL_NUM_SIZE * 2)
#define SUBSCRIBE_TOPIC_LEN (CLIENT_ID_LEN + 3)
#define BASE_PUBLISH_TOPIC_LEN (CLIENT_ID_LEN + 2)

    char *client_id = malloc(CLIENT_ID_LEN + 1);
    ATCA_STATUS ret = Atecc608_GetSerialString(client_id);
    if (ret != ATCA_SUCCESS)
    {
        printf("Failed to get device serial from secure element. Error: %i", ret);
        abort();
    }

    char subscribe_topic[SUBSCRIBE_TOPIC_LEN];
    char base_publish_topic[BASE_PUBLISH_TOPIC_LEN];
    snprintf(subscribe_topic, SUBSCRIBE_TOPIC_LEN, "%s/#", client_id);
    snprintf(base_publish_topic, BASE_PUBLISH_TOPIC_LEN, "%s/", client_id);

    mqttInitParams.mqttCommandTimeout_ms = 20000;
    mqttInitParams.tlsHandshakeTimeout_ms = 5000;
    mqttInitParams.isSSLHostnameVerify = true;
    mqttInitParams.disconnectHandler = disconnect_callback_handler;
    mqttInitParams.disconnectHandlerData = NULL;

    rc = aws_iot_mqtt_init(&client, &mqttInitParams);
    if(SUCCESS != rc) {
        ESP_LOGE(TAG, "aws_iot_mqtt_init returned error : %d ", rc);
        abort();
    }

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);    

    connectParams.keepAliveIntervalInSec = 10;
    connectParams.isCleanSession = true;
    connectParams.MQTTVersion = MQTT_3_1_1;

    connectParams.pClientID = client_id;
    connectParams.clientIDLen = CLIENT_ID_LEN;
    connectParams.isWillMsgPresent = false;
    ui_textarea_add("Connecting to AWS IoT Core...\n", NULL, 0);
    ESP_LOGI(TAG, "Connecting to AWS IoT Core at %s:%d", mqttInitParams.pHostURL, mqttInitParams.port);
    do {
        rc = aws_iot_mqtt_connect(&client, &connectParams);
        if(SUCCESS != rc) {
            ESP_LOGE(TAG, "Error(%d) connecting to %s:%d", rc, mqttInitParams.pHostURL, mqttInitParams.port);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } while(SUCCESS != rc);
    ui_textarea_add("Successfully connected!\n", NULL, 0);
    ESP_LOGI(TAG, "Successfully connected to AWS IoT Core!");

    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time for exponential backoff for retries.
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_mqtt_autoreconnect_set_status(&client, true);
    if(SUCCESS != rc) {
        ui_textarea_add("Unable to set Auto Reconnect to true\n", NULL, 0);
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d", rc);
        abort();
    }

    ESP_LOGI(TAG, "Subscribing to '%s'", subscribe_topic);
    rc = aws_iot_mqtt_subscribe(&client, subscribe_topic, strlen(subscribe_topic), QOS0, iot_subscribe_callback_handler, NULL);
    if(SUCCESS != rc) {
        ui_textarea_add("Error subscribing\n", NULL, 0);
        ESP_LOGE(TAG, "Error subscribing : %d ", rc);
        abort();
    } else{
        ui_textarea_add("Subscribed to topic: %s\n\n", subscribe_topic, SUBSCRIBE_TOPIC_LEN) ;
        ESP_LOGI(TAG, "Subscribed to topic '%s'", subscribe_topic);
    }
    
    ESP_LOGI(TAG, "\n****************************************\n*  AWS client Id - %s  *\n****************************************\n\n",
             client_id);
    
    ui_textarea_add("Publishing to topic: %s\n", base_publish_topic, BASE_PUBLISH_TOPIC_LEN) ;
    while((NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)) {

        //Max time the yield function will wait for read messages
        rc = aws_iot_mqtt_yield(&client, 100);
        if(NETWORK_ATTEMPTING_RECONNECT == rc) {
            // If the client is attempting to reconnect we will skip the rest of the loop.
            continue;
        }

        ESP_LOGD(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
        vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
        
        publisher(&client, base_publish_topic, BASE_PUBLISH_TOPIC_LEN);
    }

    ESP_LOGE(TAG, "An error occurred in the main loop.");
    abort();
}

void app_main()
{
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);
    Core2ForAWS_Port_PinMode(PORT_B_ADC_PIN, ADC);
    
    ui_init();
    initialise_wifi();

    xTaskCreatePinnedToCore( &aws_iot_task, "aws_iot_task", 4096 * 2, NULL, 5, NULL, 1 );
}