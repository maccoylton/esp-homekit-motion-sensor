/*
 * Example of using esp-homekit library to control
 * moition sensor using an Hs501 sensor
 * This uses an OTA mechanism created by HomeACcessoryKid
 *
 */

#define DEVICE_MANUFACTURER "David B Brown"
#define DEVICE_NAME "Motion-Sensor"
#define DEVICE_MODEL "Basic"
#define DEVICE_SERIAL "12345678"
#define FW_VERSION "1.0"
#define MOTION_SENSOR_GPIO 12
#define LED_GPIO 2
#define MAX_NAME_LENGTH 63


#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <check_wifi.h>
#include <led_codes.h>
#include <http_post.h>

// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include <ota-api.h>
char accessory_name[MAX_NAME_LENGTH+1];


homekit_characteristic_t ota_trigger      = API_OTA_TRIGGER;
homekit_characteristic_t name             = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer     = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial           = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model            = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision         = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);
homekit_characteristic_t motion_detected  = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);

TaskHandle_t http_post_task_handle;
//TaskHandle_t https_post_wolfssl_task_handle;

void identify_task(void *_args) {
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("identify\n\n");
    led_code(LED_GPIO, IDENTIFY_ACCESSORY);
    xTaskCreate(identify_task, "identify", 128, NULL, 2, NULL);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Motion Sensor"),
            &motion_detected,
            &ota_trigger,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void motion_sensor_callback(uint8_t gpio) {


    if (gpio == MOTION_SENSOR_GPIO){
        int new = 0;
        new = gpio_read(MOTION_SENSOR_GPIO);
        motion_detected.value = HOMEKIT_BOOL(new);
        homekit_characteristic_notify(&motion_detected, HOMEKIT_BOOL(new));
        if (new == 1) {
		printf("Motion Detected on %d\n", gpio);
		snprintf (post_string, 150, "sql=insert into homekit.motionsensorlog (MotionSensorName, MotionDetectionState) values ('%s', 1)", accessory_name);
		vTaskResume( http_post_task_handle );
        } else {
        printf("Motion Stopped on %d\n", gpio);
        snprintf (post_string, 150, "sql=insert into homekit.motionsensorlog (MotionSensorName, MotionDetectionState) values ('%s', 0)", accessory_name);
	vTaskResume( http_post_task_handle );

//        vTaskResume( https_post_wolfssl_task_handle );
	}
    }
    else {
        printf("Interrupt on %d", gpio);
    }

}



void create_accessory_name() {

    int serialLength = snprintf(NULL, 0, "%d", sdk_system_get_chip_id());

    char *serialNumberValue = malloc(serialLength + 1);

    snprintf(serialNumberValue, serialLength + 1, "%d", sdk_system_get_chip_id());
    
    int name_len = snprintf(NULL, 0, "%s-%s-%s",
				DEVICE_NAME,
				DEVICE_MODEL,
				serialNumberValue);

    if (name_len > MAX_NAME_LENGTH) {
        name_len = MAX_NAME_LENGTH;
    }

    char *name_value = malloc(name_len + 1);

    snprintf(name_value, name_len + 1, "%s-%s-%s",
		 DEVICE_NAME, DEVICE_MODEL, serialNumberValue);

    strcpy (accessory_name, name_value);   

    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}


void gpio_init() {

    gpio_enable(MOTION_SENSOR_GPIO, GPIO_INPUT);
    gpio_set_pullup(MOTION_SENSOR_GPIO, false, false);
    gpio_set_interrupt(MOTION_SENSOR_GPIO, GPIO_INTTYPE_EDGE_ANY, motion_sensor_callback);
}


void user_init(void) {
    uart_set_baud(0, 115200);

    gpio_init();

    create_accessory_name(); 


    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    if (c_hash==0) c_hash=1;
        config.accessories[0]->config_number=c_hash;

    homekit_server_init(&config);

    xTaskCreate(checkWifiTask, "check_wifi_connection_task", 256, NULL, 2, NULL);
    xTaskCreate(http_post_task, "http post task", 512, NULL, 2, &http_post_task_handle);
//    xTaskCreate(https_post_wolfssl_task, "https post wolfssl task", 2048, NULL, 2, &https_post_wolfssl_task_handle);


}
