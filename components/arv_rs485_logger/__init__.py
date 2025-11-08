from esphome import codegen as cg, config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@exp0devel"]

arv_ns = cg.esphome_ns.namespace("arv_rs485_logger")
ArvRs485Logger = arv_ns.class_("ArvRs485Logger", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(ArvRs485Logger),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
