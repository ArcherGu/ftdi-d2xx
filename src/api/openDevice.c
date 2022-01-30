#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api/openDevice.h"
#include "api/FTDI_Device.h"
#include "module_data.h"
#include "ftd2xx.h"
#include "utils.h"

typedef struct {
  // Node-API variables
  napi_async_work async_work;
  napi_deferred deferred;

  // Pointer to global module data
  module_data_t* module_data;

  // Data passed to execute_callback
  char serial_number[128];

  // Data passed to complete_callback
  FT_STATUS ftStatus;
  FT_HANDLE ftHandle;

} async_data_t;


// This function runs on a worker thread.
// It has no access to the JavaScript. Only FTDI functions are called here.
static void execute_callback(napi_env env, void* data) {
  async_data_t* async_data = (async_data_t*) data;

  // Open FTDI device
  async_data->ftStatus = FT_OpenEx(async_data->serial_number, FT_OPEN_BY_SERIAL_NUMBER, &(async_data->ftHandle));
}


// This function runs on the main thread after `execute_callback` exits.
// JavaScript functions are called here to convert data generated by FTDI.
static void complete_callback(napi_env env, napi_status status, void* data) {
  napi_value device_class, serial_number, device_instance;
  async_data_t* async_data = (async_data_t*) data;

  // Manage FT_OpenEx error if any. Otherwise, process the return value
  if(!utils_check(async_data->ftStatus == FT_DEVICE_NOT_FOUND, "Device not found")
    && !utils_check(async_data->ftStatus == FT_DEVICE_NOT_OPENED, "Device could not be opened as it may be already open") //TODO check if ftHandle is populated then dismiss this error
    && !utils_check(FT_|async_data->ftStatus)) { // manage other errors

    // Get FTDI_Device class from its reference
    utils_check(napi_get_reference_value(env, async_data->module_data->device_class_ref, &device_class));

    // Convert serial number string to JavaScript
    utils_check(napi_create_string_utf8(env, async_data->serial_number, NAPI_AUTO_LENGTH, &serial_number));

    // Create FTDI_Device class instance
    utils_check(napi_new_instance(env, device_class, 1, &serial_number, &device_instance));
    device_instance_set_handler(env, device_instance, async_data->ftHandle);
  }

  // Resolve the JavaScript `Promise`:
  bool is_exception_pending;
  napi_is_exception_pending(env, &is_exception_pending);
  if(is_exception_pending) {
    // If an exception is pending, clear it to prevent Node.js from crashing
    napi_value error;
    napi_get_and_clear_last_exception(env, &error);

    // Reject the JavaScript `Promise` with the error
    napi_reject_deferred(env, async_data->deferred, error);

  } else {
    // Else resolve the JavaScript `Promise` with the return value
    napi_resolve_deferred(env, async_data->deferred, device_instance);
  }

  // Clean up the work item associated with this run
  napi_delete_async_work(env, async_data->async_work);

  // Free async instance data structure
  free(async_data);
}


// Create a deferred JavaScript `Promise` and an async queue work item
napi_value openDevice(napi_env env, napi_callback_info info) {
  // Get JavaScript `argc`/`argv` passed to the function
  size_t argc = 1; // size of the buffer
  napi_value argv[argc];
  utils_check(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  if(utils_check(argc < 1, "Missing argument")) return NULL;

  // Check that the serial number argument is a string
  napi_valuetype type;
  utils_check(napi_typeof(env, argv[0], &type));
  if(utils_check(type != napi_string, "Device serial number must be a string")) return NULL;

  // Allocate memory for async instance data structure
  async_data_t* async_data = malloc(sizeof(async_data_t));

  // Copy the global module data pointer to the async instance data
  utils_check(napi_get_cb_info(env, info, NULL, NULL, NULL, (void**)(&(async_data->module_data))));

  // Get the device serial number from argument and copy it to the async instance data
  utils_check(napi_get_value_string_utf8(env, argv[0], async_data->serial_number, sizeof(async_data->serial_number), NULL));

  // Create a deferred `Promise` which we will resolve at the completion of the work
  napi_value promise;
  utils_check(napi_create_promise(env, &(async_data->deferred), &promise));

  // Create an async work item, passing in the addon data, which will give the worker thread access to the `Promise`
  napi_value name;
  utils_check(napi_create_string_utf8(env, "openDevice", NAPI_AUTO_LENGTH, &name));
  utils_check(napi_create_async_work(env, NULL, name, execute_callback, complete_callback, async_data, &(async_data->async_work)));

  // Queue the work item for execution
  utils_check(napi_queue_async_work(env, async_data->async_work));

  // This causes created `Promise` to be returned to JavaScript
  return promise;
}
