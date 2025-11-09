from esphome import codegen as cg, config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@exp0devel"]

arv_ns = cg.esphome_ns.namespace("arv_rs485_logger")
ArvRs485Logger = arv_ns.class_("ArvRs485Logger", cg.Component, uart.UARTDevice)

CONF_MIN_GAP_US = "min_gap_us"
CONF_MAX_BURST_LEN = "max_burst_len"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ArvRs485Logger),
            cv.Optional(CONF_MIN_GAP_US, default=5000): cv.positive_int,
            cv.Optional(CONF_MAX_BURST_LEN, default=256): cv.int_range(min=32, max=2048),
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