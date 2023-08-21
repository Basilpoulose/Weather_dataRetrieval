
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_tls.h"


#include "jsmn.h"
#include "esp_http_client.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 1024
static const char *TAG = "HTTP_CLIENT";
// for usart
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
static const int RX_BUF_SIZE = 1024;
//jsmn string
char *JSON_STRING;
int jsm_parsing();
char final_data[70] = {0};
int str_len=0;
char city_data[20];
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}



esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char output_buffer[1024];  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                // if (evt->user_data) {
                //     memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                // } else {
                //     if (output_buffer == NULL) {
                //         output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                //         output_len = 0;
                //         if (output_buffer == NULL) {
                //             ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                //             return ESP_FAIL;
                //         }
                //     }
                //     memcpy(output_buffer + output_len, evt->data, evt->data_len);
                    
                // }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            JSON_STRING = output_buffer;
            printf("jason data: %s\n",JSON_STRING);
            jsm_parsing();
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                //Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                //ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                //printf("There is data in buffer.\n");
                //free(output_buffer);
                //output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                //free(output_buffer);
                //output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}
static void http_rest_with_url(void)
{
    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    esp_http_client_config_t config = {
        .host = "api.openweathermap.org",
        .path = "/data/2.5/weather",
        .query = "lat=12.939905&lon=77.628570&appid=69cf53acc5a0f0e34a411d311e17736d",
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,        // Pass address of local buffer to get response
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // GET
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    

    esp_http_client_cleanup(client);
}
//parsing the data with jsmn
int jsm_parsing()
{
    int i,r;
    jsmn_parser p;
    jsmntok_t t[100];
    char city_name[15],temp_value[8],humidity_value[4] = {0};
    char *kelvin = (char *)malloc(9);
    printf("parsing json now :\n");
    jsmn_init(&p);
    r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t,sizeof(t) / sizeof(t[0]));
    if (r < 0)
    {
        printf("Failed to parse JSON: %d\n", r);
         return 1;    
    }
    //Assume the top-level element is an object
    if (r < 1 || t[0].type != JSMN_OBJECT)
    {
         printf("Object expected\n");
         return 1;
    }
    //Loop over all keys of the root object
    printf("weather data :\n");
    for (i = 1; i < r; i++) 
    {
        if (jsoneq(JSON_STRING, &t[i], "name") == 0) 
        {
            str_len = t[i + 1].end - t[i + 1].start;
            strncpy(city_name,JSON_STRING + t[i + 1].start,str_len);
            city_name[str_len]= '\0';
            printf("%d \n",str_len);
            for(int l=str_len;l<15;l++)
            {
                city_name[l] = '#';
            }
            city_name[15]= '\0';
            printf("city name : %s \n",city_name);
            //printf("- City name: %.*s\n",len1,JSON_STRING + t[i + 1].start);
            i++;
        } else if (jsoneq(JSON_STRING, &t[i], "temp") == 0) {

            str_len = t[i + 1].end - t[i + 1].start;
            strncpy(temp_value,JSON_STRING + t[i + 1].start,str_len);
            int len1=strlen(temp_value);
            printf("%d \n",len1);
            //temp_value[str_len] = '\0';
            for(int m=str_len;m<8;m++)
            {
                temp_value[m] = '#';
            }
            temp_value[9]= '\0';

            snprintf(kelvin, 9, "%s", temp_value);
            printf("Temperature : %s \n",kelvin);
            printf("length of kelvin : %d \n",strlen(kelvin));
            //printf("- Temperature: %.*s\n", len2,JSON_STRING + t[i + 1].start);
            i++;
        } else if (jsoneq(JSON_STRING, &t[i], "humidity") == 0) {

            str_len = t[i + 1].end - t[i + 1].start;
            //humidity_value[str_len+1];
            strncpy(humidity_value,JSON_STRING + t[i + 1].start,str_len); 
            printf("%d \n",str_len);
            humidity_value[str_len] = '\0';
            for(int n=str_len;n<4;n++)
            {
                humidity_value[n] = '#';
            }
            humidity_value[4] = '\0';
            printf("Humidity : %s \n",humidity_value);
            //printf("- Humidity: %.*s\n", len3,JSON_STRING + t[i + 1].start);
            i++;
        }
    }
    int s_len = strlen (city_name) + strlen (kelvin) +  strlen (humidity_value) +1;
    
    strcpy(final_data,city_name);
    strcat(final_data,kelvin);
    strcat(final_data,humidity_value);
    
    final_data[s_len] = '\0';
    printf("final output is : %s \n",final_data);

    /*char *temp = kelvin;
    char *name = city_name;
    char *humd = humidity_value;

    //Calculate the total length of the resulting big string
    int total_length = strlen(city_name) + strlen(kelvin) + strlen(humidity_value)  +1;
   
    // Allocate memory for the big string
    char *big_string = (char *)malloc(total_length + 1); // +1 for the null terminator
    if (big_string == NULL) 
    {
        perror("Memory allocation failed");
         return 1;
    }
    // Concatenate the strings using snprintf
    snprintf(big_string, total_length + 1, "%s%s%s", city_name,kelvin,humidity_value);

    printf("Concatenated string: %s\n", big_string); 

    // Free allocated memory
    //free(big_string);
    //sprintf(final_data, "City: %s, Temperature: %.1f, Humidity: %s", city_name, temp_value, humidity_value);
    printf("final data value %d \n",strlen(big_string));*/

    return 0;
}
// usart config 
void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}
int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        sendData(TX_TASK_TAG, final_data);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
static void rx_task(void *arg)
{
    char city[15];
    char tempe[8];
    char humi[5];
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            //ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
            for(int i=0;i<rxBytes;i++)
            {
                if(data[i] !='#')
                {
                    city[i]=data[i];
                }
            }
            printf("City name : %s \n",city);
            int len=strlen(city);
            for(int j=len;j<rxBytes;j++)
            {
                if(data[j] !='#')
                {
                    tempe[j]=data[j];
                }
            }
            printf("temperature : %s \n",tempe);
            int len1=strlen(tempe);
            for(int k=len1;k<rxBytes;k++)
            {
                if(data[k] !='#')
                {
                    humi[k]=data[k];
                }
            }
            printf("Humidity : %s \n",humi);

        }
    }
    free(data);
}


static void http_test_task(void *pvParameters)
{
    http_rest_with_url();
    ESP_LOGI(TAG, "Finish http example");
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http example");

    xTaskCreate(&http_test_task, "http_test_task", 8192, NULL, 5, NULL);
    init();
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
}
