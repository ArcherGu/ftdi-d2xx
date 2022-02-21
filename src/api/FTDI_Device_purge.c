#include "api/FTDI_Device_purge.h"
#include "api/FTDI_Device.h"
#include "ftd2xx.h"
#include "utils.h"

napi_value device_purge(napi_env env, napi_callback_info info) {
  // Get JavaScript `this` corresponding to this instance of the class and `argc`/`argv` passed to the function
  const size_t nb_args = 1; // number of expected arguments
  size_t argc = nb_args; // size of the argv buffer
  napi_value this_arg, argv[nb_args];
  utils_check(napi_get_cb_info(env, info, &argc, argv, &this_arg, NULL));
  if(utils_check(argc < nb_args, "Missing argument", "missarg")) return NULL;

  // Get the class instance data containing FTDI device handle
  device_instance_data_t* instance_data;
  utils_check(napi_unwrap(env, this_arg, (void**)(&instance_data)));

  // Check the device is open if its handle is still there
  if(utils_check(instance_data->ftHandle == NULL, "Dead device object", "deadobj")) return NULL;

  // Check arguments, and convert JavaScript values to C values
  uint32_t mask;
  if(utils_check(((napi_get_value_uint32(env, argv[0], &mask) != napi_ok) || (mask & 0b11) == 0),
      "Mask must be a combination of of FT_PURGE_RX and FT_PURGE_TX", "wrongarg")) return NULL;

  // Update FTDI device
  utils_check(FT_Purge(instance_data->ftHandle, mask));

  return NULL;
}
