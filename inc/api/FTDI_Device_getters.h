#ifndef FTDI_Device_getters_h
#define FTDI_Device_getters_h

#include <node_api.h>

napi_value device_get_info(napi_env env, napi_callback_info info);
napi_value device_get_is_open(napi_env env, napi_callback_info info);
napi_value device_get_modem_status(napi_env env, napi_callback_info info);

#endif