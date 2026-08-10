#ifndef TEST_NRF_LOG_H
#define TEST_NRF_LOG_H

#define NRF_ERROR_INVALID_PARAM 7
#define APP_ERROR_CHECK(error) do { (void)(error); } while (0)
#define NRF_LOG_MODULE_REGISTER() typedef int test_nrf_log_module_registration_t

#endif
