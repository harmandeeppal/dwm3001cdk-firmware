/*
 * DIY-UWB — SS-TWR with bidirectional COBS serial bridge
 * Zephyr 2.7.1 / nRF52833 / DWM3001CDK
 *
 * Both units range continuously at ~50 Hz.
 * COBS packets received on UART are embedded in the next UWB frame.
 * UWB payloads received from the peer are output as COBS on UART.
 *
 * COBS output frame: [type:1][range_cm:4 LE i32][plen:1][payload:0-64]
 *   type 0x01 = initiator report, 0x02 = responder report
 *
 * Permanent device identity is supplied by -DUWB_NODE_NAME.
 * Temporary test role is supplied separately:
 *   -DUWB_INITIATOR  -> sends polls and reports distance
 *   (default)        -> listens and replies
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/time_units.h>
#include <string.h>

#include "deca_device_api.h"
#include "deca_interface.h"
#include "ble_motion_transport.h"
#include "icm20948_stream.h"

#ifdef COMP971_UWB_TX_STATUS_DIAGNOSTICS
extern int32_t dwt_get_last_tx_failure_detail(void);
#endif

#ifdef UWB_MESH_NODE_INDEX
static void mesh_queue_icm_sample(const struct icm20948_sample *sample);
#endif

/* ── Platform shims ─────────────────────────────────────────────────────────*/
void deca_usleep(unsigned long time_us) { k_usleep((int32_t)time_us); }
void deca_sleep(unsigned int time_ms)   { k_msleep((int32_t)time_ms); }
decaIrqStatus_t decamutexon(void)       { return (decaIrqStatus_t)irq_lock(); }
void decamutexoff(decaIrqStatus_t s)    { irq_unlock((unsigned int)s); }

/* ── DT nodes ───────────────────────────────────────────────────────────────*/
#define SPI_NODE  DT_NODELABEL(spi3)
#define UART_NODE DT_NODELABEL(uart0)

static const struct device *gpio0_dev;
static const struct device *gpio1_dev;

#define IRQ_PIN     2U   /* P1.02 */
#define RST_PIN    25U   /* P0.25 */
#define WAKEUP_PIN 19U   /* P1.19 */

/* ── LEDs (active-low) ─────────────────────────────────────────────────────*/
#define LED1_PIN    4U   /* D9:  TX */
#define LED2_PIN    5U   /* D10: RX */
#define LED3_PIN   22U   /* D11: OK */
#define LED4_PIN   14U   /* D12: ERR */

static void led_on(uint8_t pin)  { gpio_pin_set(gpio0_dev, pin, 1); }
static void led_off(uint8_t pin) { gpio_pin_set(gpio0_dev, pin, 0); }

/* ── SPI ────────────────────────────────────────────────────────────────────*/
static const struct device *spi_dev;

#define SPI_OP (SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER)

/* Use the same chip-select path as the working standalone DW3110 probe:
 * the devicetree supplies the pin, and this application asserts it around
 * each SPI transfer. */
static const struct gpio_dt_spec uwb_cs =
	GPIO_DT_SPEC_GET_BY_IDX(SPI_NODE, cs_gpios, 0);

static struct spi_config spi_cfg_slow = {
	/* DW3110 requires a conservative clock while its identity is probed. */
	.frequency = 500000U, .operation = SPI_OP, .slave = 0, .cs = { 0 },
};
static struct spi_config spi_cfg_fast = {
	.frequency = 4000000U, .operation = SPI_OP, .slave = 0, .cs = { 0 },
};

static struct spi_config *spi_cfg = &spi_cfg_slow;

#define SPI_SCRATCH_LEN 512U
static uint8_t spi_tx_scratch[SPI_SCRATCH_LEN];
static uint8_t spi_rx_scratch[SPI_SCRATCH_LEN];

static int uwb_readfromspi(uint16_t hlen, uint8_t *hbuf,
			   uint16_t rlen, uint8_t *rbuf)
{
	uint16_t total = hlen + rlen;
	if (!hbuf || !rbuf || total > SPI_SCRATCH_LEN) { return -1; }

	memcpy(spi_tx_scratch, hbuf, hlen);
	memset(spi_tx_scratch + hlen, 0x00, rlen);

	const struct spi_buf tx_b = { .buf = spi_tx_scratch, .len = total };
	const struct spi_buf rx_b = { .buf = spi_rx_scratch, .len = total };
	const struct spi_buf_set tx_set = { .buffers = &tx_b, .count = 1 };
	const struct spi_buf_set rx_set = { .buffers = &rx_b, .count = 1 };

	(void)gpio_pin_set_dt(&uwb_cs, 1);
	k_busy_wait(1U);
	int ret = spi_transceive(spi_dev, spi_cfg, &tx_set, &rx_set);
	k_busy_wait(1U);
	(void)gpio_pin_set_dt(&uwb_cs, 0);
	if (ret == 0) { memcpy(rbuf, spi_rx_scratch + hlen, rlen); }
	return ret;
}

static int uwb_writetospi(uint16_t hlen, const uint8_t *hbuf,
			  uint16_t blen, const uint8_t *bbuf)
{
	uint16_t total = hlen + blen;
	if (!hbuf || total > SPI_SCRATCH_LEN) { return -1; }

	memcpy(spi_tx_scratch, hbuf, hlen);
	if (blen > 0 && bbuf) { memcpy(spi_tx_scratch + hlen, bbuf, blen); }

	const struct spi_buf tx_b = { .buf = spi_tx_scratch, .len = total };
	const struct spi_buf rx_b = { .buf = spi_rx_scratch, .len = total };
	const struct spi_buf_set tx_set = { .buffers = &tx_b, .count = 1 };
	const struct spi_buf_set rx_set = { .buffers = &rx_b, .count = 1 };

	(void)gpio_pin_set_dt(&uwb_cs, 1);
	k_busy_wait(1U);
	int ret = spi_transceive(spi_dev, spi_cfg, &tx_set, &rx_set);
	k_busy_wait(1U);
	(void)gpio_pin_set_dt(&uwb_cs, 0);
	return ret;
}

static int uwb_writetospi_crc(uint16_t hlen, const uint8_t *hbuf,
			      uint16_t blen, const uint8_t *bbuf, uint8_t crc8)
{
	ARG_UNUSED(crc8);
	return uwb_writetospi(hlen, hbuf, blen, bbuf);
}

static void uwb_spi_setfastrate(void) { spi_cfg = &spi_cfg_fast; }
static void uwb_spi_setslowrate(void) { spi_cfg = &spi_cfg_slow; }

/* ── DW3110 control ─────────────────────────────────────────────────────────*/
static void uwb_wakeup_fn(void)
{
	/* A hard reset below is sufficient on the DWM3001CDK.  The board's
	 * WAKEUP pin is not pulsed here because that leaves some modules with no
	 * SPI response during the initial device-ID probe. */
}

static int waitforsysstatus(uint32_t *lo_result, uint32_t *hi_result,
			    uint32_t lo_mask, uint32_t hi_mask)
{
	uint32_t lo, hi = 0U;
	uint32_t cnt = 0U;
	do {
		lo = dwt_readsysstatuslo();
		if (hi_mask) { hi = dwt_readsysstatushi(); }
		if (++cnt > 500000U) {
			if (lo_result) { *lo_result = lo; }
			if (hi_result) { *hi_result = hi; }
			return -1;
		}
	} while (!(lo & lo_mask) && !(hi & hi_mask));
	if (lo_result) { *lo_result = lo; }
	if (hi_result) { *hi_result = hi; }
	return 0;
}

/* ── COBS encode/decode ─────────────────────────────────────────────────────*/
/*
 * COBS encoding: replaces all 0x00 bytes so the frame can be delimited by 0x00.
 * Output: [encoded bytes...] 0x00
 */
static uint16_t cobs_encode(const uint8_t *in, uint16_t in_len,
			    uint8_t *out, uint16_t out_max)
{
	uint16_t out_idx = 1;  /* leave room for first code byte */
	uint16_t code_idx = 0;
	uint8_t code = 1;

	for (uint16_t i = 0; i < in_len; i++) {
		if (out_idx >= out_max - 1U) { return 0; }  /* overflow */
		if (in[i] == 0x00) {
			out[code_idx] = code;
			code_idx = out_idx++;
			code = 1;
		} else {
			out[out_idx++] = in[i];
			code++;
			if (code == 0xFF) {
				out[code_idx] = code;
				code_idx = out_idx++;
				code = 1;
			}
		}
	}
	out[code_idx] = code;
	if (out_idx >= out_max) { return 0; }
	out[out_idx++] = 0x00;  /* frame delimiter */
	return out_idx;
}

static uint16_t cobs_decode(const uint8_t *in, uint16_t in_len,
			    uint8_t *out, uint16_t out_max)
{
	uint16_t out_idx = 0;
	uint16_t i = 0;

	while (i < in_len) {
		uint8_t code = in[i++];
		if (code == 0) { break; }  /* end of frame */
		for (uint8_t j = 1; j < code; j++) {
			if (i >= in_len || out_idx >= out_max) { return 0; }
			out[out_idx++] = in[i++];
		}
		if (code < 0xFF && i < in_len) {
			if (out_idx >= out_max) { return 0; }
			out[out_idx++] = 0x00;
		}
	}
	/* Remove trailing 0x00 added by the decode if present */
	if (out_idx > 0 && out[out_idx - 1] == 0x00) { out_idx--; }
	return out_idx;
}

/* ── UART + COBS framing ───────────────────────────────────────────────────*/
#define PAYLOAD_MAX  100U
#define COBS_BUF_MAX (PAYLOAD_MAX + 10U)

static const struct device *uart_dev;
K_MUTEX_DEFINE(uart_tx_mutex);

/* RX: message queue holds decoded COBS payloads (up to 4 pending) */
struct uart_msg {
	uint8_t data[PAYLOAD_MAX];
	uint8_t len;
};

K_MSGQ_DEFINE(uart_rx_msgq, sizeof(struct uart_msg), 2, 4);

/* ISR accumulates COBS bytes, decodes on 0x00, enqueues */
static uint8_t  uart_rx_cobs[COBS_BUF_MAX];
static uint16_t uart_rx_cobs_len;

static void uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	while (uart_irq_update(dev) && uart_irq_rx_ready(dev)) {
		uint8_t c;
		if (uart_fifo_read(dev, &c, 1) != 1) { break; }

		if (c == 0x00) {
			if (uart_rx_cobs_len > 0) {
				struct uart_msg msg;
				uint16_t dlen = cobs_decode(uart_rx_cobs,
							    uart_rx_cobs_len,
							    msg.data,
							    PAYLOAD_MAX);
				if (dlen > 0) {
					msg.len = (uint8_t)dlen;
					k_msgq_put(&uart_rx_msgq, &msg, K_NO_WAIT);
				}
			}
			uart_rx_cobs_len = 0;
		} else if (uart_rx_cobs_len < COBS_BUF_MAX) {
			uart_rx_cobs[uart_rx_cobs_len++] = c;
		}
	}
}

/* Grab the next pending UART payload (non-blocking). Returns len or 0. */
static uint8_t uart_rx_get(uint8_t *buf)
{
	struct uart_msg msg;
	if (k_msgq_get(&uart_rx_msgq, &msg, K_NO_WAIT) == 0) {
		memcpy(buf, msg.data, msg.len);
		return msg.len;
	}
	return 0;
}

/* TX: send COBS-encoded output frame */
static void uart_tx_bytes(const uint8_t *data, uint16_t len)
{
	k_mutex_lock(&uart_tx_mutex, K_FOREVER);
	for (uint16_t i = 0; i < len; i++) {
		uart_poll_out(uart_dev, data[i]);
	}
	k_mutex_unlock(&uart_tx_mutex);
}

/*
 * Build and send a COBS output frame:
 * [type:1][range_cm:4 LE i32][plen:1][payload:0-64]
 */
static void cobs_output(uint8_t type, int32_t range_cm,
			const uint8_t *payload, uint8_t plen)
{
	uint8_t raw[6 + PAYLOAD_MAX];
	raw[0] = type;
	raw[1] = (uint8_t)(range_cm);
	raw[2] = (uint8_t)(range_cm >> 8);
	raw[3] = (uint8_t)(range_cm >> 16);
	raw[4] = (uint8_t)(range_cm >> 24);
	raw[5] = plen;
	if (plen > 0 && payload) {
		memcpy(&raw[6], payload, plen);
	}
	uint16_t raw_len = 6U + plen;

	uint8_t cobs_buf[COBS_BUF_MAX + 10];
	uint16_t enc_len = cobs_encode(raw, raw_len, cobs_buf, sizeof(cobs_buf));
	if (enc_len > 0) {
		uart_tx_bytes(cobs_buf, enc_len);
	}
}

/* Type 0x03 payload: source_node:u8, seq:u32, t_ms:u32, accel/gyro/temp:i16x7.
 * This is a 6-axis stream: accelerometer and gyroscope only.  Raw values
 * are converted and fused into the model's 6D orientation by the collector.
 */
static void put_le16(uint8_t *out, int16_t value)
{
	out[0] = (uint8_t)value;
	out[1] = (uint8_t)((uint16_t)value >> 8);
}

static void put_le32(uint8_t *out, uint32_t value)
{
	out[0] = (uint8_t)value;
	out[1] = (uint8_t)(value >> 8);
	out[2] = (uint8_t)(value >> 16);
	out[3] = (uint8_t)(value >> 24);
}

static void encode_icm_sample(const struct icm20948_sample *sample, uint8_t *payload)
{
	const int16_t values[] = {
		sample->ax, sample->ay, sample->az,
		sample->gx, sample->gy, sample->gz,
		sample->temperature,
	};

	put_le32(&payload[0], sample->sequence);
	put_le32(&payload[4], sample->timestamp_ms);
	for (uint8_t i = 0; i < 7U; ++i) {
		put_le16(&payload[8U + (uint8_t)(i * 2U)], values[i]);
	}
}

static void emit_icm_sample_from_node(uint8_t node_index,
				      const struct icm20948_sample *sample)
{
	uint8_t payload[23];
	payload[0] = node_index;
	encode_icm_sample(sample, &payload[1]);
	cobs_output(0x03, -1, payload, sizeof(payload));
}

static void emit_icm_sample(const struct icm20948_sample *sample)
{
    /* The final transport is direct BLE to the dongle.  Do not consume UWB
     * airtime by forwarding raw IMU samples through PELVIS. */
	motion_ble_queue_imu(sample);
#ifdef COMP971_DIRECT_BLE_IMU
	ARG_UNUSED(sample);
	return;
#else
#ifdef UWB_MESH_NODE_INDEX
	if (UWB_MESH_NODE_INDEX != 1U) {
		mesh_queue_icm_sample(sample);
		return;
	}
	emit_icm_sample_from_node(UWB_MESH_NODE_INDEX, sample);
#else
	emit_icm_sample_from_node(0xFFU, sample);
#endif
#endif
}

/* ── UWB config ─────────────────────────────────────────────────────────────*/
static dwt_config_t uwb_config = {
	.chan            = 5,
	.txPreambLength  = DWT_PLEN_128,
	.rxPAC           = DWT_PAC8,
	.txCode          = 9,
	.rxCode          = 9,
	.sfdType         = DWT_SFD_DW_8,
	.dataRate        = DWT_BR_6M8,
	.phrMode         = DWT_PHRMODE_STD,
	.phrRate         = DWT_PHRRATE_STD,
	.sfdTO           = (128 + 1 + 8 - 8),
	.stsMode         = DWT_STS_MODE_OFF,
	.stsLength       = DWT_STS_LEN_64,
	.pdoaMode        = DWT_PDOA_M0,
};

/* Qorvo DW3xxx API Guide, Tables 6 and 7: explicit channel-5 TX spectrum
 * configuration.  dwt_configure() configures the radio PHY but does not set
 * the pulse-generator delay and TX power held in this separate structure.
 * Leaving the chip boot defaults in place makes the receive quality and hence
 * ranging behaviour dependent on the prior device state. */
static dwt_txconfig_t uwb_txconfig = {
	.PGdly = 0x34U,
	.power = 0xfdfdfdfdUL,
	.PGcount = 0U,
};

/* ── Frame layout ───────────────────────────────────────────────────────────
 * All frames: [FC:2][SN:1][PAN:2][DST:2][SRC:2][FUNC:1] = 10 bytes header
 * Poll:     [HDR:10][plen:1][payload:0-64]
 * Response: [HDR:10][reply_time:5][plen:1][payload:0-64]
 * FCS (2B) appended by hardware.
 */
#define HDR_LEN           10U
#define SN_IDX             2U
#define FUNC_CODE_IDX      9U
#define FUNC_POLL         0x21U
#define FUNC_RESP         0x10U
#define APP_FCS_LEN        2U

#define POLL_PLEN_IDX     10U
#define POLL_PAYLOAD_IDX  11U

#define RESP_REPLY_IDX    10U
#define RESP_PLEN_IDX     15U
#define RESP_PAYLOAD_IDX  16U
#define RESP_HDR_LEN      16U  /* minimum response length (no payload) */

/* Timing */
#ifndef UWB_RESP_DELAY_UUS
/* Default for ordinary builds. The pair-only diagnostic build may override
 * this to test additional delayed-transmit preparation margin. */
#define UWB_RESP_DELAY_UUS 2000U
#endif

#define POLL_RX_TO_RESP_DLY_UUS UWB_RESP_DELAY_UUS
#define RESP_RX_TIMEOUT_UUS     20000U
#define UUS_TO_DWT              65536UL
#define DWT_TS_MASK             0xFFFFFFFFFFULL

#ifndef UWB_TX_ANT_DLY
#define UWB_TX_ANT_DLY 16390U
#endif

#ifndef UWB_RX_ANT_DLY
#define UWB_RX_ANT_DLY 16390U
#endif

/* Qorvo's DWM3001CDK OTP Revision 1 map identifies word 0x1A as the
 * channel-5 PRF-64 antenna-delay calibration word: bits [31:16] are RX and
 * bits [15:0] are TX.  This optional diagnostic only calls dwt_otpread(); it
 * never calls an OTP-programming API and never applies the read value. */
#ifdef COMP971_FACTORY_OTP_REPORT
#define COMP971_FACTORY_OTP_CH5_PRF64_ADDR 0x001AU
#ifndef COMP971_FACTORY_OTP_PAIR_INDEX
#define COMP971_FACTORY_OTP_PAIR_INDEX 5U
#endif
static uint32_t factory_otp_ch5_prf64;

static void maybe_publish_factory_otp(void)
{
	static int64_t next_report_ms;
	int64_t now_ms = k_uptime_get();

	/* Send periodically so the viewer's 20-second old-record drain does not
	 * discard the only copy. The diagnostic is one 32-bit read-only OTP word. */
	if (now_ms < next_report_ms) {
		return;
	}
	next_report_ms = now_ms + 10000LL;
	motion_ble_publish_uwb_diagnostic(COMP971_FACTORY_OTP_PAIR_INDEX,
		0x50U, (int32_t)factory_otp_ch5_prf64);
}
#endif

#ifndef UWB_NODE_NAME
#define UWB_NODE_NAME "DWM-UNCONFIGURED"
#endif

static uint8_t frame_sn;
static int32_t last_range_cm = -1;  /* shared between roles */

/* ── Timestamp helpers ──────────────────────────────────────────────────────*/
static uint64_t get_tx_ts(void)
{
	uint8_t b[5];
	dwt_readtxtimestamp(b);
	return (uint64_t)b[0] | ((uint64_t)b[1] << 8) | ((uint64_t)b[2] << 16) |
	       ((uint64_t)b[3] << 24) | ((uint64_t)b[4] << 32);
}

static uint64_t get_rx_ts(void)
{
	uint8_t b[5];
	dwt_readrxtimestamp(b, DWT_COMPAT_NONE);
	return (uint64_t)b[0] | ((uint64_t)b[1] << 8) | ((uint64_t)b[2] << 16) |
	       ((uint64_t)b[3] << 24) | ((uint64_t)b[4] << 32);
}

static void pack_ts5(uint8_t *buf, uint64_t ts)
{
	buf[0] = (uint8_t)(ts);       buf[1] = (uint8_t)(ts >> 8);
	buf[2] = (uint8_t)(ts >> 16); buf[3] = (uint8_t)(ts >> 24);
	buf[4] = (uint8_t)(ts >> 32);
}

static uint64_t unpack_ts5(const uint8_t *buf)
{
	return (uint64_t)buf[0] | ((uint64_t)buf[1] << 8) |
	       ((uint64_t)buf[2] << 16) | ((uint64_t)buf[3] << 24) |
	       ((uint64_t)buf[4] << 32);
}

/* ── Legacy two-board initiator/responder test ──────────────────────────────*/
#ifndef UWB_MESH_NODE_INDEX
#ifdef UWB_INITIATOR

static void run_initiator(void)
{
	uint8_t rx_buf[HDR_LEN + 1U + PAYLOAD_MAX + 16U];
	uint32_t status;
	uint8_t tx_poll[HDR_LEN + 1U + PAYLOAD_MAX];

	static const uint8_t poll_hdr[] =
		{ 0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', FUNC_POLL };
	memcpy(tx_poll, poll_hdr, sizeof(poll_hdr));

	/* Pending payload: held until successfully transmitted over UWB */
	uint8_t pending_payload[PAYLOAD_MAX];
	uint8_t pending_len = 0;
	bool    pending_sent = false;  /* true once UWB TX succeeded */

	while (1) {
		/* If previous payload was sent, grab next from queue */
		if (pending_len == 0 || pending_sent) {
			uint8_t new_len = uart_rx_get(pending_payload);
			if (new_len > 0) {
				pending_len = new_len;
				pending_sent = false;
			} else if (pending_sent) {
				pending_len = 0;  /* Nothing new, clear old */
			}
		}
		uint8_t plen = pending_len;
		uint8_t *payload = pending_payload;

		/* Build poll frame */
		tx_poll[SN_IDX]        = frame_sn;
		tx_poll[POLL_PLEN_IDX] = plen;
		if (plen > 0) { memcpy(&tx_poll[POLL_PAYLOAD_IDX], payload, plen); }
		uint16_t poll_len = (uint16_t)(HDR_LEN + 1U + plen);

		/* TX poll */
		dwt_forcetrxoff();
		dwt_writetxdata(poll_len, tx_poll, 0);
		dwt_writetxfctrl(poll_len + APP_FCS_LEN, 0, 1);
		dwt_setrxaftertxdelay(0);
		dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

		led_on(LED1_PIN);
		if (dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED)
		    != DWT_SUCCESS) {
			led_off(LED1_PIN);
			k_msleep(5);
			continue;
		}

		/* Wait TX done */
		if (waitforsysstatus(&status, NULL, DWT_INT_TXFRS_BIT_MASK, 0U) != 0) {
			led_off(LED1_PIN); dwt_forcetrxoff(); k_msleep(5); continue;
		}
		uint64_t poll_tx_ts = get_tx_ts();
		dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
		led_off(LED1_PIN);
		frame_sn++;

		/* Wait for response */
		if (waitforsysstatus(&status, NULL,
				DWT_INT_RXFCG_BIT_MASK |
				SYS_STATUS_ALL_RX_TO |
				SYS_STATUS_ALL_RX_ERR, 0U) != 0 ||
		    !(status & DWT_INT_RXFCG_BIT_MASK)) {
			dwt_writesysstatuslo(0xFFFFFFFFUL);
			dwt_forcetrxoff();
			k_msleep(5);
			continue;
		}
		uint64_t resp_rx_ts = get_rx_ts();
		dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);

		uint8_t rng_bit;
		uint16_t flen = dwt_getframelength(&rng_bit);
		if (flen > sizeof(rx_buf) || flen < RESP_HDR_LEN) {
			dwt_forcetrxoff(); k_msleep(5); continue;
		}
		dwt_readrxdata(rx_buf, flen, 0);
		if (rx_buf[FUNC_CODE_IDX] != FUNC_RESP) {
			dwt_forcetrxoff(); k_msleep(5); continue;
		}

		led_on(LED2_PIN);

		/* Compute range */
		uint64_t reply_time = unpack_ts5(&rx_buf[RESP_REPLY_IDX]);
		last_range_cm = -1;
		if (reply_time > 0) {
			/*
			 * Correct the responder delay for crystal-clock differences.
			 * This follows Qorvo's SS-TWR calculation. The correction is
			 * essential here because the responder delay is several ms.
			 */
			double clock_offset_ratio =
				(double)dwt_readclockoffset() * CLOCK_OFFSET_PPM_TO_RATIO;
			uint64_t rtd_init =
				(resp_rx_ts - poll_tx_ts) & DWT_TS_MASK;
			uint64_t rtd_resp = reply_time & DWT_TS_MASK;
			double tof_ticks =
				((double)rtd_init -
				 (double)rtd_resp * (1.0 - clock_offset_ratio)) / 2.0;

			if (tof_ticks > 0.0) {
				double dist_m = tof_ticks * DWT_TIME_UNITS * 299702547.0;
				if (dist_m <= 100.0) {
					last_range_cm = (int32_t)(dist_m * 100.0);
				}
			}
		}

		/* Payload was successfully carried in the poll → mark as sent */
		if (plen > 0) {
			pending_sent = true;
		}

		/* Extract responder's payload from response */
		uint8_t resp_plen = rx_buf[RESP_PLEN_IDX];
		if (resp_plen > PAYLOAD_MAX) { resp_plen = 0; }

		/* Always report the measured range to the connected laptop. */
		cobs_output(0x01, last_range_cm,
			    &rx_buf[RESP_PAYLOAD_IDX], resp_plen);

		led_off(LED2_PIN);
		led_on(LED3_PIN); k_usleep(200); led_off(LED3_PIN);

		k_msleep(5);  /* ~50 Hz with exchange overhead */
	}
}

/* ── Responder ──────────────────────────────────────────────────────────────*/
#else

static void run_responder(void)
{
	uint8_t rx_buf[HDR_LEN + 1U + PAYLOAD_MAX + 16U];
	uint32_t status;
	uint8_t tx_resp[RESP_HDR_LEN + PAYLOAD_MAX];

	static const uint8_t resp_hdr[] =
		{ 0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', FUNC_RESP };
	memcpy(tx_resp, resp_hdr, sizeof(resp_hdr));

	dwt_setrxtimeout(0U);
	dwt_setpreambledetecttimeout(0U);

	/* Pending payload: held until successfully sent in a response */
	uint8_t pending_payload[PAYLOAD_MAX];
	uint8_t pending_len = 0;
	bool    pending_sent = false;

	while (1) {
		/* If previous payload was sent, grab next from queue */
		if (pending_len == 0 || pending_sent) {
			uint8_t new_len = uart_rx_get(pending_payload);
			if (new_len > 0) {
				pending_len = new_len;
				pending_sent = false;
			} else if (pending_sent) {
				pending_len = 0;
			}
		}

		dwt_rxenable(DWT_START_RX_IMMEDIATE);

		waitforsysstatus(&status, NULL,
				 DWT_INT_RXFCG_BIT_MASK |
				 SYS_STATUS_ALL_RX_TO |
				 SYS_STATUS_ALL_RX_ERR, 0U);

		if (!(status & DWT_INT_RXFCG_BIT_MASK)) {
			dwt_writesysstatuslo(SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
			continue;
		}

		uint64_t poll_rx_ts = get_rx_ts();
		led_on(LED2_PIN);
		dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);

		uint8_t rng_bit;
		uint16_t flen = dwt_getframelength(&rng_bit);
		if (flen > sizeof(rx_buf)) { led_off(LED2_PIN); continue; }
		dwt_readrxdata(rx_buf, flen, 0);
		if (rx_buf[FUNC_CODE_IDX] != FUNC_POLL) { led_off(LED2_PIN); continue; }

		/* Extract initiator's payload */
		uint8_t poll_plen = rx_buf[POLL_PLEN_IDX];
		if (poll_plen > PAYLOAD_MAX) { poll_plen = 0; }

		uint8_t resp_plen = pending_len;
		uint8_t *resp_payload = pending_payload;

		/* Build response: [HDR][reply_time:5][plen:1][payload:0-N] */
		tx_resp[SN_IDX] = rx_buf[SN_IDX];

		/* Delayed TX for precise reply time */
		uint32_t resp_tx_time =
			(uint32_t)((poll_rx_ts +
				    (uint64_t)POLL_RX_TO_RESP_DLY_UUS * UUS_TO_DWT) >> 8);
		dwt_setdelayedtrxtime(resp_tx_time);

		uint64_t resp_tx_ts =
			(((uint64_t)(resp_tx_time & 0xFFFFFFFEUL)) << 8) + UWB_TX_ANT_DLY;
		uint64_t reply_time = resp_tx_ts - poll_rx_ts;
		pack_ts5(&tx_resp[RESP_REPLY_IDX], reply_time);

		tx_resp[RESP_PLEN_IDX] = resp_plen;
		if (resp_plen > 0) {
			memcpy(&tx_resp[RESP_PAYLOAD_IDX], resp_payload, resp_plen);
		}
		uint16_t resp_len = (uint16_t)(RESP_HDR_LEN + resp_plen);

		dwt_forcetrxoff();
		dwt_writetxdata(resp_len, tx_resp, 0);
		dwt_writetxfctrl(resp_len + APP_FCS_LEN, 0, 1);

		led_on(LED1_PIN);
		if (dwt_starttx(DWT_START_TX_DELAYED) != DWT_SUCCESS) {
			led_off(LED1_PIN); led_off(LED2_PIN);
			dwt_forcetrxoff();
			continue;
		}

		if (waitforsysstatus(&status, NULL, DWT_INT_TXFRS_BIT_MASK, 0U) != 0) {
			led_off(LED1_PIN); led_off(LED2_PIN);
			dwt_forcetrxoff();
			continue;
		}
		dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
		led_off(LED1_PIN);

		/* Response sent successfully → mark payload as sent */
		if (resp_plen > 0) {
			pending_sent = true;
		}

		/* Range not available on responder (SS-TWR computed on initiator) */
		last_range_cm = -1;

		/* Output COBS frame if initiator sent data */
		if (poll_plen > 0) {
			cobs_output(0x02, last_range_cm,
				    &rx_buf[POLL_PAYLOAD_IDX], poll_plen);
		}

		led_off(LED2_PIN);
		led_on(LED3_PIN); k_usleep(200); led_off(LED3_PIN);
	}
}
#endif /* UWB_INITIATOR */
#endif /* !UWB_MESH_NODE_INDEX */

/* ── Six-node scheduled full mesh ──────────────────────────────────────────
 * This is a PELVIS-commanded sequential schedule, not six free-running
 * initiators. The earlier shared-beacon schedule was removed because the
 * boards do not share a clock and a missed beacon could leave a remote in the
 * wrong slot. PELVIS now broadcasts one explicit command before each range
 * transaction. Only the selected pair performs double-sided TWR (POLL, RESP,
 * FINAL); the responder returns one compact result frame to PELVIS, the only
 * range collector. This serializes the pair exchanges but does not depend on
 * all nodes keeping fixed, shared-clock slot boundaries.
 */
#ifdef UWB_MESH_NODE_INDEX

#define MESH_NODE_COUNT       6U
#define MESH_PAIR_COUNT       15U
#define MESH_COORDINATOR      1U /* PELVIS in the canonical project order */
#define MESH_SLOT_UUS         16000U
#define MESH_RESULT_LEN       (HDR_LEN + 5U)
#define MESH_COMMAND_LEN      (HDR_LEN + 2U)
#define MESH_FINAL_LEN        (HDR_LEN + 15U)
/* Default for ordinary builds. The pair-only diagnostic build may override
 * this to test additional delayed-transmit preparation margin. */
#ifndef MESH_FINAL_DELAY_UUS
#define MESH_FINAL_DELAY_UUS  3000U
#endif
#define MESH_REMOTE_COMMAND_GUARD_UUS 1000U
#define MESH_REMOTE_IMU_COMMAND_GUARD_UUS 5000U
#define MESH_COORDINATOR_INIT_GUARD_UUS 1500U
#define MESH_BETWEEN_COMMAND_GUARD_UUS 2000U
#define MESH_COMMAND_TIMEOUT_UUS 50000U
/* PELVIS can observe POLL, RESP and FINAL before the result for a remote
 * pair.  Collect against a time deadline, not a number of observed frames. */
#define MESH_RESULT_COLLECTION_TIMEOUT_UUS 40000U
#define MESH_IMU_SAMPLE_LEN   22U
#define MESH_IMU_BATCH_MAX    5U
/* Remote IMU packets follow a command guard and may contain five samples.
 * A 1.5 ms slot occasionally expired before the transmitter started. */
#define MESH_UPLINK_SLOT_UUS  10000U
#define MESH_UPLINK_LEN       (HDR_LEN + 1U + (MESH_IMU_BATCH_MAX * MESH_IMU_SAMPLE_LEN))
#define FUNC_COMMAND          0x30U
#define FUNC_RESULT           0x31U
#define FUNC_IMU_UPLINK       0x32U
#define FUNC_FINAL            0x22U
#define MESH_CMD_RANGE        0x01U
#define MESH_CMD_IMU          0x02U

#if UWB_MESH_NODE_INDEX >= MESH_NODE_COUNT
#error "UWB_MESH_NODE_INDEX must use the canonical range 0..5"
#endif

#if defined(COMP971_UWB_PAIR_ONLY_INDEX) && \
	COMP971_UWB_PAIR_ONLY_INDEX >= MESH_PAIR_COUNT
#error "COMP971_UWB_PAIR_ONLY_INDEX must use the canonical range 0..14"
#endif

#if defined(COMP971_UWB_TX_STATUS_DIAGNOSTICS) && \
	!defined(COMP971_UWB_PAIR_ONLY_INDEX)
#error "UWB TX status diagnostics must be restricted to a pair-only bench build"
#endif

#ifdef COMP971_UWB_TX_STATUS_DIAGNOSTICS
/* Encoded pre-recovery SYS_STATUS high byte and SYS_STATE_LO low 24 bits. */
#define MESH_TX_FAILURE_DETAIL() dwt_get_last_tx_failure_detail()
#else
#define MESH_TX_FAILURE_DETAIL() (-1)
#endif

struct mesh_pair {
	uint8_t first;
	uint8_t second;
	uint8_t initiator;
};

/* This order is exactly workspace/configs/topology/smplh_wearable_topology_v0.json.
 * Node index: 0=HEAD, 1=PELVIS, 2=LEFT-WRIST, 3=RIGHT-WRIST,
 *             4=LEFT-ANKLE, 5=RIGHT-ANKLE.
 */
static const struct mesh_pair mesh_pairs[MESH_PAIR_COUNT] = {
	{ 0U, 1U, MESH_COORDINATOR }, { 0U, 2U, 0U }, { 0U, 3U, 0U },
	{ 0U, 4U, 0U },                 { 0U, 5U, 0U }, { 1U, 2U, MESH_COORDINATOR },
	{ 1U, 3U, MESH_COORDINATOR },   { 1U, 4U, MESH_COORDINATOR },
	{ 1U, 5U, MESH_COORDINATOR },   { 2U, 3U, 2U }, { 2U, 4U, 2U },
	{ 2U, 5U, 2U },                 { 3U, 4U, 3U }, { 3U, 5U, 3U },
	{ 4U, 5U, 4U },
};

static uint8_t mesh_frame_sn;

#ifdef MESH_DEBUG
static void mesh_debug(uint8_t code, uint8_t index, int32_t detail)
{
	/* Codes in this list describe an actual failed step. Success/range trace
	 * codes stay local so the BLE channel only carries useful fault evidence. */
	switch (code) {
	case 0x20U: case 0x21U: case 0x22U: case 0x23U: case 0x25U:
	case 0x30U: case 0x32U: case 0x33U: case 0x40U: case 0x41U:
	case 0x42U: case 0x43U:
		motion_ble_publish_uwb_diagnostic(index, code, detail);
		break;
	default:
		break;
	}
	uint8_t payload[6] = { code, index, 0U, 0U, 0U, 0U };
	put_le32(&payload[2], (uint32_t)detail);
	cobs_output(0xFEU, -1, payload, sizeof(payload));
}
#else
static void mesh_debug(uint8_t code, uint8_t index, int32_t detail)
{
	/* Event-only BLE logging also works with verbose local MESH_DEBUG disabled. */
	switch (code) {
	case 0x20U: case 0x21U: case 0x22U: case 0x23U: case 0x25U:
	case 0x30U: case 0x32U: case 0x33U: case 0x40U: case 0x41U:
	case 0x42U: case 0x43U:
		motion_ble_publish_uwb_diagnostic(index, code, detail);
		break;
	default:
		break;
	}
}
#endif

struct mesh_collected_imu {
	uint8_t source_node;
	struct icm20948_sample sample;
};

K_MSGQ_DEFINE(mesh_remote_imu_msgq, sizeof(struct icm20948_sample), 16, 4);
K_MSGQ_DEFINE(mesh_collected_imu_msgq, sizeof(struct mesh_collected_imu), 32, 4);

static void mesh_queue_icm_sample(const struct icm20948_sample *sample)
{
	/* A full queue means the radio uplink is not keeping pace.  The collector
	 * will expose that gap through missing device sequence numbers. */
	(void)k_msgq_put(&mesh_remote_imu_msgq, sample, K_NO_WAIT);
}

static uint16_t mesh_address(uint8_t node_index)
{
	return (uint16_t)(node_index + 1U);
}

static uint16_t frame_u16(const uint8_t *frame, uint8_t offset)
{
	return (uint16_t)frame[offset] | ((uint16_t)frame[offset + 1U] << 8);
}

static int32_t frame_i32(const uint8_t *frame, uint8_t offset)
{
	return (int32_t)((uint32_t)frame[offset] |
			 ((uint32_t)frame[offset + 1U] << 8) |
			 ((uint32_t)frame[offset + 2U] << 16) |
			 ((uint32_t)frame[offset + 3U] << 24));
}

static int16_t frame_i16(const uint8_t *frame, uint16_t offset)
{
	return (int16_t)((uint16_t)frame[offset] |
			 ((uint16_t)frame[offset + 1U] << 8));
}

static void mesh_header(uint8_t *frame, uint16_t destination, uint16_t source,
				uint8_t function_code)
{
	frame[0] = 0x41U; frame[1] = 0x88U; frame[2] = mesh_frame_sn++;
	frame[3] = 0xCAU; frame[4] = 0xDEU;
	frame[5] = (uint8_t)destination; frame[6] = (uint8_t)(destination >> 8);
	frame[7] = (uint8_t)source;      frame[8] = (uint8_t)(source >> 8);
	frame[9] = function_code;
}

static bool mesh_frame_is_for(const uint8_t *frame, uint16_t destination,
				      uint16_t source, uint8_t function_code)
{
	return frame[FUNC_CODE_IDX] == function_code &&
		frame_u16(frame, 5U) == destination && frame_u16(frame, 7U) == source;
}

static bool mesh_read_frame(uint8_t *frame, uint16_t frame_capacity,
				    uint32_t timeout_uus, uint16_t *frame_length);

/*
 * The first mesh implementation tried to make every board derive the same
 * schedule from a broadcast synchronisation timestamp.  That is fragile on
 * this target: the boards do not share a clock, and a missed beacon leaves a
 * remote board permanently in the wrong slot.  PELVIS now sends one explicit
 * command before every range or IMU transaction.  The command is broadcast,
 * but the transaction itself is still point-to-point and serialized.
 */
static bool mesh_send_command(uint8_t command, uint8_t index)
{
	uint8_t frame[MESH_COMMAND_LEN];
	uint32_t status;
	mesh_header(frame, 0xFFFFU, mesh_address(MESH_COORDINATOR), FUNC_COMMAND);
	frame[HDR_LEN] = command;
	frame[HDR_LEN + 1U] = index;
	dwt_forcetrxoff();
	dwt_writetxdata(sizeof(frame), frame, 0);
	dwt_writetxfctrl(sizeof(frame) + APP_FCS_LEN, 0, 1);
	if (dwt_starttx(DWT_START_TX_IMMEDIATE) != DWT_SUCCESS ||
		waitforsysstatus(&status, NULL, DWT_INT_TXFRS_BIT_MASK, 0U) != 0) {
		dwt_forcetrxoff();
		return false;
	}
	dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
	mesh_debug(0x10U, index, command);
	return true;
}

static bool mesh_wait_for_command(uint8_t *command, uint8_t *index)
{
	uint8_t frame[MESH_COMMAND_LEN];
	uint16_t frame_length;
	if (!mesh_read_frame(frame, sizeof(frame), MESH_COMMAND_TIMEOUT_UUS,
			&frame_length) || frame_length != MESH_COMMAND_LEN ||
		!mesh_frame_is_for(frame, 0xFFFFU, mesh_address(MESH_COORDINATOR),
			FUNC_COMMAND)) {
		return false;
	}
	*command = frame[HDR_LEN];
	*index = frame[HDR_LEN + 1U];
	return true;
}

static void mesh_remote_command_guard(void)
{
	/* Give all addressed remotes time to leave RX before the transaction. */
	k_busy_wait(MESH_REMOTE_COMMAND_GUARD_UUS);
}

static void mesh_coordinator_initiator_guard(void)
{
	/* The coordinator transmitted the command first; let remotes enter RX. */
	k_busy_wait(MESH_COORDINATOR_INIT_GUARD_UUS);
}

static bool mesh_read_frame(uint8_t *frame, uint16_t frame_capacity,
				    uint32_t timeout_uus, uint16_t *frame_length)
{
	uint32_t status;
	uint8_t rng_bit;
	dwt_setrxtimeout(timeout_uus);
	dwt_rxenable(DWT_START_RX_IMMEDIATE);
	if (waitforsysstatus(&status, NULL,
		DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR,
		0U) != 0 || !(status & DWT_INT_RXFCG_BIT_MASK)) {
		dwt_writesysstatuslo(SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
		return false;
	}
	/* DW3110 reports RXFLEN including the two-byte FCS.  The mesh frame
	 * lengths and buffers below describe only the application bytes, so strip
	 * the FCS from the reported length before validating and reading data. */
	uint16_t wire_length = dwt_getframelength(&rng_bit);
	dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);
	if (wire_length < (HDR_LEN + APP_FCS_LEN) ||
		wire_length - APP_FCS_LEN > frame_capacity) {
		dwt_forcetrxoff();
		return false;
	}
	*frame_length = wire_length - APP_FCS_LEN;
	dwt_readrxdata(frame, *frame_length, 0);
	return true;
}

/* Calculate a double-sided TWR range at the responder.  The FINAL frame
 * carries the initiator's three timestamps; the responder supplies its own
 * POLL-RX, RESP-TX and FINAL-RX timestamps.  This form largely cancels the
 * clock-rate difference between boards. */
static int32_t mesh_calculate_ds_range(uint64_t poll_rx_ts, uint64_t response_tx_ts,
				       uint64_t final_rx_ts, const uint8_t *final)
{
	uint64_t poll_tx_ts = unpack_ts5(&final[HDR_LEN]);
	uint64_t response_rx_ts = unpack_ts5(&final[HDR_LEN + 5U]);
	uint64_t final_tx_ts = unpack_ts5(&final[HDR_LEN + 10U]);
	double round_a = (double)((response_rx_ts - poll_tx_ts) & DWT_TS_MASK);
	double reply_a = (double)((final_tx_ts - response_rx_ts) & DWT_TS_MASK);
	double reply_b = (double)((response_tx_ts - poll_rx_ts) & DWT_TS_MASK);
	double round_b = (double)((final_rx_ts - response_tx_ts) & DWT_TS_MASK);
	double denominator = round_a + reply_a + reply_b + round_b;
	if (denominator <= 0.0) { return -1; }
	double tof_ticks = ((round_a * round_b) - (reply_a * reply_b)) / denominator;
	if (tof_ticks <= 0.0) { return -1; }
	double distance_m = tof_ticks * DWT_TIME_UNITS * 299702547.0;
	return distance_m <= 100.0 ? (int32_t)(distance_m * 100.0) : -1;
}

static void mesh_emit_range(uint8_t pair_index, int32_t range_cm,
			    bool result_timeout)
{
	uint8_t payload[3] = { pair_index, mesh_pairs[pair_index].first,
		mesh_pairs[pair_index].second };
	cobs_output(0x04U, range_cm, payload, sizeof(payload));
	motion_ble_record_uwb_result(range_cm, result_timeout);
	motion_ble_publish_range(pair_index, range_cm);
}

static void mesh_send_result(uint8_t pair_index, int32_t range_cm)
{
	uint8_t result[MESH_RESULT_LEN];
	mesh_header(result, mesh_address(MESH_COORDINATOR),
			mesh_address(UWB_MESH_NODE_INDEX), FUNC_RESULT);
	result[HDR_LEN] = pair_index;
	put_le32(&result[HDR_LEN + 1U], (uint32_t)range_cm);
	dwt_forcetrxoff();
	dwt_writetxdata(sizeof(result), result, 0);
	dwt_writetxfctrl(sizeof(result) + APP_FCS_LEN, 0, 1);
	if (dwt_starttx(DWT_START_TX_IMMEDIATE) == DWT_SUCCESS) {
		uint32_t status;
		(void)waitforsysstatus(&status, NULL, DWT_INT_TXFRS_BIT_MASK, 0U);
		dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
	}
}

static uint8_t mesh_responder_node(const struct mesh_pair *pair)
{
	return pair->initiator == pair->first ? pair->second : pair->first;
}

static void mesh_send_imu_batch(void)
{
	uint8_t frame[MESH_UPLINK_LEN];
	uint8_t count = 0U;
	struct icm20948_sample sample;
	while (count < MESH_IMU_BATCH_MAX &&
		k_msgq_get(&mesh_remote_imu_msgq, &sample, K_NO_WAIT) == 0) {
		encode_icm_sample(&sample,
			&frame[HDR_LEN + 1U + ((uint16_t)count * MESH_IMU_SAMPLE_LEN)]);
		++count;
	}
	if (count == 0U) { return; }
	uint16_t frame_length = HDR_LEN + 1U + ((uint16_t)count * MESH_IMU_SAMPLE_LEN);
	mesh_header(frame, mesh_address(MESH_COORDINATOR),
			mesh_address(UWB_MESH_NODE_INDEX), FUNC_IMU_UPLINK);
	frame[HDR_LEN] = count;
	dwt_forcetrxoff();
	dwt_writetxdata(frame_length, frame, 0);
	dwt_writetxfctrl(frame_length + APP_FCS_LEN, 0, 1);
	if (dwt_starttx(DWT_START_TX_IMMEDIATE) == DWT_SUCCESS) {
		uint32_t status;
		(void)waitforsysstatus(&status, NULL, DWT_INT_TXFRS_BIT_MASK, 0U);
		dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
	}
}

static void mesh_collect_imu(uint8_t source_node)
{
	uint8_t frame[MESH_UPLINK_LEN];
	uint16_t frame_length;
	uint16_t source = mesh_address(source_node);
	/* PELVIS can still hear a late ranging frame as it enters an IMU slot.
	 * Ignore it and wait for the expected uplink, rather than dropping the
	 * complete IMU opportunity after the first unrelated frame. */
	for (uint8_t attempt = 0U; attempt < 3U; ++attempt) {
		if (!mesh_read_frame(frame, sizeof(frame), MESH_UPLINK_SLOT_UUS - 100U,
				&frame_length)) {
			return;
		}
		if (!mesh_frame_is_for(frame, mesh_address(MESH_COORDINATOR), source,
				FUNC_IMU_UPLINK)) {
			continue;
		}
		uint8_t count = frame[HDR_LEN];
		uint16_t expected_length = HDR_LEN + 1U + ((uint16_t)count * MESH_IMU_SAMPLE_LEN);
		if (count == 0U || count > MESH_IMU_BATCH_MAX || frame_length != expected_length) {
			continue;
		}
		for (uint8_t index = 0U; index < count; ++index) {
			uint8_t *encoded = &frame[HDR_LEN + 1U + ((uint16_t)index * MESH_IMU_SAMPLE_LEN)];
			struct mesh_collected_imu collected = {
				.source_node = source_node,
				.sample = {
					.sequence = (uint32_t)frame_i32(encoded, 0U),
					.timestamp_ms = (uint32_t)frame_i32(encoded, 4U),
					.ax = frame_i16(encoded, 8U), .ay = frame_i16(encoded, 10U),
					.az = frame_i16(encoded, 12U), .gx = frame_i16(encoded, 14U),
					.gy = frame_i16(encoded, 16U), .gz = frame_i16(encoded, 18U),
					.temperature = frame_i16(encoded, 20U),
				},
			};
			(void)k_msgq_put(&mesh_collected_imu_msgq, &collected, K_NO_WAIT);
		}
		return;
	}
}

static void mesh_drain_collected_imu(void)
{
	struct mesh_collected_imu collected;
	while (k_msgq_get(&mesh_collected_imu_msgq, &collected, K_NO_WAIT) == 0) {
		emit_icm_sample_from_node(collected.source_node, &collected.sample);
	}
}

static bool mesh_run_initiator_slot(uint8_t pair_index)
{
	const struct mesh_pair *pair = &mesh_pairs[pair_index];
	uint8_t poll[HDR_LEN];
	uint8_t response[RESP_HDR_LEN + APP_FCS_LEN];
	uint8_t final[MESH_FINAL_LEN];
	uint16_t response_length;
	uint32_t status;
	uint16_t own = mesh_address(UWB_MESH_NODE_INDEX);
	uint16_t peer = mesh_address(mesh_responder_node(pair));

	mesh_header(poll, peer, own, FUNC_POLL);
	dwt_forcetrxoff();
	dwt_writetxdata(sizeof(poll), poll, 0);
	dwt_writetxfctrl(sizeof(poll) + APP_FCS_LEN, 0, 1);
	dwt_setrxaftertxdelay(0U);
	dwt_setrxtimeout(MESH_SLOT_UUS - 400U);
	led_on(LED1_PIN);
	if (dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED) != DWT_SUCCESS ||
		waitforsysstatus(&status, NULL, DWT_INT_TXFRS_BIT_MASK, 0U) != 0) {
		led_off(LED1_PIN); dwt_forcetrxoff();
		mesh_debug(0x20U, pair_index, -1);
		return false;
	}
	uint64_t poll_tx_ts = get_tx_ts();
	dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
	led_off(LED1_PIN);

	if (waitforsysstatus(&status, NULL,
		DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR,
		0U) != 0 || !(status & DWT_INT_RXFCG_BIT_MASK)) {
		dwt_forcetrxoff();
		mesh_debug(0x21U, pair_index, (int32_t)status);
		return false;
	}
	uint64_t response_rx_ts = get_rx_ts();
	dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);
	uint8_t rng_bit;
	response_length = dwt_getframelength(&rng_bit);
	if (response_length > sizeof(response) || response_length < RESP_HDR_LEN) {
		dwt_forcetrxoff();
		mesh_debug(0x22U, pair_index, response_length);
		return false;
	}
	dwt_readrxdata(response, response_length, 0);
	if (!mesh_frame_is_for(response, own, peer, FUNC_RESP)) {
		dwt_forcetrxoff();
		mesh_debug(0x23U, pair_index, response[FUNC_CODE_IDX]);
		return false;
	}

	/* Send FINAL after a known delay from the received response.  The responder
	 * will calculate the double-sided range and return it to PELVIS. */
	uint32_t final_tx_time = (uint32_t)((response_rx_ts +
		(uint64_t)MESH_FINAL_DELAY_UUS * UUS_TO_DWT) >> 8);
	uint64_t final_tx_ts =
		(((uint64_t)(final_tx_time & 0xFFFFFFFEUL)) << 8) + UWB_TX_ANT_DLY;
	mesh_header(final, peer, own, FUNC_FINAL);
	pack_ts5(&final[HDR_LEN], poll_tx_ts);
	pack_ts5(&final[HDR_LEN + 5U], response_rx_ts);
	pack_ts5(&final[HDR_LEN + 10U], final_tx_ts);
	dwt_forcetrxoff();
	dwt_setdelayedtrxtime(final_tx_time);
	dwt_writetxdata(sizeof(final), final, 0);
	dwt_writetxfctrl(sizeof(final) + APP_FCS_LEN, 0, 1);
	led_on(LED1_PIN);
	int tx_result = dwt_starttx(DWT_START_TX_DELAYED);
	if (tx_result != DWT_SUCCESS ||
		waitforsysstatus(&status, NULL, DWT_INT_TXFRS_BIT_MASK, 0U) != 0) {
		int32_t tx_failure_detail = tx_result != DWT_SUCCESS ?
			MESH_TX_FAILURE_DETAIL() : -1;
		led_off(LED1_PIN); dwt_forcetrxoff();
		mesh_debug(0x25U, pair_index, tx_failure_detail);
		return false;
	}
	dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
	led_off(LED1_PIN);
	mesh_debug(0x24U, pair_index, 1);
	return true;
}

static void mesh_run_responder_slot(uint8_t pair_index)
{
	const struct mesh_pair *pair = &mesh_pairs[pair_index];
	uint8_t poll[HDR_LEN];
	uint8_t response[RESP_HDR_LEN] = { 0U };
	uint8_t final[MESH_FINAL_LEN];
	uint16_t poll_length;
	uint16_t final_length;
	uint32_t status;
	uint16_t own = mesh_address(UWB_MESH_NODE_INDEX);
	uint16_t initiator = mesh_address(pair->initiator);

	if (!mesh_read_frame(poll, sizeof(poll), MESH_SLOT_UUS - 300U, &poll_length) ||
		!mesh_frame_is_for(poll, own, initiator, FUNC_POLL)) {
		mesh_debug(0x30U, pair_index, -1);
		return;
	}
	uint64_t poll_rx_ts = get_rx_ts();
	mesh_header(response, initiator, own, FUNC_RESP);
	uint32_t response_tx_time = (uint32_t)((poll_rx_ts +
		(uint64_t)POLL_RX_TO_RESP_DLY_UUS * UUS_TO_DWT) >> 8);
	dwt_setdelayedtrxtime(response_tx_time);
	response[RESP_PLEN_IDX] = 0U;
	dwt_forcetrxoff();
	dwt_writetxdata(sizeof(response), response, 0);
	dwt_writetxfctrl(sizeof(response) + APP_FCS_LEN, 0, 1);
	led_on(LED1_PIN);
	int tx_result = dwt_starttx(DWT_START_TX_DELAYED);
	if (tx_result == DWT_SUCCESS) {
		(void)waitforsysstatus(&status, NULL, DWT_INT_TXFRS_BIT_MASK, 0U);
		dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
	} else {
		mesh_debug(0x32U, pair_index, MESH_TX_FAILURE_DETAIL());
		led_off(LED1_PIN);
		return;
	}
	led_off(LED1_PIN);
	uint64_t response_tx_ts = get_tx_ts();
	if (!mesh_read_frame(final, sizeof(final), MESH_SLOT_UUS, &final_length) ||
		final_length != MESH_FINAL_LEN ||
		!mesh_frame_is_for(final, own, initiator, FUNC_FINAL)) {
		mesh_debug(0x33U, pair_index, -1);
		mesh_send_result(pair_index, -1);
		return;
	}
	uint64_t final_rx_ts = get_rx_ts();
	int32_t range_cm = mesh_calculate_ds_range(poll_rx_ts, response_tx_ts,
		final_rx_ts, final);
	mesh_debug(range_cm < 0 ? 0x43U : 0x31U, pair_index, range_cm);
	mesh_send_result(pair_index, range_cm);
}

static int32_t mesh_collect_result(uint8_t pair_index, uint8_t responder_node,
				   bool *result_received)
{
	/* This listener can hear the remote transaction's RESP and FINAL frames as
	 * well as the responder's final RESULT frame, so it must accommodate the
	 * larger FINAL frame. */
	uint8_t frame[MESH_FINAL_LEN];
	uint16_t frame_length;
	uint16_t source = mesh_address(responder_node);
	uint64_t deadline_cycles;

	*result_received = false;
	/* The coordinator is not a member of a remote-initiated range, but its
	 * receiver still hears that range's POLL and RESP frames.  Discard those
	 * frames and keep listening for the responder's final result addressed to
	 * PELVIS.  A DS-TWR transaction has POLL, RESP, FINAL, then RESULT. */
	deadline_cycles = k_cycle_get_64() +
		k_us_to_cyc_ceil64(MESH_RESULT_COLLECTION_TIMEOUT_UUS);
	while (k_cycle_get_64() < deadline_cycles) {
		uint64_t remaining_cycles = deadline_cycles - k_cycle_get_64();
		uint64_t remaining_uus = k_cyc_to_us_floor64(remaining_cycles);

		if (remaining_uus == 0U ||
			!mesh_read_frame(frame, sizeof(frame), (uint32_t)remaining_uus,
				&frame_length)) {
			break;
		}
		if (frame_length == MESH_RESULT_LEN &&
			mesh_frame_is_for(frame, mesh_address(MESH_COORDINATOR), source,
				FUNC_RESULT) && frame[HDR_LEN] == pair_index) {
			*result_received = true;
			int32_t range_cm = frame_i32(frame, HDR_LEN + 1U);
			if (range_cm < 0) {
				mesh_debug(0x42U, pair_index, range_cm);
			}
			return range_cm;
		}
	}
	mesh_debug(0x40U, pair_index, (int32_t)MESH_RESULT_COLLECTION_TIMEOUT_UUS);
	return -1;
}

static void run_mesh(void)
{
	/* The standalone responder explicitly disables the DW3110 preamble
	 * detection timeout.  The mesh receiver also waits for frames in several
	 * short slots; leaving the reset default here can make those receives time
	 * out before a valid preamble is accepted. */
	dwt_setpreambledetecttimeout(0U);
	while (true) {
#ifdef COMP971_FACTORY_OTP_REPORT
		maybe_publish_factory_otp();
#endif
		if (UWB_MESH_NODE_INDEX == MESH_COORDINATOR) {
			for (uint8_t pair_index = 0U; pair_index < MESH_PAIR_COUNT; ++pair_index) {
				/* Calibration builds can exercise one selected link without
				 * generating failures/diagnostics for the other fourteen links. */
#ifdef COMP971_UWB_PAIR_ONLY_INDEX
				if (pair_index != COMP971_UWB_PAIR_ONLY_INDEX) {
					continue;
				}
#endif
				const struct mesh_pair *pair = &mesh_pairs[pair_index];
				bool result_received = false;
				/* Leave the DW3110 in idle state before beginning the next
				 * command/slot.  Without this short gap the coordinator can try
				 * to transmit its next command while the preceding RX/TX state is
				 * still being released.  That silently skips remote-initiated
				 * pairs (for example HEAD-to-LEFT-WRIST). */
				k_busy_wait(MESH_BETWEEN_COMMAND_GUARD_UUS);
				if (!mesh_send_command(MESH_CMD_RANGE, pair_index)) {
					mesh_debug(0x41U, pair_index, -1);
					mesh_emit_range(pair_index, -1, false);
					continue;
				}
				if (pair->initiator == MESH_COORDINATOR) {
					mesh_coordinator_initiator_guard();
					if (mesh_run_initiator_slot(pair_index)) {
					int32_t range_cm = mesh_collect_result(pair_index,
						mesh_responder_node(pair), &result_received);
					mesh_emit_range(pair_index, range_cm, !result_received);
					} else {
						mesh_emit_range(pair_index, -1, false);
					}
				} else {
					int32_t range_cm = mesh_collect_result(pair_index,
						mesh_responder_node(pair), &result_received);
					mesh_emit_range(pair_index, range_cm, !result_received);
				}
			}
			#if !defined(COMP971_DIRECT_BLE_IMU) && \
				!defined(COMP971_UWB_PAIR_ONLY_INDEX)
			for (uint8_t source_node = 0U; source_node < MESH_NODE_COUNT; ++source_node) {
				if (source_node == MESH_COORDINATOR) { continue; }
				k_busy_wait(MESH_BETWEEN_COMMAND_GUARD_UUS);
				if (mesh_send_command(MESH_CMD_IMU, source_node)) {
					mesh_collect_imu(source_node);
				}
			}
			mesh_drain_collected_imu();
			#endif
		} else {
			/* Accept whichever valid command arrives next.  Requiring the
			 * exact next pair number made a missed startup command leave a
			 * remote permanently out of step with PELVIS. */
			uint8_t command;
			uint8_t command_index;
			if (!mesh_wait_for_command(&command, &command_index)) {
				continue;
			}
			if (command == MESH_CMD_RANGE &&
				command_index < MESH_PAIR_COUNT) {
				mesh_remote_command_guard();
				const struct mesh_pair *remote_pair =
					&mesh_pairs[command_index];
				if (remote_pair->initiator == UWB_MESH_NODE_INDEX) {
					mesh_run_initiator_slot(command_index);
				} else if (remote_pair->first == UWB_MESH_NODE_INDEX ||
					   remote_pair->second == UWB_MESH_NODE_INDEX) {
					mesh_run_responder_slot(command_index);
				}
			} else if (command == MESH_CMD_IMU &&
				command_index < MESH_NODE_COUNT &&
				command_index != MESH_COORDINATOR &&
				command_index == UWB_MESH_NODE_INDEX) {
				/* Give PELVIS a deterministic margin to enter RX before this
				 * larger one-way IMU packet is transmitted. */
				k_busy_wait(MESH_REMOTE_IMU_COMMAND_GUARD_UUS);
				mesh_send_imu_batch();
			}
		}
	}
}
#endif /* UWB_MESH_NODE_INDEX */

/* ── main ───────────────────────────────────────────────────────────────────*/
void main(void)
{
	/* GPIO init */
	gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
	gpio1_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	if (!device_is_ready(gpio0_dev) || !device_is_ready(gpio1_dev) ||
	    !gpio_is_ready_dt(&uwb_cs)) { return; }

	gpio_pin_configure(gpio0_dev, RST_PIN,    GPIO_OUTPUT_HIGH);
	gpio_pin_configure(gpio1_dev, WAKEUP_PIN, GPIO_OUTPUT_LOW);
	gpio_pin_configure(gpio1_dev, IRQ_PIN,    GPIO_INPUT);
	if (gpio_pin_configure_dt(&uwb_cs, GPIO_OUTPUT_INACTIVE) != 0) {
		return;
	}

	gpio_pin_configure(gpio0_dev, LED1_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
	gpio_pin_configure(gpio0_dev, LED2_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
	gpio_pin_configure(gpio0_dev, LED3_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);
	gpio_pin_configure(gpio0_dev, LED4_PIN, GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);

	/* Boot flash */
	led_on(LED1_PIN); led_on(LED2_PIN); led_on(LED3_PIN); led_on(LED4_PIN);
	k_msleep(300);
	led_off(LED1_PIN); led_off(LED2_PIN); led_off(LED3_PIN); led_off(LED4_PIN);

	/* SPI init */
	spi_dev = DEVICE_DT_GET(SPI_NODE);
	if (!device_is_ready(spi_dev)) { return; }

	/* UART init */
	uart_dev = DEVICE_DT_GET(UART_NODE);
	if (!device_is_ready(uart_dev)) { return; }
	uart_irq_callback_set(uart_dev, uart_isr);
	uart_irq_rx_enable(uart_dev);

	/* DW3110: reset → wait → probe → init → configure */
	/* Match the known-good standalone probe: wake the DW3110 with a short
	 * chip-select assertion before applying the hard reset. */
	(void)gpio_pin_set_dt(&uwb_cs, 1);
	k_msleep(1);
	(void)gpio_pin_set_dt(&uwb_cs, 0);
	gpio_pin_configure(gpio0_dev, RST_PIN, GPIO_OUTPUT_LOW);
	k_msleep(2);
	gpio_pin_configure(gpio0_dev, RST_PIN, GPIO_INPUT);
	k_msleep(50);

	static struct dwt_spi_s dwt_spi = {
		.readfromspi       = uwb_readfromspi,
		.writetospi        = uwb_writetospi,
		.writetospiwithcrc = uwb_writetospi_crc,
		.setfastrate       = uwb_spi_setfastrate,
		.setslowrate       = uwb_spi_setslowrate,
	};
	static struct dwchip_s dw_chip;
	static struct dwt_probe_s probe = {
		.dw = &dw_chip, .spi = &dwt_spi,
		.wakeup_device_with_io = uwb_wakeup_fn,
	};
	extern const struct dwt_driver_s dw3000_driver;
	static const struct dwt_driver_s *driver_list[] = { &dw3000_driver };
	probe.driver_list   = (struct dwt_driver_s **)driver_list;
	probe.dw_driver_num = 1;

	if (dwt_probe(&probe) != DWT_SUCCESS) {
		led_on(LED4_PIN);
		return;
	}

	{ int t = 0; while (!dwt_checkidlerc()) { k_msleep(1); if (++t > 100) { led_on(LED4_PIN); return; } } }

	if (dwt_initialise(DWT_DW_INIT) != DWT_SUCCESS) { led_on(LED4_PIN); return; }
	if (dwt_configure(&uwb_config) != DWT_SUCCESS) { led_on(LED4_PIN); return; }
	dwt_configuretxrf(&uwb_txconfig);

#ifdef COMP971_FACTORY_OTP_REPORT
	dwt_otpread(COMP971_FACTORY_OTP_CH5_PRF64_ADDR,
		&factory_otp_ch5_prf64, 1U);
#endif

	dwt_setdwstate(DWT_DW_IDLE);
	k_msleep(1);

	dwt_setrxantennadelay(UWB_RX_ANT_DLY);
	dwt_settxantennadelay(UWB_TX_ANT_DLY);
	dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

	if (motion_ble_start() != 0) {
		led_on(LED4_PIN);
	}

	/* The ICM reader is independent of the UWB timing loop.  A failure is
	 * signalled by D12, while UWB ranging continues to run. */
	if (icm20948_start(emit_icm_sample) != 0) {
		led_on(LED4_PIN);
	}

	/* Identify the connected board over serial at startup. */
	cobs_output(0xFF, -1, (const uint8_t *)UWB_NODE_NAME,
		    (uint8_t)(sizeof(UWB_NODE_NAME) - 1U));

#ifdef UWB_MESH_NODE_INDEX
	run_mesh();
#elif defined(UWB_INITIATOR)
	run_initiator();
#else
	run_responder();
#endif
}
