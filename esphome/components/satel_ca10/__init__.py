import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_CLK_PIN, CONF_DATA_PIN

CODEOWNERS = ["@jonczykp"]
AUTO_LOAD = ["binary_sensor"]

CONF_SATEL_CA10_ID = "satel_ca10_id"

satel_ca10_ns = cg.esphome_ns.namespace("satel_ca10")
SatelCA10Hub = satel_ca10_ns.class_("SatelCA10Hub", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SatelCA10Hub),
        cv.Required(CONF_CLK_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_DATA_PIN): pins.internal_gpio_input_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk))
    data = await cg.gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_data_pin(data))
