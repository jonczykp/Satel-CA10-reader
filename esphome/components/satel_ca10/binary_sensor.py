import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from . import CONF_SATEL_CA10_ID, SatelCA10Hub

CONF_BIT = "bit"
CONF_LED = "led"

# Frame bits of the Satel CA10 keypad (KLED) signal
LEDS = {
    "led1": 31,
    "led2": 29,
    "led3": 27,
    "led4": 25,
    "led5": 15,
    "led6": 13,
    "led7": 11,
    "led8": 9,
    "warning": 19,
    "zone1": 21,
    "zone2": 17,
    "alarm": 23,
}

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema().extend(
        {
            cv.GenerateID(CONF_SATEL_CA10_ID): cv.use_id(SatelCA10Hub),
            cv.Optional(CONF_LED): cv.enum(LEDS, lower=True),
            cv.Optional(CONF_BIT): cv.int_range(min=0, max=31),
        }
    ),
    cv.has_exactly_one_key(CONF_LED, CONF_BIT),
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_SATEL_CA10_ID])
    bit = config[CONF_LED] if CONF_LED in config else config[CONF_BIT]
    cg.add(hub.register_bit_sensor(bit, var))
