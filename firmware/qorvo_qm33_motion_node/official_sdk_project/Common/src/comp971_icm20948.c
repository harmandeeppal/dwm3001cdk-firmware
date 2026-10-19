#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "nrfx_twim.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#include "comp971_icm20948.h"

#define COMP971_I2C_SDA_PIN 26U
#define COMP971_I2C_SCL_PIN 23U
#define COMP971_I2C_TIMEOUT_MS 25U
#define COMP971_ICM_ADDRESS_DEFAULT 0x69U
#define COMP971_ICM_ADDRESS_ALTERNATE 0x68U
#define COMP971_REG_BANK_SELECT 0x7FU
#define COMP971_REG_WHO_AM_I 0x00U
#define COMP971_REG_PWR_MGMT_1 0x06U
#define COMP971_REG_PWR_MGMT_2 0x07U
#define COMP971_REG_LP_CONFIG 0x05U
#define COMP971_REG_ACCEL_XOUT_H 0x2DU
#define COMP971_REG_GYRO_CONFIG_1 0x01U
#define COMP971_REG_ACCEL_CONFIG 0x14U
#define COMP971_WHO_AM_I_VALUE 0xEAU
#define COMP971_FRAME_SIZE 14U

static const nrfx_twim_t m_twim = NRFX_TWIM_INSTANCE(1);
static volatile bool m_transfer_complete;
static volatile nrfx_err_t m_transfer_result;
static bool m_twim_ready;
static uint8_t m_address;
static int8_t m_bank = -1;

static void twim_event_handler(nrfx_twim_evt_t const *event, void *context)
{
    (void)context;
    m_transfer_result = (event->type == NRFX_TWIM_EVT_DONE) ?
        NRFX_SUCCESS : NRFX_ERROR_INTERNAL;
    m_transfer_complete = true;
}

static int twim_transfer(nrfx_twim_xfer_desc_t const *transfer)
{
    TickType_t const deadline = xTaskGetTickCount() +
        pdMS_TO_TICKS(COMP971_I2C_TIMEOUT_MS);
    nrfx_err_t result;

    m_transfer_complete = false;
    m_transfer_result = NRFX_ERROR_INTERNAL;
    result = nrfx_twim_xfer(&m_twim, transfer, 0U);
    if (result != NRFX_SUCCESS) {
        return (int)result;
    }

    while (!m_transfer_complete) {
        if ((int32_t)(xTaskGetTickCount() - deadline) >= 0) {
            return (int)NRFX_ERROR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    return (int)m_transfer_result;
}

static int write_register_raw(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    nrfx_twim_xfer_desc_t transfer = NRFX_TWIM_XFER_DESC_TX(address, data,
                                                               sizeof(data));
    return twim_transfer(&transfer);
}

static int read_registers_raw(uint8_t address, uint8_t reg, uint8_t *data,
                              uint8_t length)
{
    nrfx_twim_xfer_desc_t transfer = NRFX_TWIM_XFER_DESC_TXRX(address, &reg,
                                                                 1U, data, length);
    return twim_transfer(&transfer);
}

static int select_bank(uint8_t bank)
{
    int result;

    if (m_bank == (int8_t)bank) {
        return 0;
    }
    result = write_register_raw(m_address, COMP971_REG_BANK_SELECT,
                                (uint8_t)(bank << 4));
    if (result == 0) {
        m_bank = (int8_t)bank;
    }
    return result;
}

static int write_register(uint8_t bank, uint8_t reg, uint8_t value)
{
    int result = select_bank(bank);
    return (result == 0) ? write_register_raw(m_address, reg, value) : result;
}

static int read_registers(uint8_t bank, uint8_t reg, uint8_t *data,
                          uint8_t length)
{
    int result = select_bank(bank);
    return (result == 0) ? read_registers_raw(m_address, reg, data, length) : result;
}

static int16_t decode_be(uint8_t const *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static void comp971_icm20948_bus_recover(void)
{
    if (m_twim_ready) {
        nrfx_twim_disable(&m_twim);
        nrfx_twim_uninit(&m_twim);
        m_twim_ready = false;
    }

    /* Configure SCL and SDA as open-drain outputs with pull-up to un-wedge bus */
    nrf_gpio_cfg(
        COMP971_I2C_SCL_PIN,
        NRF_GPIO_PIN_DIR_OUTPUT,
        NRF_GPIO_PIN_INPUT_CONNECT,
        NRF_GPIO_PIN_PULLUP,
        NRF_GPIO_PIN_S0D1,
        NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_cfg(
        COMP971_I2C_SDA_PIN,
        NRF_GPIO_PIN_DIR_OUTPUT,
        NRF_GPIO_PIN_INPUT_CONNECT,
        NRF_GPIO_PIN_PULLUP,
        NRF_GPIO_PIN_S0D1,
        NRF_GPIO_PIN_NOSENSE);

    nrf_gpio_pin_set(COMP971_I2C_SCL_PIN);
    nrf_gpio_pin_set(COMP971_I2C_SDA_PIN);
    nrf_delay_us(10U);

    /* Clock SCL up to 16 cycles until slave releases SDA */
    for (int i = 0; i < 16; ++i) {
        if (nrf_gpio_pin_read(COMP971_I2C_SDA_PIN) != 0U) {
            break;
        }
        nrf_gpio_pin_clear(COMP971_I2C_SCL_PIN);
        nrf_delay_us(10U);
        nrf_gpio_pin_set(COMP971_I2C_SCL_PIN);
        nrf_delay_us(10U);
    }

    /* Generate STOP condition: SDA low -> SCL high -> SDA high */
    nrf_gpio_pin_clear(COMP971_I2C_SDA_PIN);
    nrf_delay_us(10U);
    nrf_gpio_pin_set(COMP971_I2C_SCL_PIN);
    nrf_delay_us(10U);
    nrf_gpio_pin_set(COMP971_I2C_SDA_PIN);
    nrf_delay_us(10U);
}

int comp971_icm20948_init(void)
{
    nrfx_twim_config_t const config = {
        .scl = COMP971_I2C_SCL_PIN,
        .sda = COMP971_I2C_SDA_PIN,
        .frequency = NRF_TWIM_FREQ_100K,
        .interrupt_priority = 6U,
        .hold_bus_uninit = false,
    };
    uint8_t who_am_i = 0U;
    int result;

    /* Always ensure the bus is clean and free before initializing TWIM */
    comp971_icm20948_bus_recover();

    result = (int)nrfx_twim_init(&m_twim, &config, twim_event_handler, NULL);
    if (result != (int)NRFX_SUCCESS) {
        return result;
    }
    nrfx_twim_enable(&m_twim);
    m_twim_ready = true;

    m_address = COMP971_ICM_ADDRESS_DEFAULT;
    m_bank = -1;
    result = read_registers(0U, COMP971_REG_WHO_AM_I, &who_am_i, 1U);
    if ((result != 0) || (who_am_i != COMP971_WHO_AM_I_VALUE)) {
        m_address = COMP971_ICM_ADDRESS_ALTERNATE;
        m_bank = -1;
        result = read_registers(0U, COMP971_REG_WHO_AM_I, &who_am_i, 1U);
        if ((result != 0) || (who_am_i != COMP971_WHO_AM_I_VALUE)) {
            comp971_icm20948_bus_recover();
            return (result != 0) ? result : (int)NRFX_ERROR_NOT_SUPPORTED;
        }
    }

    result = write_register(0U, COMP971_REG_PWR_MGMT_1, 0x80U);
    if (result != 0) {
        comp971_icm20948_bus_recover();
        return result;
    }
    m_bank = -1;
    vTaskDelay(pdMS_TO_TICKS(100U));
    result = write_register(0U, COMP971_REG_PWR_MGMT_1, 0x01U);
    if (result == 0) {
        result = write_register(0U, COMP971_REG_PWR_MGMT_2, 0x00U);
    }
    if (result == 0) {
        result = write_register(0U, COMP971_REG_LP_CONFIG, 0x00U);
    }
    if (result == 0) {
        result = write_register(2U, COMP971_REG_ACCEL_CONFIG, 0x00U);
    }
    if (result == 0) {
        result = write_register(2U, COMP971_REG_GYRO_CONFIG_1, 0x00U);
    }
    if (result != 0) {
        comp971_icm20948_bus_recover();
    }
    return result;
}

int comp971_icm20948_read(struct comp971_icm20948_sample *sample)
{
    uint8_t frame[COMP971_FRAME_SIZE];
    int result;

    if (sample == NULL) {
        return (int)NRFX_ERROR_NULL;
    }
    result = read_registers(0U, COMP971_REG_ACCEL_XOUT_H, frame, sizeof(frame));
    if (result != 0) {
        return result;
    }
    sample->ax = decode_be(&frame[0]);
    sample->ay = decode_be(&frame[2]);
    sample->az = decode_be(&frame[4]);
    sample->gx = decode_be(&frame[6]);
    sample->gy = decode_be(&frame[8]);
    sample->gz = decode_be(&frame[10]);
    sample->temperature = decode_be(&frame[12]);
    return 0;
}
