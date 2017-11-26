#include <asf.h>
#include "at_ble_api.h"
#include "ble_manager.h"
#include "ble_utils.h"
#include "transparent_uart.h"


/* Transparent UART service */
static transparent_uart_service_t  transparent_uart;

/* Buffer data to be send over the air */
static uint8_t send_data[APP_BUF_SIZE];

/* Remote device connection info */
static remote_dev_info_t remote_dev_info[MAX_REMOTE_DEVICE] = {0};

/* BLE Application state */
static ble_app_init_t ble_app_state = BLE_APP_INIT;

static bool is_ble_advertising = false;

/* Stock symbol */
//static char stock_symbol[10];
static char city_name[20];


/* Function Prototype */
static at_ble_status_t ble_app_connected_event(void *param);
static at_ble_status_t ble_app_disconnected_event(void *param);
static at_ble_status_t ble_app_char_changed_event(void *param);
static at_ble_status_t ble_app_noti_confirmed_event(void *param);
static at_ble_status_t ble_app_start_adv(void);
static at_ble_status_t ble_app_tu_primary_service_define(transparent_uart_service_t *tu_serv);
static at_ble_status_t ble_app_tu_serv_init(uint8_t *buf, uint16_t len);
static at_ble_status_t ble_app_tu_serv_send_data(uint16_t connhandle, uint8_t *databuf, uint16_t datalen);
static void ble_app_state_set(ble_app_init_t state);

/* Request stock quote from internet */
//extern void request_stock_quote(char *symbol);
extern void request_weather(char* symbol);

/* GAP event callback list */
const ble_gap_event_cb_t app_ble_gap_event = {
	.connected = ble_app_connected_event,
	.disconnected = ble_app_disconnected_event,
};

/* GATT Server event callback list */
const ble_gatt_server_event_cb_t app_ble_gatt_server_event = {
	.notification_confirmed = ble_app_noti_confirmed_event,
	.characteristic_changed = ble_app_char_changed_event,
};

/* Callback registered for AT_BLE_CONNECTED event from stack */
static at_ble_status_t ble_app_connected_event(void *param)
{
	at_ble_connected_t *conn_param = (at_ble_connected_t *) param;
	bool disconnect = true;
	
	if(AT_BLE_SUCCESS == conn_param->conn_status)
	{
		/* Find an empty space for new device */
		for(uint8_t conn_index = 0; conn_index < MAX_REMOTE_DEVICE; conn_index++)
		{
			if(!remote_dev_info[conn_index].entry_flag)
			{
				/* Copy connection parameters of remote device */
				memcpy(&remote_dev_info[conn_index].remote_dev_conn_info, conn_param, sizeof(at_ble_connected_t));
				/* mark this entry as used */
				remote_dev_info[conn_index].entry_flag = true;
				/* There is enough space to accommodate this device */
				disconnect = false;
				/* Start advertisement again */
				is_ble_advertising = false;
				
				/* If there is no connection exist before, mark it as connected now.
					So that it can process user data */
				if(ble_app_state < BLE_APP_CONNECTED)
				{
					ble_app_state = BLE_APP_CONNECTED;
				}
				break;
			}
		}
		
		if(disconnect)
		{
			/* Since there is not enough space to accommodate this device, disconnect it */
			at_ble_disconnect(conn_param->handle, AT_BLE_REMOTE_DEV_TERM_LOW_RESOURCES);
		}
	}
	
	return AT_BLE_SUCCESS;
}

/* Callback registered for AT_BLE_DISCONNECTED event from stack */
static at_ble_status_t ble_app_disconnected_event(void *param)
{
	at_ble_disconnected_t *disconnected = (at_ble_disconnected_t *) param;
	
	if(disconnected->status == AT_BLE_SUCCESS)
	{
		LED_Off(LED0);
		for(uint8_t conn_index = 0; conn_index < MAX_REMOTE_DEVICE; conn_index++)
		{
			if(remote_dev_info[conn_index].remote_dev_conn_info.handle == disconnected->handle)
			{
				memset(&remote_dev_info[conn_index], 0, sizeof(remote_dev_info_t));
				ble_app_state = BLE_APP_DISCONNECTED;
				break;
			}
		}
	}
	
	ALL_UNUSED(param);
	return AT_BLE_SUCCESS;
}

static at_ble_status_t ble_app_char_changed_event(void *param)
{
	at_ble_characteristic_changed_t *char_data = (at_ble_characteristic_changed_t *)param;
	uint16_t index;
	
	if(char_data->status == AT_BLE_SUCCESS)
	{
		if((char_data->char_len == 2) && ((uint16_t)*char_data->char_new_value == 0x0001))
		{
			/* Notification enabled */
			/* The phone Apps should enable notifications (mandatory). So not keep tracking, who is enabled and who is not */
			return AT_BLE_SUCCESS;		
		}
		else
		{
			for(uint8_t conn_index = 0; conn_index < MAX_REMOTE_DEVICE; conn_index++)
			{
				if((remote_dev_info[conn_index].remote_dev_conn_info.handle == char_data->conn_handle) && (remote_dev_info[conn_index].entry_flag))
				{
					for(index = 0; index < char_data->char_len; index++)
					{
						//DBG_LOG_CONT("%c",char_data->char_new_value[index]);
//						remote_dev_info[conn_index].stock_symbol[index] = (char)char_data->char_new_value[index];
						remote_dev_info[conn_index].city_name[index] = (char)char_data->char_new_value[index];
					}
//					remote_dev_info[conn_index].stock_symbol[index] = '\0';
//					remote_dev_info[conn_index].sq_state = BLE_APP_STOCK_SYMBOL_RECEIVED;
					remote_dev_info[conn_index].city_name[index] = '\0';
					remote_dev_info[conn_index].sq_state = BLE_APP_CITY_NAME_RECEIVED;
					ble_app_state = BLE_APP_SYMBOL_RECEIVED;
					break;
				}
			}
		}
	}

	
	return AT_BLE_SUCCESS;
}

/**
* \ Notification confirmation event
*/
static at_ble_status_t ble_app_noti_confirmed_event(void *param)
{
	at_ble_cmd_complete_event_t *noti_cmpl = (at_ble_cmd_complete_event_t *)param;
	
	if (noti_cmpl->status != AT_BLE_SUCCESS)
	{
		DBG_LOG("Sending Notification over the air failed");
	}
	return AT_BLE_SUCCESS;
}

/**
* \ Initialize and start advertisement
*/
static at_ble_status_t ble_app_start_adv(void)
{
	/*	ADV_PAYLOAD_128_UUID : 0x11, 0x07, 0x55, 0xe4, 0x05, 0xD2, 0xAF, 0x9F, 0xA9, 0x8f, 0xE5, 0x4A, 0x7D, 0xFE, 0x43, 0x53, 0x53, 0x49,
		ADV_PAYLOAD_DEV_NAME : 0x09, 0x09, 'S', 't', 'c', 'k', ' ', 'Q', 't', 'e' 
	*/
	//const uint8_t adv_data[] = {ADV_PAYLOAD_128_UUID, ADV_PAYLOAD_DEV_NAME};
	const uint8_t adv_data[] = {0x11, 0x07, 0x55, 0xe4, 0x05, 0xD2, 0xAF, 0x9F, 0xA9, 0x8f, 0xE5, 0x4A, 0x7D, 0xFE, 0x43, 0x53, 0x53, 0x49,
								0x09, 0x09, 'S', 't', 'c', 'k', ' ', 'Q', 't', 'e'};
	
	at_ble_status_t status = AT_BLE_SUCCESS;
	
	status = at_ble_adv_data_set(adv_data, sizeof(adv_data), NULL, 0);
	if(AT_BLE_SUCCESS != status)
	{
		DBG_LOG("Adv data set failed. Reason = 0x%02X", status);
		return status;
	}
	
	status = at_ble_adv_start(AT_BLE_ADV_TYPE_UNDIRECTED, AT_BLE_ADV_GEN_DISCOVERABLE, NULL, AT_BLE_ADV_FP_ANY, 160, 0, false);
	if(AT_BLE_SUCCESS != status)
	{
		DBG_LOG("Adv start failed. Reason = 0x%02X", status);
		return status;
	}
	
	is_ble_advertising = true;
	DBG_LOG("Advertisement started");
	return status;
}

/**
* \ Send Transparent UART data
*/
static at_ble_status_t ble_app_tu_serv_send_data(uint16_t connhandle, uint8_t *databuf, uint16_t datalen)
{
	at_ble_status_t status;
	uint16_t value = 0;
	uint16_t length = sizeof(uint16_t);
	
	printf("get char val\r\n");
	status = at_ble_characteristic_value_get(transparent_uart.chars[CHAR_TX].client_config_handle, (uint8_t *)&value, &length);
	if (status != AT_BLE_SUCCESS)
	{
		DBG_LOG("at_ble_characteristic_value_get value get failed = 0x%02X", status);
		return status;
	}
	if(value == 1)
	{
		printf("val set\r\n");
		status = at_ble_characteristic_value_set(transparent_uart.chars[CHAR_TX].char_val_handle, databuf, datalen);
		printf("status = %x\r\n",status);
		if (status != AT_BLE_SUCCESS)
		{
			DBG_LOG("at_ble_characteristic_value_set value set failed = 0x%02X", status);
			return status;
		}
		printf("notif send\r\n");
		status = at_ble_notification_send(connhandle, transparent_uart.chars[CHAR_TX].char_val_handle);
		if (status != AT_BLE_SUCCESS)
		{
			DBG_LOG("at_ble_notification_send  failed = 0x%02X", status);
			return status;
		}
	}
	return status;
}

/** @brief Send stock quote to remote device
  * 
  * @param[in] data	Stock quote
  * @param[in] data_len	Stock quote length
  *
  * @return 
  */
//void ble_app_send_stock_quote(uint8_t *data, uint16_t data_len)
void ble_app_send_weather_data(uint8_t *data, uint16_t data_len)
{
	for(uint8_t index = 0; index < MAX_REMOTE_DEVICE; index++)
	{
//		if(remote_dev_info[index].sq_state == BLE_APP_STOCK_QUOTE_UNDER_PROCESSING)
		if(remote_dev_info[index].sq_state == BLE_APP_WEATHER_UNDER_PROCESSING)
		{
			printf("send data packet\r\n");
			ble_app_tu_serv_send_data(remote_dev_info[index].remote_dev_conn_info.handle, data, data_len);
			/* This application is NOT resending stock quote, if it fails first time. */
//			remote_dev_info[index].sq_state = BLE_APP_STOCK_SYMBOL_NOT_RECEIVED;
			remote_dev_info[index].sq_state = BLE_APP_CITY_NAME_NOT_RECEIVED;
			printf("breaking from send packet\r\n");
			break;
		}
	}
}

/** @brief Initialize the Transparent UART
  * 
  * @param[in] buf		Buffer pointer for data to be send
  * @param[in] len      size of buffer
  *
  * @return Upon successful completion the function shall return @ref AT_BLE_SUCCESS,
  * Otherwise the function shall return @ref at_ble_status_t
  */
static at_ble_status_t ble_app_tu_serv_init(uint8_t *buf, uint16_t len)
{
	transparent_uart.serv_handle = 0;
	transparent_uart.serv_uuid.type = AT_BLE_UUID_128;
	memcpy(transparent_uart.serv_uuid.uuid, TU_SERVICE_UUID, UUID_128_LEN);
	
	/* Characteristic TX */
	transparent_uart.chars[CHAR_TX].char_val_handle = 0;
	transparent_uart.chars[CHAR_TX].uuid.type = AT_BLE_UUID_128;
	memcpy(transparent_uart.chars[CHAR_TX].uuid.uuid, TU_TX_CHAR_UUID, UUID_128_LEN);
	transparent_uart.chars[CHAR_TX].properties = AT_BLE_CHAR_NOTIFY | AT_BLE_CHAR_WRITE | AT_BLE_CHAR_WRITE_WITHOUT_RESPONSE;
	transparent_uart.chars[CHAR_TX].init_value = buf;
	transparent_uart.chars[CHAR_TX].value_init_len = len;
	transparent_uart.chars[CHAR_TX].value_max_len = len;
	transparent_uart.chars[CHAR_TX].presentation_format = NULL;
	transparent_uart.chars[CHAR_TX].value_permissions = AT_BLE_ATTR_WRITABLE_NO_AUTHN_NO_AUTHR;
	transparent_uart.chars[CHAR_TX].user_desc_handle = 0;
	transparent_uart.chars[CHAR_TX].user_desc = NULL;
	transparent_uart.chars[CHAR_TX].user_desc_len = 0;
	transparent_uart.chars[CHAR_TX].user_desc_max_len = 0;
	transparent_uart.chars[CHAR_TX].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	transparent_uart.chars[CHAR_TX].client_config_handle = 0;
	transparent_uart.chars[CHAR_TX].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	transparent_uart.chars[CHAR_TX].server_config_handle = 0;
	transparent_uart.chars[CHAR_TX].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	
	/* Characteristic RX */
	transparent_uart.chars[CHAR_RX].char_val_handle = 0;
	transparent_uart.chars[CHAR_RX].uuid.type = AT_BLE_UUID_128;
	memcpy(transparent_uart.chars[CHAR_RX].uuid.uuid, TU_RX_CHAR_UUID, UUID_128_LEN);
	transparent_uart.chars[CHAR_RX].properties = AT_BLE_CHAR_WRITE | AT_BLE_CHAR_WRITE_WITHOUT_RESPONSE;
	transparent_uart.chars[CHAR_RX].init_value = buf;
	transparent_uart.chars[CHAR_RX].value_init_len = len;
	transparent_uart.chars[CHAR_RX].value_max_len = len;
	transparent_uart.chars[CHAR_RX].presentation_format = NULL;
	transparent_uart.chars[CHAR_RX].value_permissions = AT_BLE_ATTR_WRITABLE_NO_AUTHN_NO_AUTHR;
	transparent_uart.chars[CHAR_RX].user_desc_handle = 0;
	transparent_uart.chars[CHAR_RX].user_desc = NULL;
	transparent_uart.chars[CHAR_RX].user_desc_len = 0;
	transparent_uart.chars[CHAR_RX].user_desc_max_len = 0;
	transparent_uart.chars[CHAR_RX].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	transparent_uart.chars[CHAR_RX].client_config_handle = 0;
	transparent_uart.chars[CHAR_RX].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	transparent_uart.chars[CHAR_RX].server_config_handle = 0;
	transparent_uart.chars[CHAR_RX].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	
	/* Characteristic TCP */
	transparent_uart.chars[CHAR_TCP].char_val_handle = 0;
	transparent_uart.chars[CHAR_TCP].uuid.type = AT_BLE_UUID_128;
	memcpy(transparent_uart.chars[CHAR_TCP].uuid.uuid, TU_TCP_CHAR_UUID, UUID_128_LEN);
	transparent_uart.chars[CHAR_TCP].properties = AT_BLE_CHAR_NOTIFY | AT_BLE_CHAR_WRITE;
	transparent_uart.chars[CHAR_TCP].init_value = buf;
	transparent_uart.chars[CHAR_TCP].value_init_len = len;
	transparent_uart.chars[CHAR_TCP].value_max_len = len;
	transparent_uart.chars[CHAR_TCP].presentation_format = NULL;
	transparent_uart.chars[CHAR_TCP].value_permissions = AT_BLE_ATTR_WRITABLE_NO_AUTHN_NO_AUTHR;
	transparent_uart.chars[CHAR_TCP].user_desc_handle = 0;
	transparent_uart.chars[CHAR_TCP].user_desc = NULL;
	transparent_uart.chars[CHAR_TCP].user_desc_len = 0;
	transparent_uart.chars[CHAR_TCP].user_desc_max_len = 0;
	transparent_uart.chars[CHAR_TCP].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	transparent_uart.chars[CHAR_TCP].client_config_handle = 0;
	transparent_uart.chars[CHAR_TCP].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	transparent_uart.chars[CHAR_TCP].server_config_handle = 0;
	transparent_uart.chars[CHAR_TCP].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	
	return AT_BLE_SUCCESS;
}

/** @brief Register Transparent UART service
  * 
  * @param[in] tu_serv	Transparent UART service
  *
  * @return Upon successful completion the function shall return @ref AT_BLE_SUCCESS,
  * Otherwise the function shall return @ref at_ble_status_t
  */
static at_ble_status_t ble_app_tu_primary_service_define(transparent_uart_service_t *tu_serv)
{
	return(at_ble_primary_service_define(&tu_serv->serv_uuid,
										&tu_serv->serv_handle, NULL, 0, 
										&tu_serv->chars[CHAR_TX], TOTAL_NUM_OF_TU_CHARATERISTIC));
}

/** @brief Set BLE application state
  * 
  * @param[in] state	BLE application state
  *
  * @return 
  */
static void ble_app_state_set(ble_app_init_t state)
{
	ble_app_state = state;
}

/** @brief Application initialization
  * 
  * @param
  *
  * @return Upon successful completion the function shall return @ref AT_BLE_SUCCESS,
  * Otherwise the function shall return @ref at_ble_status_t
  */
at_ble_status_t ble_app_init(void)
{
	at_ble_status_t status = AT_BLE_SUCCESS;
	
	/* GAP event callback registration */
	ble_mgr_events_callback_handler(REGISTER_CALL_BACK, BLE_GAP_EVENT_TYPE, &app_ble_gap_event);
	/* GATT Server event callback registration */
	ble_mgr_events_callback_handler(REGISTER_CALL_BACK, BLE_GATT_SERVER_EVENT_TYPE, &app_ble_gatt_server_event);
	
	
	/* Define the Transparent UART primary service in the GATT server database */
	ble_app_tu_serv_init(send_data, APP_BUF_SIZE);
	/* Register Primary service */
	if((status = ble_app_tu_primary_service_define(&transparent_uart)) != AT_BLE_SUCCESS)
	{
		DBG_LOG("Transparent UART Service definition failed,reason %x",status);
		return status;
	}
	
	/* Start advertisement */
	status = ble_app_start_adv();
	if(status != AT_BLE_SUCCESS)
	{
		return status;
	}
	
	return status;
}

/** @brief Set BLE application state to start advertisement
  * 
  * @param
  *
  * @return 
  */
void ble_app_state_set_start_adv(void)
{
	ble_app_state_set(BLE_APP_START_ADV);
}

/** @brief Tells whether BLE App is in @BLE_APP_INIT or not
  * 
  * @param
  *
  * @return true if BLE App is in BLE_APP_INIT, false otherwise
  */
bool ble_app_is_init_state(void)
{
	return ble_app_state == BLE_APP_INIT;
}

/** @brief Get stock symbol sent by remote device
  * 
  * @param
  *
  * @return stock symbol
  */
//char* ble_app_get_stock_symbol(void)
char* ble_app_get_city_name(void)
{
	for(uint8_t conn_index = 0; conn_index < MAX_REMOTE_DEVICE; conn_index++)
	{
		if(remote_dev_info[conn_index].sq_state == BLE_APP_WEATHER_UNDER_PROCESSING)
		{
			return remote_dev_info[conn_index].city_name;
		}
	}
	
	return NULL;
}

/** @brief Handle different BLE application states and BLE events
  * 
  * @param
  *
  * @return 
  */
void ble_app_process(void)
{
	at_ble_status_t status = AT_BLE_SUCCESS;
	
	switch(ble_app_state)
	{
		case BLE_APP_INIT:
		{
			/* Do nothing */
			break;
		}
		
		case BLE_APP_START_ADV:
		{
			/* Initialize BLE application */
			status = ble_app_init();
			if(status == AT_BLE_SUCCESS)
			{
				ble_app_state = BLE_APP_ADV_STARTED;
			}
			
			break;
		}
		
		case BLE_APP_ADV_STARTED:
		{
			/* Do nothing */
			break;
		}
		
		case BLE_APP_CONNECTED:
		{
			for(uint8_t conn_index = 0; conn_index < MAX_REMOTE_DEVICE; conn_index++)
			{
				if(!remote_dev_info[conn_index].entry_flag && !is_ble_advertising)
				{
					is_ble_advertising = true;
					/* Still there is room for more devices. Start advertisement */
					ble_app_start_adv();
					
					break;
				}
			}
			break;
		}
		
		case BLE_APP_DISCONNECTED:
		{
			bool conn_exist = false;
			
			/* Start advertisement, if it is not in progress */
			if(!is_ble_advertising)
			{
				status = ble_app_start_adv();
			}
			
			for(uint8_t conn_index = 0; conn_index < MAX_REMOTE_DEVICE; conn_index++)
			{
				if(remote_dev_info[conn_index].entry_flag)
				{
					conn_exist = true;
					break;
				}
			}
			
			if(conn_exist)
			{
				/* Since there are other connections, don't change the application state */
				break;
			}
			else if(status == AT_BLE_SUCCESS)
			{
				ble_app_state = BLE_APP_ADV_STARTED;
			}
			else
			{
				/* Advertisement fails, start again */
				ble_app_state = BLE_APP_START_ADV;
			}
			
			break;
		}
		
		case BLE_APP_SYMBOL_RECEIVED:
		{
			/* Set to true, if any other devices requested stock quote. Otherwise set to false */
//			bool more_stock_symbol = false;
			bool more_city_name = false;
			
			for(uint8_t conn_index = 0; conn_index < MAX_REMOTE_DEVICE; conn_index++)
			{
//				if(remote_dev_info[conn_index].sq_state == BLE_APP_STOCK_SYMBOL_RECEIVED)
				if(remote_dev_info[conn_index].sq_state == BLE_APP_CITY_NAME_RECEIVED)
				{
//					remote_dev_info[conn_index].sq_state = BLE_APP_STOCK_QUOTE_UNDER_PROCESSING;
					remote_dev_info[conn_index].sq_state = BLE_APP_WEATHER_UNDER_PROCESSING;
					//request_stock_quote(remote_dev_info[conn_index].stock_symbol);
					request_weather(remote_dev_info[conn_index].city_name);
				}
				
//				if(remote_dev_info[conn_index].sq_state == BLE_APP_STOCK_SYMBOL_RECEIVED)
				if(remote_dev_info[conn_index].sq_state == BLE_APP_WEATHER_UNDER_PROCESSING)
				{
//					more_stock_symbol = true;
					more_city_name = true;
				}
			}
			
//			if(!more_stock_symbol)
			if(!more_city_name)
			{
				ble_app_state = BLE_APP_CONNECTED;
			}
			
			break;
		}
		
		default:
			return;
	}
	
	if(ble_app_state >= BLE_APP_START_ADV)
	{
		ble_event_task();
	}
}