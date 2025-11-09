from esphome import codegen as cg, config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@exp0devel"]

arv_ns = cg.esphome_ns.namespace("arv_rs485_logger")
ArvRs485Logger = arv_ns.class_("ArvRs485Logger", cg.Component, uart.UARTDevice)

CONF_MIN_GAP_US = "min_gap_us"
CONF_MAX_BURST_LEN = "max_burst_len"
CONF_MIN_LENGTH = "min_length"
CONF_DEDUPE_MS = "dedupe_ms"
CONF_IDLE_FILTER = "idle_filter"
CONF_IDLE_BYTES = "idle_bytes"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ArvRs485Logger),
            cv.Optional(CONF_MIN_GAP_US, default=5000): cv.positive_int,
            cv.Optional(CONF_MAX_BURST_LEN, default=512): cv.int_range(min=32, max=4096),
            cv.Optional(CONF_MIN_LENGTH, default=8): cv.int_range(min=0, max=512),
            cv.Optional(CONF_DEDUPE_MS, default=200): cv.positive_int,
            cv.Optional(CONF_IDLE_FILTER, default=True): cv.boolean,
            cv.Optional(CONF_IDLE_BYTES, default=[0x06,0x66,0x98,0xFE,0xE0,0xE6,0x7E,0x78,0x80,0x86,0x00,0xF8,0x1E,0x18,0x60,0x9E]): [cv.int_range(min=0, max=255)],
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    cg.add(var.set_min_gap_us(config[CONF_MIN_GAP_US]))
    cg.add(var.set_max_burst_len(config[CONF_MAX_BURST_LEN]))
    cg.add(var.set_min_length(config[CONF_MIN_LENGTH]))
    cg.add(var.set_dedupe_ms(config[CONF_DEDUPE_MS]))
    cg.add(var.set_idle_filter(config[CONF_IDLE_FILTER]))
    cg.add(var.set_idle_bytes(config[CONF_IDLE_BYTES]))
