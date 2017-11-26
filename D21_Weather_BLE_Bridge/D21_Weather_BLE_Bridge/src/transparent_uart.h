#ifndef TRANSPARENT_UART_SERVICE_H_
#define TRANSPARENT_UART_SERVICE_H_
#include "at_ble_api.h"

/****************************************************************************************
*							        Macros	                                     							*
****************************************************************************************/
/**@brief Keypad debounce time */
#define KEY_PAD_DEBOUNCE_TIME	(200)

/**@brief Application maximum buffer size */
#define APP_BUF_SIZE			(150)

/**@brief Enter button press to send data */
#define ENTER_BUTTON_PRESS		(13)

/**@brief Entered backspace button */
#define BACKSPACE_BUTTON_PRESS	(8)

/**@brief Space bar */
#define SPACE_BAR (32)

/**@brief TAB entered */
#define TAB_PRESSED (9)

/** \brief Transparent UART service UUID */
#define TU_SERVICE_UUID				("\x55\xE4\x05\xD2\xAF\x9F\xA9\x8F\xE5\x4A\x7D\xFE\x43\x53\x53\x49")

/** \brief Transparent UART TX characteristic UUID */
#define TU_TX_CHAR_UUID				("\x16\x96\x24\x47\xC6\x23\x61\xBA\xD9\x4B\x4D\x1E\x43\x53\x53\x49")

/** \brief Transparent UART RX characteristic UUID */
#define TU_RX_CHAR_UUID				("\xB3\x9B\x72\x34\xBE\xEC\xD4\xA8\xF4\x43\x41\x88\x43\x53\x53\x49")

/** \brief Transparent UART TCP characteristic UUID */
#define TU_TCP_CHAR_UUID			("\x7e\x3b\x07\xff\x1c\x51\x49\x2f\xb3\x39\x8a\x4c\x43\x53\x53\x49")

#define TOTAL_NUM_OF_TU_CHARATERISTIC	3
#define UUID_128_LEN					16
#define MAX_REMOTE_DEVICE				3

/* Advertisement payload definitions */
#define ADV_DATA_TYPE_SIZE				1
#define ADV_DATA_TYPE_DEV_NAME			0x09
#define ADV_DATA_TYPE_128_COMP_UUID		0x07

#define ADV_PAYLOAD_DEV_NAME_VAL		('S', 't', 'c', 'k', ' ', 'Q', 't', 'e')
#define ADV_PAYLOAD_DEV_NAME_SIZE		8
#define ADV_PAYLOAD_DEV_NAME			(ADV_DATA_TYPE_SIZE + ADV_PAYLOAD_DEV_NAME_SIZE, ADV_DATA_TYPE_DEV_NAME, ADV_PAYLOAD_DEV_NAME_VAL)

#define ADV_PAYLOAD_128_UUID_VAL		(0x55, 0xe4, 0x05, 0xD2, 0xAF, 0x9F, 0xA9, 0x8f, 0xE5, 0x4A, 0x7D, 0xFE, 0x43, 0x53, 0x53, 0x49)
#define ADV_PAYLOAD_128_UUID_SIZE		16
#define ADV_PAYLOAD_128_UUID			(ADV_DATA_TYPE_SIZE + ADV_PAYLOAD_128_UUID_SIZE, ADV_DATA_TYPE_128_COMP_UUID, ADV_PAYLOAD_128_UUID_VAL)	

enum
{
	CHAR_TX,
	CHAR_RX,
	CHAR_TCP,	
};

typedef enum
{
	BLE_APP_INIT,
	BLE_APP_START_ADV,
	BLE_APP_ADV_STARTED,
	BLE_APP_CONNECTED,
	BLE_APP_DISCONNECTED,
	BLE_APP_SYMBOL_RECEIVED,
}ble_app_init_t;

typedef enum
{
//	BLE_APP_STOCK_SYMBOL_NOT_RECEIVED,
	BLE_APP_CITY_NAME_NOT_RECEIVED,
//	BLE_APP_STOCK_SYMBOL_RECEIVED,
	BLE_APP_CITY_NAME_RECEIVED,
//	BLE_APP_STOCK_QUOTE_UNDER_PROCESSING,
	BLE_APP_WEATHER_UNDER_PROCESSING
}ble_app_sq_state_t;

typedef struct  
{
	/* Connection parameters */
	at_ble_connected_t remote_dev_conn_info;
	/* Entry is occupied or not */
	bool entry_flag;
	/* BLE Application state */
	ble_app_sq_state_t sq_state;
	/* Stock symbol received from remote device */
	//char stock_symbol[10];
	char city_name[20];
}remote_dev_info_t;

/****************************************************************************************
*							        Structures                                     		*
****************************************************************************************/
/** @brief Transparent UART service info */
typedef struct transparent_uart_service
{
	at_ble_uuid_t	serv_uuid;
	at_ble_handle_t	serv_handle;
	at_ble_characteristic_t	chars[3];
}transparent_uart_service_t;

/****************************************************************************************
*                                       Functions                                       *
****************************************************************************************/

/** @brief Application initialization
  * 
  * @param
  *
  * @return Upon successful completion the function shall return @ref AT_BLE_SUCCESS,
  * Otherwise the function shall return @ref at_ble_status_t
  */
at_ble_status_t ble_app_init(void);

/** @brief Handle different BLE application states and BLE events
  * 
  * @param
  *
  * @return 
  */
void ble_app_process(void);

/** @brief Tells whether BLE App is in @BLE_APP_INIT or not
  * 
  * @param
  *
  * @return true if BLE App is in BLE_APP_INIT, false otherwise
  */
bool ble_app_is_init_state(void);

/** @brief Get stock symbol sent by remote device
  * 
  * @param
  *
  * @return stock symbol
  */
//char* ble_app_get_stock_symbol(void);
char* ble_app_get_city_name(void);
/** @brief Send stock quote to remote device
  * 
  * @param[in] data	Stock quote
  * @param[in] data_len	Stock quote length
  *
  * @return 
  */
//void ble_app_send_stock_quote(uint8_t *data, uint16_t data_len);
void ble_app_send_weather_data(uint8_t *data, uint16_t data_len);

/** @brief Set BLE application state to start advertisement
  * 
  * @param
  *
  * @return 
  */
void ble_app_state_set_start_adv(void);

#endif //TRANSPARENT_UART_SERVICE_H_