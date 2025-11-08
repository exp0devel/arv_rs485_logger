import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.components import uart

DEPENDENCIES = ['uart']

arv_rs485_logger_ns = cg.esphome_ns.namespace('arv_rs485_logger')
ArvRs485Logger = arv_rs485_logger_ns.class_('ArvRs485Logger', cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cg.Schema({
    cg.GenerateID(): cg.declare_id(ArvRs485Logger),
}).extend(cg.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)